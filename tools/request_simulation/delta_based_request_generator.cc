// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tools/request_simulation/delta_based_request_generator.h"

#include <utility>

#include "absl/functional/bind_front.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/filename_utils.h"
#include "public/data_loading/record_utils.h"
#include "src/errors/retry.h"
#include "src/telemetry/tracing.h"

using privacy_sandbox::server_common::MetricsRecorder;
using privacy_sandbox::server_common::TraceWithStatusOr;

namespace kv_server {
namespace {

using ::privacy_sandbox::server_common::RetryUntilOk;

// Holds an input stream pointing to a blob of Riegeli records.
class BlobRecordStream : public RecordStream {
 public:
  explicit BlobRecordStream(std::unique_ptr<BlobReader> blob_reader)
      : blob_reader_(std::move(blob_reader)) {}
  std::istream& Stream() { return blob_reader_->Stream(); }

 private:
  std::unique_ptr<BlobReader> blob_reader_;
};
}  // namespace

absl::Status DeltaBasedRequestGenerator::Start() {
  LOG(INFO) << "Start monitor and load delta files";
  absl::Status status = options_.delta_notifier.Start(
      options_.change_notifier, {.bucket = options_.data_bucket},
      {std::make_pair("", "")},
      absl::bind_front(&DeltaBasedRequestGenerator::EnqueueNewFilesToProcess,
                       this));
  if (!status.ok()) {
    return status;
  }
  return data_load_thread_manager_->Start([this]() { ProcessNewFiles(); });
}
absl::Status DeltaBasedRequestGenerator::Stop() {
  {
    absl::MutexLock l(&mu_);
    stop_ = true;
  }
  LOG(INFO) << "Stopping loading new data from " << options_.data_bucket;
  if (options_.delta_notifier.IsRunning()) {
    if (const auto status = options_.delta_notifier.Stop(); !status.ok()) {
      LOG(ERROR) << "Failed to stop notify: " << status;
    }
  }
  LOG(INFO) << "Delta notifier stopped";
  LOG(INFO) << "Stopping loading new data";
  return data_load_thread_manager_->Stop();
}
bool DeltaBasedRequestGenerator::IsRunning() const {
  return data_load_thread_manager_->IsRunning();
}
void DeltaBasedRequestGenerator::EnqueueNewFilesToProcess(
    const std::string& basename) {
  absl::MutexLock l(&mu_);
  unprocessed_basenames_.push_front(basename);
  LOG(INFO) << "queued " << basename << " for loading";
}
void DeltaBasedRequestGenerator::ProcessNewFiles() {
  LOG(INFO) << "Thread for new file processing started";
  absl::Condition has_new_event(
      this, &DeltaBasedRequestGenerator::HasNewEventToProcess);
  while (!data_load_thread_manager_->ShouldStop()) {
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
          return TraceCreateRequestsAndAddToMessageQueue(
              {.bucket = options_.data_bucket, .key = basename});
        },
        "LoadNewFileAndCreateRequest", [](const absl::Status&, int) {});
  }
}
absl::StatusOr<DataLoadingStats>
DeltaBasedRequestGenerator::CreateRequestsAndAddToMessageQueue(
    const BlobStorageClient::DataLocation& location) {
  LOG(INFO) << "Loading " << location;
  auto& blob_client = options_.blob_client;
  auto record_reader =
      options_.delta_stream_reader_factory.CreateConcurrentReader(
          /*stream_factory=*/[&location, &blob_client]() {
            return std::make_unique<BlobRecordStream>(
                blob_client.GetBlobReader(location));
          });
  auto metadata = record_reader->GetKVFileMetadata();
  if (!metadata.ok()) {
    return metadata.status();
  }
  DataLoadingStats data_loading_stats;
  const auto process_data_record_fn = [this, &data_loading_stats](
                                          const DataRecord& data_record) {
    if (data_record.record_type() == Record::KeyValueMutationRecord) {
      const auto* record = data_record.record_as_KeyValueMutationRecord();
      if (record->value_type() == Value::StringValue) {
        options_.message_queue.Push(
            request_generation_fn_(record->key()->string_view()));
        if (record->mutation_type() == KeyValueMutationType::Update) {
          data_loading_stats.total_updated_records++;
        } else if (record->mutation_type() == KeyValueMutationType::Delete) {
          data_loading_stats.total_deleted_records++;
        }
        return absl::OkStatus();
      }
    }
  };
  auto status = record_reader->ReadStreamRecords(
      [&process_data_record_fn](std::string_view raw) {
        return DeserializeRecord(raw, process_data_record_fn);
      });
  if (!status.ok()) {
    return status;
  }
  return data_loading_stats;
}
bool DeltaBasedRequestGenerator::HasNewEventToProcess() const {
  return !unprocessed_basenames_.empty() || stop_;
}
absl::StatusOr<DataLoadingStats>
DeltaBasedRequestGenerator::TraceCreateRequestsAndAddToMessageQueue(
    BlobStorageClient::DataLocation location) {
  return TraceWithStatusOr(
      [location, this] { return CreateRequestsAndAddToMessageQueue(location); },
      "CreateRequestsAndAddToMessageQueue",
      {{"bucket", std::move(location.bucket)},
       {"key", std::move(location.key)}});
}

}  // namespace kv_server
