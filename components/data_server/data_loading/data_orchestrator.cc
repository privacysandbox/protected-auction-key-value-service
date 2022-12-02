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

#include <deque>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "absl/strings/str_cat.h"
#include "components/errors/retry.h"
#include "glog/logging.h"
#include "public/constants.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/filename_utils.h"

namespace fledge::kv_server {
namespace {

// Reads the file from `location` and updates the cache based on the delta read.
absl::Status LoadCacheWithDataFromFile(
    const BlobStorageClient::DataLocation& location,
    const DataOrchestrator::Options& options) {
  LOG(INFO) << "Loading " << location;
  std::unique_ptr<BlobReader> blob_reader =
      options.blob_client.GetBlobReader(location);
  std::unique_ptr<StreamRecordReader<std::string_view>> record_reader =
      options.delta_stream_reader_factory.CreateReader(blob_reader->Stream());
  auto maybe_metadata = record_reader->GetKVFileMetadata();
  if (!maybe_metadata.ok()) {
    return maybe_metadata.status();
  }
  if (!maybe_metadata->has_key_namespace()) {
    return absl::InvalidArgumentError(
        "key namespace is not set in file metadata");
  }
  Cache& cache =
      options.cache.GetMutableCacheShard(maybe_metadata->key_namespace());
  return record_reader->ReadStreamRecords([&cache](std::string_view raw) {
    auto record = flatbuffers::GetRoot<DeltaFileRecord>(raw.data());

    auto recordVerifier = flatbuffers::Verifier(
        reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
    if (!record->Verify(recordVerifier)) {
      // TODO(b/239061954): Publish metrics for alerting
      return absl::InvalidArgumentError("Invalid flatbuffer format");
    }

    switch (record->mutation_type()) {
      case DeltaMutationType::Update: {
        cache.UpdateKeyValue(
            {record->key()->string_view(), record->subkey()->string_view()},
            record->value()->str());
        break;
      }
      case DeltaMutationType::Delete: {
        cache.DeleteKey(
            {record->key()->string_view(), record->subkey()->string_view()});
        break;
      }
      default:
        return absl::InvalidArgumentError(
            absl::StrCat("Invalid mutation type: ",
                         EnumNameDeltaMutationType(record->mutation_type())));
    }
    return absl::OkStatus();
  });
}

class DataOrchestratorImpl : public DataOrchestrator {
 public:
  // `last_basename` is the last file seen during init. The cache is up to date
  // until this file.
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
      if (const auto s = options_.delta_notifier.StopNotify(); !s.ok()) {
        LOG(ERROR) << "Failed to stop notify: " << s;
      }
    }
    LOG(INFO) << "Delta notifier stopped";
    data_loader_thread_->join();
    LOG(INFO) << "Stopped loading new data";
  }

  static absl::StatusOr<std::string> Init(Options& options) {
    auto maybe_filenames = options.blob_client.ListBlobs(
        {.bucket = options.data_bucket},
        {.prefix = std::string(FilePrefix<FileType::DELTA>())});
    if (!maybe_filenames.ok()) {
      return maybe_filenames.status();
    }
    LOG(INFO) << "Initializing cache with " << maybe_filenames->size()
              << " files from " << options.data_bucket;

    std::string last_basename;
    for (auto&& basename : std::move(*maybe_filenames)) {
      if (!IsDeltaFilename(basename)) {
        LOG(WARNING) << "Saw a file " << basename
                     << " not in delta file format. Skipping it.";
        continue;
      }
      last_basename = basename;
      if (const auto s = LoadCacheWithDataFromFile(
              {.bucket = options.data_bucket, .key = std::move(basename)},
              options);
          !s.ok()) {
        return s;
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
    // TODO: rename StartNotify to Start.
    const absl::Status status = options_.delta_notifier.StartNotify(
        options_.change_notifier, {.bucket = options_.data_bucket},
        last_basename_of_init_,
        absl::bind_front(&DataOrchestratorImpl::EnqueueNewFilesToProcess,
                         this));
    if (!status.ok()) {
      return status;
    }
    data_loader_thread_ = std::make_unique<std::thread>(
        absl::bind_front(&DataOrchestratorImpl::ProcessNewFiles, this));
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
            return LoadCacheWithDataFromFile(
                {.bucket = options_.data_bucket, .key = basename}, options_);
          },
          "LoadNewFile - " + basename);
    }
  }

  // Puts newly found file names into `unprocessed_basenames_`.
  void EnqueueNewFilesToProcess(const std::string& basename) {
    absl::MutexLock l(&mu_);
    unprocessed_basenames_.push_front(basename);
    LOG(INFO) << "queued " << basename << " for loading";
    // TODO: block if the queue is too large: consumption is too slow.
  }

  const Options options_;
  absl::Mutex mu_;
  std::deque<std::string> unprocessed_basenames_ GUARDED_BY(mu_);
  std::unique_ptr<std::thread> data_loader_thread_;
  bool stop_ GUARDED_BY(mu_) = false;
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
}  // namespace fledge::kv_server
