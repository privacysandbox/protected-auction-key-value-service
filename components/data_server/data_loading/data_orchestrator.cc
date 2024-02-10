// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/data_server/data_loading/data_orchestrator.h"

#include <algorithm>
#include <deque>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "components/data/file_group/file_group_search_utils.h"
#include "components/errors/retry.h"
#include "public/constants.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/filename_utils.h"
#include "public/data_loading/records_utils.h"
#include "public/sharding/sharding_function.h"
#include "src/cpp/telemetry/tracing.h"
#include "src/cpp/util/status_macro/status_macros.h"

namespace kv_server {
namespace {
// TODO(b/321716836): use the default prefix to apply cache updates for realtime
//  for now. This needs to be removed after we are done with directory support
//  for file updates.
constexpr std::string_view kDefaultPrefixForRealTimeUpdates = "";

using privacy_sandbox::server_common::TraceWithStatusOr;

// Holds an input stream pointing to a blob of Riegeli records.
class BlobRecordStream : public RecordStream {
 public:
  explicit BlobRecordStream(std::unique_ptr<BlobReader> blob_reader)
      : blob_reader_(std::move(blob_reader)) {}
  std::istream& Stream() { return blob_reader_->Stream(); }

 private:
  std::unique_ptr<BlobReader> blob_reader_;
};

void LogDataLoadingMetrics(const DataLoadingStats& data_loading_stats) {
  LogIfError(
      KVServerContextMap()
          ->SafeMetric()
          .LogUpDownCounter<kTotalRowsUpdatedInDataLoading>(
              static_cast<double>(data_loading_stats.total_updated_records)));
  LogIfError(
      KVServerContextMap()
          ->SafeMetric()
          .LogUpDownCounter<kTotalRowsDeletedInDataLoading>(
              static_cast<double>(data_loading_stats.total_deleted_records)));
  LogIfError(
      KVServerContextMap()
          ->SafeMetric()
          .LogUpDownCounter<kTotalRowsDroppedInDataLoading>(
              static_cast<double>(data_loading_stats.total_dropped_records)));
}

absl::Status ApplyUpdateMutation(std::string_view prefix,
                                 const KeyValueMutationRecord& record,
                                 Cache& cache) {
  if (record.value_type() == Value::StringValue) {
    cache.UpdateKeyValue(record.key()->string_view(),
                         GetRecordValue<std::string_view>(record),
                         record.logical_commit_time(), prefix);
    return absl::OkStatus();
  }
  if (record.value_type() == Value::StringSet) {
    auto values = GetRecordValue<std::vector<std::string_view>>(record);
    cache.UpdateKeyValueSet(record.key()->string_view(), absl::MakeSpan(values),
                            record.logical_commit_time(), prefix);
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Record with key: ", record.key()->string_view(),
                   " has unsupported value type: ", record.value_type()));
}

absl::Status ApplyDeleteMutation(std::string_view prefix,
                                 const KeyValueMutationRecord& record,
                                 Cache& cache) {
  if (record.value_type() == Value::StringValue) {
    cache.DeleteKey(record.key()->string_view(), record.logical_commit_time(),
                    prefix);
    return absl::OkStatus();
  }
  if (record.value_type() == Value::StringSet) {
    auto values = GetRecordValue<std::vector<std::string_view>>(record);
    cache.DeleteValuesInSet(record.key()->string_view(), absl::MakeSpan(values),
                            record.logical_commit_time(), prefix);
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Record with key: ", record.key()->string_view(),
                   " has unsupported value type: ", record.value_type()));
}

bool ShouldProcessRecord(const KeyValueMutationRecord& record,
                         int64_t num_shards, int64_t server_shard_num,
                         const KeySharder& key_sharder,
                         DataLoadingStats& data_loading_stats) {
  if (num_shards <= 1) {
    return true;
  }
  auto sharding_result =
      key_sharder.GetShardNumForKey(record.key()->string_view(), num_shards);
  if (sharding_result.shard_num == server_shard_num) {
    return true;
  }
  data_loading_stats.total_dropped_records++;
  LOG_EVERY_N(ERROR, 100000) << absl::StrFormat(
      "Data does not belong to this shard replica. Key: %s, Sharding key (if "
      "regex matched): %s, Actual "
      "shard id: %d, Server's shard id: %d.",
      record.key()->string_view(), sharding_result.sharding_key,
      sharding_result.shard_num, server_shard_num);
  return false;
}

absl::Status ApplyKeyValueMutationToCache(
    std::string_view prefix, const KeyValueMutationRecord& record, Cache& cache,
    int64_t& max_timestamp, DataLoadingStats& data_loading_stats) {
  switch (record.mutation_type()) {
    case KeyValueMutationType::Update: {
      if (auto status = ApplyUpdateMutation(prefix, record, cache);
          !status.ok()) {
        return status;
      }
      max_timestamp = std::max(max_timestamp, record.logical_commit_time());
      data_loading_stats.total_updated_records++;
      break;
    }
    case KeyValueMutationType::Delete: {
      if (auto status = ApplyDeleteMutation(prefix, record, cache);
          !status.ok()) {
        return status;
      }
      max_timestamp = std::max(max_timestamp, record.logical_commit_time());
      data_loading_stats.total_deleted_records++;
      break;
    }
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid mutation type: ",
                       EnumNameKeyValueMutationType(record.mutation_type())));
  }
  return absl::OkStatus();
}

absl::StatusOr<DataLoadingStats> LoadCacheWithData(
    std::string_view prefix, StreamRecordReader& record_reader, Cache& cache,
    int64_t& max_timestamp, const int32_t server_shard_num,
    const int32_t num_shards, UdfClient& udf_client,
    const KeySharder& key_sharder) {
  DataLoadingStats data_loading_stats;
  const auto process_data_record_fn =
      [prefix, &cache, &max_timestamp, &data_loading_stats, server_shard_num,
       num_shards, &udf_client, &key_sharder](const DataRecord& data_record) {
        if (data_record.record_type() == Record::KeyValueMutationRecord) {
          const auto* record = data_record.record_as_KeyValueMutationRecord();
          if (!ShouldProcessRecord(*record, num_shards, server_shard_num,
                                   key_sharder, data_loading_stats)) {
            // NOTE: currently upstream logic retries on non-ok status
            // this will get us in a loop
            return absl::OkStatus();
          }
          return ApplyKeyValueMutationToCache(
              prefix, *record, cache, max_timestamp, data_loading_stats);
        } else if (data_record.record_type() ==
                   Record::UserDefinedFunctionsConfig) {
          const auto* udf_config =
              data_record.record_as_UserDefinedFunctionsConfig();
          VLOG(3) << "Setting UDF code snippet for version: "
                  << udf_config->version();
          return udf_client.SetCodeObject(CodeConfig{
              .js = udf_config->code_snippet()->str(),
              .udf_handler_name = udf_config->handler_name()->str(),
              .logical_commit_time = udf_config->logical_commit_time(),
              .version = udf_config->version()});
        }
        LOG(ERROR) << "Received unsupported record ";
        return absl::InvalidArgumentError("Record type not supported.");
      };

  auto status = record_reader.ReadStreamRecords(
      [&process_data_record_fn](std::string_view raw) {
        return DeserializeDataRecord(raw, process_data_record_fn);
      });
  if (!status.ok()) {
    return status;
  }
  LogDataLoadingMetrics(data_loading_stats);
  return data_loading_stats;
}

// Reads the file from `location` and updates the cache based on the delta read.
absl::StatusOr<DataLoadingStats> LoadCacheWithDataFromFile(
    const BlobStorageClient::DataLocation& location,
    const DataOrchestrator::Options& options) {
  LOG(INFO) << "Loading " << location;
  int64_t max_timestamp = 0;
  auto& cache = options.cache;
  auto record_reader =
      options.delta_stream_reader_factory.CreateConcurrentReader(
          /*stream_factory=*/[&location, &options]() {
            return std::make_unique<BlobRecordStream>(
                options.blob_client.GetBlobReader(location));
          });
  auto metadata = record_reader->GetKVFileMetadata();
  if (!metadata.ok()) {
    return metadata.status();
  }
  if (metadata->has_sharding_metadata() &&
      metadata->sharding_metadata().shard_num() != options.shard_num) {
    LOG(INFO) << "Blob " << location << " belongs to shard num "
              << metadata->sharding_metadata().shard_num()
              << " but server shard num is " << options.shard_num
              << " Skipping it.";
    return DataLoadingStats{
        .total_updated_records = 0,
        .total_deleted_records = 0,
        .total_dropped_records = 0,
    };
  }
  auto status = LoadCacheWithData(
      location.prefix, *record_reader, cache, max_timestamp, options.shard_num,
      options.num_shards, options.udf_client, options.key_sharder);
  if (status.ok()) {
    cache.RemoveDeletedKeys(max_timestamp, location.prefix);
  }
  return status;
}
absl::StatusOr<DataLoadingStats> TraceLoadCacheWithDataFromFile(
    BlobStorageClient::DataLocation location,
    const DataOrchestrator::Options& options) {
  return TraceWithStatusOr(
      [location, &options] {
        return LoadCacheWithDataFromFile(std::move(location), options);
      },
      "LoadCacheWithDataFromFile",
      {{"bucket", std::move(location.bucket)},
       {"key", std::move(location.key)}});
}

class DataOrchestratorImpl : public DataOrchestrator {
 public:
  // `last_basename` is the last file seen during init. The cache is up to
  // date until this file.
  DataOrchestratorImpl(Options options, std::string last_basename)
      : options_(std::move(options)),
        last_basename_of_init_(std::move(last_basename)) {}

  ~DataOrchestratorImpl() override {
    if (!data_loader_thread_) return;
    {
      absl::MutexLock l(&mu_);
      stop_ = true;
    }
    LOG(INFO) << "Sent cancel signal to data loader thread";
    LOG(INFO) << "Stopping loading new data from " << options_.data_bucket;
    if (options_.delta_notifier.IsRunning()) {
      if (const auto s = options_.delta_notifier.Stop(); !s.ok()) {
        LOG(ERROR) << "Failed to stop notify: " << s;
      }
    }
    LOG(INFO) << "Delta notifier stopped";
    data_loader_thread_->join();
    LOG(INFO) << "Stopped loading new data";
  }

  static absl::StatusOr<std::string> Init(Options& options) {
    auto ending_delta_file = LoadSnapshotFiles(options);
    if (!ending_delta_file.ok()) {
      return ending_delta_file.status();
    }
    auto maybe_filenames = options.blob_client.ListBlobs(
        {.bucket = options.data_bucket},
        {.prefix = std::string(FilePrefix<FileType::DELTA>()),
         .start_after = *ending_delta_file});
    if (!maybe_filenames.ok()) {
      return maybe_filenames.status();
    }
    LOG(INFO) << "Initializing cache with " << maybe_filenames->size()
              << " delta files from " << options.data_bucket;

    std::string last_basename = std::move(*ending_delta_file);
    for (auto&& basename : std::move(*maybe_filenames)) {
      if (!IsDeltaFilename(basename)) {
        LOG(WARNING) << "Saw a file " << basename
                     << " not in delta file format. Skipping it.";
        continue;
      }
      last_basename = basename;
      if (const auto s = TraceLoadCacheWithDataFromFile(
              {.bucket = options.data_bucket, .key = std::move(basename)},
              options);
          !s.ok()) {
        return s.status();
      }
      LOG(INFO) << "Done loading " << last_basename;
    }
    return last_basename;
  }

  absl::Status Start() override {
    if (data_loader_thread_) {
      return absl::OkStatus();
    }
    LOG(INFO) << "Transitioning to state ContinuouslyLoadNewData";
    absl::Status status = options_.delta_notifier.Start(
        options_.change_notifier, {.bucket = options_.data_bucket},
        {std::make_pair("", last_basename_of_init_)},
        absl::bind_front(&DataOrchestratorImpl::EnqueueNewFilesToProcess,
                         this));
    if (!status.ok()) {
      return status;
    }
    data_loader_thread_ = std::make_unique<std::thread>(
        absl::bind_front(&DataOrchestratorImpl::ProcessNewFiles, this));

    return options_.realtime_thread_pool_manager.Start(
        [this, &cache = options_.cache,
         &delta_stream_reader_factory = options_.delta_stream_reader_factory](
            const std::string& message_body) {
          return LoadCacheWithHighPriorityUpdates(
              kDefaultPrefixForRealTimeUpdates, delta_stream_reader_factory,
              message_body, cache);
        });
  }

 private:
  bool HasNewEventToProcess() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    return !unprocessed_basenames_.empty() || stop_ == true;
  }
  // Reads new files, if any, from the `unprocessed_basenames_` queue and
  // processes them one by one.
  //
  // On failure, puts the file back to the end of the queue and retry at a
  // later point.
  void ProcessNewFiles() {
    LOG(INFO) << "Thread for new file processing started";
    absl::Condition has_new_event(this,
                                  &DataOrchestratorImpl::HasNewEventToProcess);
    while (true) {
      std::string basename;
      {
        absl::MutexLock l(&mu_, has_new_event);
        if (stop_) {
          LOG(INFO) << "Thread for new file processing stopped";
          return;
        }
        basename = std::move(unprocessed_basenames_.back());
        unprocessed_basenames_.pop_back();
      }
      LOG(INFO) << "Loading " << basename;
      if (!IsDeltaFilename(basename)) {
        LOG(WARNING) << "Received file with invalid name: " << basename;
        continue;
      }
      RetryUntilOk(
          [this, &basename] {
            // TODO: distinguish status. Some can be retried while others
            // are fatal.
            return TraceLoadCacheWithDataFromFile(
                {.bucket = options_.data_bucket, .key = basename}, options_);
          },
          "LoadNewFile", LogStatusSafeMetricsFn<kLoadNewFilesStatus>());
    }
  }

  // Puts newly found file names into `unprocessed_basenames_`.
  void EnqueueNewFilesToProcess(const std::string& basename) {
    absl::MutexLock l(&mu_);
    unprocessed_basenames_.push_front(basename);
    LOG(INFO) << "queued " << basename << " for loading";
    // TODO: block if the queue is too large: consumption is too slow.
  }

  // Loads snapshot files if there are any.
  // Returns the latest delta file to be included in a snapshot.
  static absl::StatusOr<std::string> LoadSnapshotFiles(const Options& options) {
    LOG(INFO) << "Initializing cache with snapshot file(s) from: "
              << options.data_bucket;
    std::string ending_delta_file;
    PS_ASSIGN_OR_RETURN(
        auto snapshot_group,
        FindMostRecentFileGroup(
            BlobStorageClient::DataLocation{.bucket = options.data_bucket},
            FileGroupFilter{.file_type = FileType::SNAPSHOT,
                            .status = FileGroup::FileStatus::kComplete},
            options.blob_client));
    if (!snapshot_group.has_value()) {
      return ending_delta_file;
    }
    for (const auto& snapshot : snapshot_group->Filenames()) {
      BlobStorageClient::DataLocation location{.bucket = options.data_bucket,
                                               .key = snapshot};
      auto record_reader =
          options.delta_stream_reader_factory.CreateConcurrentReader(
              /*stream_factory=*/[&location, &options]() {
                return std::make_unique<BlobRecordStream>(
                    options.blob_client.GetBlobReader(location));
              });
      PS_ASSIGN_OR_RETURN(auto metadata, record_reader->GetKVFileMetadata());
      if (metadata.has_sharding_metadata() &&
          metadata.sharding_metadata().shard_num() != options.shard_num) {
        LOG(INFO) << "Snapshot " << location << " belongs to shard num "
                  << metadata.sharding_metadata().shard_num()
                  << " but server shard num is " << options.shard_num
                  << ". Skipping it.";
        continue;
      }
      LOG(INFO) << "Loading snapshot file: " << location;
      PS_ASSIGN_OR_RETURN(auto stats,
                          TraceLoadCacheWithDataFromFile(location, options));
      if (metadata.snapshot().ending_delta_file() > ending_delta_file) {
        ending_delta_file = metadata.snapshot().ending_delta_file();
      }
      LOG(INFO) << "Done loading snapshot file: " << location;
    }
    return ending_delta_file;
  }

  absl::StatusOr<DataLoadingStats> LoadCacheWithHighPriorityUpdates(
      std::string_view prefix,
      StreamRecordReaderFactory& delta_stream_reader_factory,
      const std::string& record_string, Cache& cache) {
    std::istringstream is(record_string);
    int64_t max_timestamp = 0;
    auto record_reader = delta_stream_reader_factory.CreateReader(is);
    return LoadCacheWithData(prefix, *record_reader, cache, max_timestamp,
                             options_.shard_num, options_.num_shards,
                             options_.udf_client, options_.key_sharder);
  }

  const Options options_;
  absl::Mutex mu_;
  std::deque<std::string> unprocessed_basenames_ ABSL_GUARDED_BY(mu_);
  std::unique_ptr<std::thread> data_loader_thread_;
  bool stop_ ABSL_GUARDED_BY(mu_) = false;
  // last basename of file in initialization.
  const std::string last_basename_of_init_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<DataOrchestrator>> DataOrchestrator::TryCreate(
    Options options) {
  const auto maybe_last_basename = DataOrchestratorImpl::Init(options);
  if (!maybe_last_basename.ok()) {
    return maybe_last_basename.status();
  }
  auto orchestrator = std::make_unique<DataOrchestratorImpl>(
      std::move(options), std::move(maybe_last_basename.value()));
  return orchestrator;
}
}  // namespace kv_server
