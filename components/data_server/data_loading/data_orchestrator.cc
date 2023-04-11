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
#include "absl/strings/str_cat.h"
#include "components/errors/retry.h"
#include "components/telemetry/tracing.h"
#include "glog/logging.h"
#include "public/constants.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/filename_utils.h"

namespace kv_server {
namespace {
// Holds an input stream pointing to a blob of Riegeli records.
class BlobRecordStream : public RecordStream {
 public:
  explicit BlobRecordStream(std::unique_ptr<BlobReader> blob_reader)
      : blob_reader_(std::move(blob_reader)) {}
  std::istream& Stream() { return blob_reader_->Stream(); }

 private:
  std::unique_ptr<BlobReader> blob_reader_;
};

absl::StatusOr<DataLoadingStats> LoadCacheWithData(
    StreamRecordReader<std::string_view>& record_reader, Cache& cache,
    int64_t& max_timestamp) {
  DataLoadingStats data_loading_stats;
  auto status = record_reader.ReadStreamRecords(
      [&cache, &max_timestamp, &data_loading_stats](std::string_view raw) {
        auto record = flatbuffers::GetRoot<DeltaFileRecord>(raw.data());
        auto recordVerifier = flatbuffers::Verifier(
            reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
        if (!record->Verify(recordVerifier)) {
          // TODO(b/239061954): Publish metrics for alerting
          return absl::InvalidArgumentError("Invalid flatbuffer format");
        }
        switch (record->mutation_type()) {
          case DeltaMutationType::Update: {
            cache.UpdateKeyValue(record->key()->string_view(),
                                 record->value()->string_view(),
                                 record->logical_commit_time());
            max_timestamp =
                std::max(max_timestamp, record->logical_commit_time());
            data_loading_stats.total_updated_records++;
            break;
          }
          case DeltaMutationType::Delete: {
            cache.DeleteKey(record->key()->string_view(),
                            record->logical_commit_time());
            max_timestamp =
                std::max(max_timestamp, record->logical_commit_time());
            data_loading_stats.total_deleted_records++;
            break;
          }
          default:
            return absl::InvalidArgumentError(absl::StrCat(
                "Invalid mutation type: ",
                EnumNameDeltaMutationType(record->mutation_type())));
        }
        return absl::OkStatus();
      });

  if (!status.ok()) {
    return status;
  }

  return data_loading_stats;
}

// Reads the file from `location` and updates the cache based on the delta read.
absl::StatusOr<DataLoadingStats> LoadCacheWithDataFromFile(
    MetricsRecorder& metrics_recorder,
    const BlobStorageClient::DataLocation& location,
    const DataOrchestrator::Options& options) {
  LOG(INFO) << "Loading " << location;
  int64_t max_timestamp = 0;
  auto& cache = options.cache;
  auto record_reader =
      options.delta_stream_reader_factory.CreateConcurrentReader(
          metrics_recorder,
          /*stream_factory=*/[&location, &options]() {
            return std::make_unique<BlobRecordStream>(
                options.blob_client.GetBlobReader(location));
          });
  auto status = LoadCacheWithData(*record_reader, cache, max_timestamp);
  if (status.ok()) {
    cache.RemoveDeletedKeys(max_timestamp);
  }
  return status;
}
absl::StatusOr<DataLoadingStats> TraceLoadCacheWithDataFromFile(
    MetricsRecorder& metrics_recorder, BlobStorageClient::DataLocation location,
    const DataOrchestrator::Options& options) {
  return TraceWithStatusOr(
      [&metrics_recorder, location, &options] {
        return LoadCacheWithDataFromFile(metrics_recorder, std::move(location),
                                         options);
      },
      "LoadCacheWithDataFromFile",
      {{"bucket", std::move(location.bucket)},
       {"key", std::move(location.key)}});
}

class DataOrchestratorImpl : public DataOrchestrator {
 public:
  // `last_basename` is the last file seen during init. The cache is up to date
  // until this file.
  DataOrchestratorImpl(Options options, std::string last_basename,
                       MetricsRecorder& metrics_recorder)
      : options_(std::move(options)),
        last_basename_of_init_(std::move(last_basename)),
        metrics_recorder_(metrics_recorder) {}

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

  static absl::StatusOr<std::string> Init(Options& options,
                                          MetricsRecorder& metrics_recorder) {
    auto ending_delta_file = LoadSnapshotFiles(options, metrics_recorder);
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
              metrics_recorder,
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
        last_basename_of_init_,
        absl::bind_front(&DataOrchestratorImpl::EnqueueNewFilesToProcess,
                         this));
    if (!status.ok()) {
      return status;
    }
    data_loader_thread_ = std::make_unique<std::thread>(
        absl::bind_front(&DataOrchestratorImpl::ProcessNewFiles, this));
    auto& cache = options_.cache;
    auto& delta_stream_reader_factory = options_.delta_stream_reader_factory;

    for (int i = 0; i < options_.realtime_options.size(); i++) {
      if (options_.realtime_options[i].delta_file_record_change_notifier ==
          nullptr) {
        LOG(ERROR) << "Realtime delta_file_record_change_notifier is nullptr, "
                      "realtime data loading disabled.";
        return absl::OkStatus();
      }
      if (options_.realtime_options[i].realtime_notifier == nullptr) {
        LOG(ERROR) << "Realtime realtime_notifier is nullptr, realtime data "
                      "loading disabled.";
        return absl::OkStatus();
      }

      auto realtime_notifier =
          options_.realtime_options[i].realtime_notifier.get();
      auto delta_file_record_change_notifier =
          options_.realtime_options[i].delta_file_record_change_notifier.get();
      auto status = realtime_notifier->Start(
          *delta_file_record_change_notifier,
          [this, &cache,
           &delta_stream_reader_factory](const std::string& message_body) {
            return LoadCacheWithHighPriorityUpdates(delta_stream_reader_factory,
                                                    message_body, cache);
          });
      if (!status.ok()) {
        return status;
      }
    }

    return absl::OkStatus();
  }

 private:
  bool HasNewEventToProcess() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    return !unprocessed_basenames_.empty() || stop_ == true;
  }
  // Reads new files, if any, from the `unprocessed_basenames_` queue and
  // processes them one by one.
  //
  // On failure, puts the file back to the end of the queue and retry at a later
  // point.
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
            // TODO: distinguish status. Some can be retried while others are
            // fatal.
            return TraceLoadCacheWithDataFromFile(
                metrics_recorder_,
                {.bucket = options_.data_bucket, .key = basename}, options_);
          },
          "LoadNewFile", &metrics_recorder_);
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
  static absl::StatusOr<std::string> LoadSnapshotFiles(
      const Options& options, MetricsRecorder& metrics_recorder) {
    absl::StatusOr<std::vector<std::string>> snapshots =
        options.blob_client.ListBlobs(
            {.bucket = options.data_bucket},
            {.prefix = FilePrefix<FileType::SNAPSHOT>().data()});
    if (!snapshots.ok()) {
      return snapshots.status();
    }
    LOG(INFO) << "Initializing cache with snapshot file(s) from: "
              << options.data_bucket;
    std::string ending_delta_file;

    for (int64_t s = snapshots->size() - 1; s >= 0; s--) {
      std::string_view snapshot = snapshots->at(s);
      if (!IsSnapshotFilename(snapshot)) {
        LOG(WARNING) << "Saw a file " << snapshot
                     << " not in snapshot file format. Skipping it.";
        continue;
      }
      BlobStorageClient::DataLocation location{.bucket = options.data_bucket,
                                               .key = snapshot.data()};
      auto record_reader =
          options.delta_stream_reader_factory.CreateConcurrentReader(
              metrics_recorder,
              /*stream_factory=*/[&location, &options]() {
                return std::make_unique<BlobRecordStream>(
                    options.blob_client.GetBlobReader(location));
              });
      auto metadata = record_reader->GetKVFileMetadata();
      if (!metadata.ok()) {
        return metadata.status();
      }
      LOG(INFO) << "Loading snapshot file: " << location.bucket << "/"
                << location.key;
      if (auto status = TraceLoadCacheWithDataFromFile(metrics_recorder,
                                                       location, options);
          !status.ok()) {
        return status.status();
      }
      if (metadata->snapshot().ending_delta_file() > ending_delta_file) {
        ending_delta_file = std::move(metadata->snapshot().ending_delta_file());
      }
      LOG(INFO) << "Done loading snapshot file: " << location.bucket << "/"
                << location.key;
      break;
    }
    return ending_delta_file;
  }

  absl::StatusOr<DataLoadingStats> LoadCacheWithHighPriorityUpdates(
      StreamRecordReaderFactory<std::string_view>& delta_stream_reader_factory,
      const std::string& record_string, Cache& cache) {
    std::istringstream is(record_string);
    int64_t max_timestamp = 0;
    auto record_reader = delta_stream_reader_factory.CreateReader(is);
    return LoadCacheWithData(*record_reader, cache, max_timestamp);
  }

  const Options options_;
  absl::Mutex mu_;
  std::deque<std::string> unprocessed_basenames_ GUARDED_BY(mu_);
  std::unique_ptr<std::thread> data_loader_thread_;
  bool stop_ GUARDED_BY(mu_) = false;
  // last basename of file in initialization.
  const std::string last_basename_of_init_;
  MetricsRecorder& metrics_recorder_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<DataOrchestrator>> DataOrchestrator::TryCreate(
    Options options, MetricsRecorder& metrics_recorder) {
  const auto maybe_last_basename =
      DataOrchestratorImpl::Init(options, metrics_recorder);
  if (!maybe_last_basename.ok()) {
    return maybe_last_basename.status();
  }
  auto orchestrator = std::make_unique<DataOrchestratorImpl>(
      std::move(options), std::move(maybe_last_basename.value()),
      metrics_recorder);
  return orchestrator;
}
}  // namespace kv_server
