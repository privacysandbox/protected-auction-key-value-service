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

#include "absl/container/flat_hash_map.h"
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
#include "src/telemetry/tracing.h"
#include "src/util/status_macro/status_macros.h"

namespace kv_server {
namespace {
// TODO(b/321716836): use the default prefix to apply cache updates for realtime
//  for now. This needs to be removed after we are done with directory support
//  for file updates.
constexpr std::string_view kDefaultPrefixForRealTimeUpdates = "";
constexpr std::string_view kDefaultDataSourceForRealtimeUpdates = "realtime";

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

void LogDataLoadingMetrics(std::string_view source,
                           const DataLoadingStats& data_loading_stats) {
  LogIfError(KVServerContextMap()
                 ->SafeMetric()
                 .LogUpDownCounter<kTotalRowsUpdatedInDataLoading>(
                     {{std::string(source),
                       static_cast<double>(
                           data_loading_stats.total_updated_records)}}));
  LogIfError(KVServerContextMap()
                 ->SafeMetric()
                 .LogUpDownCounter<kTotalRowsDeletedInDataLoading>(
                     {{std::string(source),
                       static_cast<double>(
                           data_loading_stats.total_deleted_records)}}));
  LogIfError(KVServerContextMap()
                 ->SafeMetric()
                 .LogUpDownCounter<kTotalRowsDroppedInDataLoading>(
                     {{std::string(source),
                       static_cast<double>(
                           data_loading_stats.total_dropped_records)}}));
}

absl::Status ApplyUpdateMutation(
    std::string_view prefix, const KeyValueMutationRecord& record, Cache& cache,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  if (record.value_type() == Value::StringValue) {
    cache.UpdateKeyValue(log_context, record.key()->string_view(),
                         GetRecordValue<std::string_view>(record),
                         record.logical_commit_time(), prefix);
    return absl::OkStatus();
  }
  if (record.value_type() == Value::StringSet) {
    auto values = GetRecordValue<std::vector<std::string_view>>(record);
    cache.UpdateKeyValueSet(log_context, record.key()->string_view(),
                            absl::MakeSpan(values),
                            record.logical_commit_time(), prefix);
    return absl::OkStatus();
  }
  if (record.value_type() == Value::UInt32Set) {
    auto values = GetRecordValue<std::vector<uint32_t>>(record);
    cache.UpdateKeyValueSet(log_context, record.key()->string_view(),
                            absl::MakeSpan(values),
                            record.logical_commit_time(), prefix);
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Record with key: ", record.key()->string_view(),
                   " has unsupported value type: ", record.value_type()));
}

absl::Status ApplyDeleteMutation(
    std::string_view prefix, const KeyValueMutationRecord& record, Cache& cache,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  if (record.value_type() == Value::StringValue) {
    cache.DeleteKey(log_context, record.key()->string_view(),
                    record.logical_commit_time(), prefix);
    return absl::OkStatus();
  }
  if (record.value_type() == Value::StringSet) {
    auto values = GetRecordValue<std::vector<std::string_view>>(record);
    cache.DeleteValuesInSet(log_context, record.key()->string_view(),
                            absl::MakeSpan(values),
                            record.logical_commit_time(), prefix);
    return absl::OkStatus();
  }
  if (record.value_type() == Value::UInt32Set) {
    auto values = GetRecordValue<std::vector<uint32_t>>(record);
    cache.DeleteValuesInSet(log_context, record.key()->string_view(),
                            absl::MakeSpan(values),
                            record.logical_commit_time(), prefix);
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Record with key: ", record.key()->string_view(),
                   " has unsupported value type: ", record.value_type()));
}

bool ShouldProcessRecord(
    const KeyValueMutationRecord& record, int64_t num_shards,
    int64_t server_shard_num, const KeySharder& key_sharder,
    DataLoadingStats& data_loading_stats,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
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
    int64_t& max_timestamp, DataLoadingStats& data_loading_stats,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  switch (record.mutation_type()) {
    case KeyValueMutationType::Update: {
      if (auto status = ApplyUpdateMutation(prefix, record, cache, log_context);
          !status.ok()) {
        return status;
      }
      max_timestamp = std::max(max_timestamp, record.logical_commit_time());
      data_loading_stats.total_updated_records++;
      break;
    }
    case KeyValueMutationType::Delete: {
      if (auto status = ApplyDeleteMutation(prefix, record, cache, log_context);
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
    std::string_view data_source, std::string_view prefix,
    StreamRecordReader& record_reader, Cache& cache, int64_t& max_timestamp,
    const int32_t server_shard_num, const int32_t num_shards,
    UdfClient& udf_client, const KeySharder& key_sharder,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  DataLoadingStats data_loading_stats;
  const auto process_data_record_fn = [prefix, &cache, &max_timestamp,
                                       &data_loading_stats, server_shard_num,
                                       num_shards, &udf_client, &key_sharder,
                                       &log_context](
                                          const DataRecord& data_record) {
    if (data_record.record_type() == Record::KeyValueMutationRecord) {
      const auto* record = data_record.record_as_KeyValueMutationRecord();
      if (!ShouldProcessRecord(*record, num_shards, server_shard_num,
                               key_sharder, data_loading_stats, log_context)) {
        // NOTE: currently upstream logic retries on non-ok status
        // this will get us in a loop
        return absl::OkStatus();
      }
      return ApplyKeyValueMutationToCache(prefix, *record, cache, max_timestamp,
                                          data_loading_stats, log_context);
    } else if (data_record.record_type() ==
               Record::UserDefinedFunctionsConfig) {
      const auto* udf_config =
          data_record.record_as_UserDefinedFunctionsConfig();
      PS_VLOG(3, log_context)
          << "Setting UDF code snippet for version: " << udf_config->version()
          << ", handler: " << udf_config->handler_name()->str()
          << ", code length: " << udf_config->code_snippet()->str().size();
      return udf_client.SetCodeObject(
          CodeConfig{
              .js = udf_config->code_snippet()->str(),
              .udf_handler_name = udf_config->handler_name()->str(),
              .logical_commit_time = udf_config->logical_commit_time(),
              .version = udf_config->version(),
          },
          log_context);
    }
    return absl::InvalidArgumentError("Received unsupported record.");
  };
  // TODO(b/314302953): ReadStreamRecords will skip over individual records that
  // have errors. We should pass the file name to the function so that it will
  // appear in error logs.
  PS_RETURN_IF_ERROR(record_reader.ReadStreamRecords(
      [&process_data_record_fn](std::string_view raw) {
        return DeserializeDataRecord(raw, process_data_record_fn);
      }));
  LogDataLoadingMetrics(data_source, data_loading_stats);
  return data_loading_stats;
}

// Reads the file from `location` and updates the cache based on the delta read.
absl::StatusOr<DataLoadingStats> LoadCacheWithDataFromFile(
    const BlobStorageClient::DataLocation& location,
    const DataOrchestrator::Options& options) {
  PS_LOG(INFO, options.log_context) << "Loading " << location;
  int64_t max_timestamp = 0;
  auto& cache = options.cache;
  auto record_reader =
      options.delta_stream_reader_factory.CreateConcurrentReader(
          /*stream_factory=*/[&location, &options]() {
            return std::make_unique<BlobRecordStream>(
                options.blob_client.GetBlobReader(location));
          });
  PS_ASSIGN_OR_RETURN(auto metadata, record_reader->GetKVFileMetadata(),
                      _ << "Blob " << location);
  if (metadata.has_sharding_metadata() &&
      metadata.sharding_metadata().shard_num() != options.shard_num) {
    PS_LOG(INFO, options.log_context)
        << "Blob " << location << " belongs to shard num "
        << metadata.sharding_metadata().shard_num()
        << " but server shard num is " << options.shard_num << " Skipping it.";
    return DataLoadingStats{
        .total_updated_records = 0,
        .total_deleted_records = 0,
        .total_dropped_records = 0,
    };
  }
  std::string file_name =
      location.prefix.empty()
          ? location.key
          : absl::StrCat(location.prefix, "/", location.key);
  PS_ASSIGN_OR_RETURN(
      auto data_loading_stats,
      LoadCacheWithData(file_name, location.prefix, *record_reader, cache,
                        max_timestamp, options.shard_num, options.num_shards,
                        options.udf_client, options.key_sharder,
                        options.log_context),
      _ << "Blob: " << location);
  cache.RemoveDeletedKeys(options.log_context, max_timestamp, location.prefix);
  return data_loading_stats;
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
       {"prefix", std::move(location.prefix)},
       {"key", std::move(location.key)}});
}

class DataOrchestratorImpl : public DataOrchestrator {
 public:
  // `last_basename` is the last file seen during init. The cache is up to
  // date until this file.
  DataOrchestratorImpl(
      Options options,
      absl::flat_hash_map<std::string, std::string> prefix_last_basenames)
      : options_(std::move(options)),
        prefix_last_basenames_(std::move(prefix_last_basenames)) {}

  ~DataOrchestratorImpl() override {
    if (!data_loader_thread_) return;
    {
      absl::MutexLock l(&mu_);
      stop_ = true;
    }
    PS_LOG(INFO, options_.log_context)
        << "Sent cancel signal to data loader thread";
    PS_LOG(INFO, options_.log_context)
        << "Stopping loading new data from " << options_.data_bucket;
    if (options_.delta_notifier.IsRunning()) {
      if (const auto s = options_.delta_notifier.Stop(); !s.ok()) {
        PS_LOG(ERROR, options_.log_context) << "Failed to stop notify: " << s;
      }
    }
    PS_LOG(INFO, options_.log_context) << "Delta notifier stopped";
    data_loader_thread_->join();
    PS_LOG(INFO, options_.log_context) << "Stopped loading new data";
  }

  static absl::StatusOr<absl::flat_hash_map<std::string, std::string>> Init(
      Options& options) {
    auto ending_delta_files = LoadSnapshotFiles(options);
    if (!ending_delta_files.ok()) {
      return ending_delta_files.status();
    }
    for (const auto& prefix : options.blob_prefix_allowlist.Prefixes()) {
      auto location = BlobStorageClient::DataLocation{
          .bucket = options.data_bucket, .prefix = prefix};
      auto iter = ending_delta_files->find(prefix);
      auto maybe_filenames = options.blob_client.ListBlobs(
          location,
          {.prefix = std::string(FilePrefix<FileType::DELTA>()),
           .start_after =
               iter != ending_delta_files->end() ? iter->second : ""});
      if (!maybe_filenames.ok()) {
        return maybe_filenames.status();
      }
      PS_LOG(INFO, options.log_context)
          << "Initializing cache with " << maybe_filenames->size()
          << " delta files from " << location;
      for (auto&& basename : std::move(*maybe_filenames)) {
        auto blob = BlobStorageClient::DataLocation{
            .bucket = options.data_bucket, .prefix = prefix, .key = basename};
        if (!IsDeltaFilename(blob.key)) {
          PS_LOG(WARNING, options.log_context)
              << "Saw a file " << blob
              << " not in delta file format. Skipping it.";
          continue;
        }
        (*ending_delta_files)[prefix] = blob.key;
        if (const auto s = TraceLoadCacheWithDataFromFile(blob, options);
            !s.ok()) {
          return s.status();
        }
        PS_LOG(INFO, options.log_context) << "Done loading " << blob;
      }
    }
    return ending_delta_files;
  }

  absl::Status Start() override {
    if (data_loader_thread_) {
      return absl::OkStatus();
    }
    PS_LOG(INFO, options_.log_context)
        << "Transitioning to state ContinuouslyLoadNewData";
    auto prefix_last_basenames = prefix_last_basenames_;
    absl::Status status = options_.delta_notifier.Start(
        options_.change_notifier, {.bucket = options_.data_bucket},
        std::move(prefix_last_basenames),
        absl::bind_front(&DataOrchestratorImpl::EnqueueNewFilesToProcess,
                         this));
    if (!status.ok()) {
      return status;
    }
    data_loader_thread_ = std::make_unique<std::thread>(
        absl::bind_front(&DataOrchestratorImpl::ProcessNewFiles, this));

    return options_.realtime_thread_pool_manager.Start(
        [this, &cache = options_.cache, &log_context = options_.log_context,
         &delta_stream_reader_factory = options_.delta_stream_reader_factory](
            const std::string& message_body) {
          return LoadCacheWithHighPriorityUpdates(
              kDefaultDataSourceForRealtimeUpdates,
              kDefaultPrefixForRealTimeUpdates, delta_stream_reader_factory,
              message_body, cache, log_context);
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
    PS_LOG(INFO, options_.log_context)
        << "Thread for new file processing started";
    absl::Condition has_new_event(this,
                                  &DataOrchestratorImpl::HasNewEventToProcess);
    while (true) {
      std::string basename;
      {
        absl::MutexLock l(&mu_, has_new_event);
        if (stop_) {
          PS_LOG(INFO, options_.log_context)
              << "Thread for new file processing stopped";
          return;
        }
        basename = std::move(unprocessed_basenames_.back());
        unprocessed_basenames_.pop_back();
      }
      PS_LOG(INFO, options_.log_context) << "Loading " << basename;
      auto blob = ParseBlobName(basename);
      if (!IsDeltaFilename(blob.key)) {
        PS_LOG(WARNING, options_.log_context)
            << "Received file with invalid name: " << basename;
        continue;
      }
      if (!options_.blob_prefix_allowlist.Contains(blob.prefix)) {
        PS_LOG(WARNING, options_.log_context)
            << "Received file with prefix not allowlisted: " << basename;
        continue;
      }
      RetryUntilOk(
          [this, &basename, &blob] {
            // TODO: distinguish status. Some can be retried while others
            // are fatal.
            return TraceLoadCacheWithDataFromFile(
                {.bucket = options_.data_bucket,
                 .prefix = blob.prefix,
                 .key = blob.key},
                options_);
          },
          "LoadNewFile", LogStatusSafeMetricsFn<kLoadNewFilesStatus>(),
          options_.log_context);
    }
  }

  // Puts newly found file names into `unprocessed_basenames_`.
  void EnqueueNewFilesToProcess(const std::string& basename) {
    absl::MutexLock l(&mu_);
    unprocessed_basenames_.push_front(basename);
    PS_LOG(INFO, options_.log_context)
        << "queued " << basename << " for loading";
    // TODO: block if the queue is too large: consumption is too slow.
  }

  // Loads snapshot files if there are any.
  // Returns the latest delta file to be included in a snapshot.
  static absl::StatusOr<absl::flat_hash_map<std::string, std::string>>
  LoadSnapshotFiles(const Options& options) {
    absl::flat_hash_map<std::string, std::string> ending_delta_files;
    for (const auto& prefix : options.blob_prefix_allowlist.Prefixes()) {
      auto location = BlobStorageClient::DataLocation{
          .bucket = options.data_bucket, .prefix = prefix};
      PS_LOG(INFO, options.log_context)
          << "Initializing cache with snapshot file(s) from: " << location;
      PS_ASSIGN_OR_RETURN(
          auto snapshot_group,
          FindMostRecentFileGroup(
              location,
              FileGroupFilter{.file_type = FileType::SNAPSHOT,
                              .status = FileGroup::FileStatus::kComplete},
              options.blob_client));
      if (!snapshot_group.has_value()) {
        PS_LOG(INFO, options.log_context)
            << "No snapshot files found in: " << location;
        continue;
      }
      for (const auto& snapshot : snapshot_group->Filenames()) {
        auto snapshot_blob = BlobStorageClient::DataLocation{
            .bucket = options.data_bucket, .prefix = prefix, .key = snapshot};
        auto record_reader =
            options.delta_stream_reader_factory.CreateConcurrentReader(
                /*stream_factory=*/[&snapshot_blob, &options]() {
                  return std::make_unique<BlobRecordStream>(
                      options.blob_client.GetBlobReader(snapshot_blob));
                });
        PS_ASSIGN_OR_RETURN(auto metadata, record_reader->GetKVFileMetadata());
        if (metadata.has_sharding_metadata() &&
            metadata.sharding_metadata().shard_num() != options.shard_num) {
          PS_LOG(INFO, options.log_context)
              << "Snapshot " << snapshot_blob << " belongs to shard num "
              << metadata.sharding_metadata().shard_num()
              << " but server shard num is " << options.shard_num
              << ". Skipping it.";
          continue;
        }
        PS_LOG(INFO, options.log_context)
            << "Loading snapshot file: " << snapshot_blob;
        PS_ASSIGN_OR_RETURN(
            auto stats, TraceLoadCacheWithDataFromFile(snapshot_blob, options));
        if (auto iter = ending_delta_files.find(prefix);
            iter == ending_delta_files.end() ||
            metadata.snapshot().ending_delta_file() > iter->second) {
          ending_delta_files[prefix] = metadata.snapshot().ending_delta_file();
        }
        PS_LOG(INFO, options.log_context)
            << "Done loading snapshot file: " << snapshot_blob;
      }
    }
    return ending_delta_files;
  }

  absl::StatusOr<DataLoadingStats> LoadCacheWithHighPriorityUpdates(
      std::string_view data_source, std::string_view prefix,
      StreamRecordReaderFactory& delta_stream_reader_factory,
      const std::string& record_string, Cache& cache,
      privacy_sandbox::server_common::log::PSLogContext& log_context) {
    std::istringstream is(record_string);
    int64_t max_timestamp = 0;
    auto record_reader = delta_stream_reader_factory.CreateReader(is);
    return LoadCacheWithData(data_source, prefix, *record_reader, cache,
                             max_timestamp, options_.shard_num,
                             options_.num_shards, options_.udf_client,
                             options_.key_sharder, log_context);
  }

  const Options options_;
  absl::Mutex mu_;
  std::deque<std::string> unprocessed_basenames_ ABSL_GUARDED_BY(mu_);
  std::unique_ptr<std::thread> data_loader_thread_;
  bool stop_ ABSL_GUARDED_BY(mu_) = false;
  // last basename of file in initialization.
  absl::flat_hash_map<std::string, std::string> prefix_last_basenames_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<DataOrchestrator>> DataOrchestrator::TryCreate(
    Options options) {
  const auto prefix_last_basenames = DataOrchestratorImpl::Init(options);
  if (!prefix_last_basenames.ok()) {
    return prefix_last_basenames.status();
  }
  auto orchestrator = std::make_unique<DataOrchestratorImpl>(
      std::move(options), std::move(prefix_last_basenames.value()));
  return orchestrator;
}
}  // namespace kv_server
