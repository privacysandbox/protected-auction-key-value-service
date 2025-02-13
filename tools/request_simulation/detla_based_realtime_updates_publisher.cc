// Copyright 2024 Google LLC
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

#include "tools/request_simulation/detla_based_realtime_updates_publisher.h"

#include "absl/functional/bind_front.h"
#include "public/data_loading/filename_utils.h"
#include "src/util/status_macro/status_macros.h"

using ::privacy_sandbox::server_common::RetryUntilOk;
using privacy_sandbox::server_common::TraceWithStatusOr;

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
}  // namespace

// b/321746753 -- this class can potentially benefit from refactoring
absl::Status DeltaBasedRealtimeUpdatesPublisher::Start() {
  LOG(INFO) << "Start monitor delta files and publish them as realtime updates";
  PS_RETURN_IF_ERROR(options_.delta_notifier.Start(
      options_.change_notifier, {.bucket = options_.data_bucket},
      {std::make_pair("", "")},
      absl::bind_front(
          &DeltaBasedRealtimeUpdatesPublisher::EnqueueNewFilesToProcess,
          this)));
  PS_RETURN_IF_ERROR(
      data_load_thread_manager_->Start([this]() { ProcessNewFiles(); }));

  return rt_publisher_thread_manager_->Start(
      [this]() { concurrent_publishing_engine_->Start(); });
}

absl::Status DeltaBasedRealtimeUpdatesPublisher::Stop() {
  {
    absl::MutexLock l(&mu_);
    stop_ = true;
  }
  LOG(INFO) << "Stopping publishing realtime updates from "
            << options_.data_bucket;
  if (options_.delta_notifier.IsRunning()) {
    if (const auto status = options_.delta_notifier.Stop(); !status.ok()) {
      LOG(ERROR) << "Failed to stop notify: " << status;
    }
  }

  if (rt_publisher_thread_manager_->IsRunning()) {
    concurrent_publishing_engine_->Stop();
    if (const auto status = rt_publisher_thread_manager_->Stop();
        !status.ok()) {
      LOG(ERROR) << "Failed to stop realtime publisher thread manager: "
                 << status;
    }
  }
  LOG(INFO) << "Delta notifier stopped";
  LOG(INFO) << "Stopping publishing realtime updates from";
  return data_load_thread_manager_->Stop();
}

bool DeltaBasedRealtimeUpdatesPublisher::IsRunning() const {
  return data_load_thread_manager_->IsRunning();
}

void DeltaBasedRealtimeUpdatesPublisher::EnqueueNewFilesToProcess(
    const std::string& basename) {
  absl::MutexLock l(&mu_);
  unprocessed_basenames_.push_front(basename);
  LOG(INFO) << "queued " << basename << " for loading";
}

void DeltaBasedRealtimeUpdatesPublisher::ProcessNewFiles() {
  LOG(INFO) << "Thread for new file processing started";
  absl::Condition has_new_event(
      this, &DeltaBasedRealtimeUpdatesPublisher::HasNewEventToProcess);
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
          return TraceCreateRealtimeMessagesAndAddToQueue(
              {.bucket = options_.data_bucket, .key = basename});
        },
        "LoadNewFileAndCreateRequest", [](const absl::Status&, int) {});
  }
}
absl::StatusOr<DataLoadingStats>
DeltaBasedRealtimeUpdatesPublisher::CreateRealtimeMessagesAndAddToQueue(
    const BlobStorageClient::DataLocation& location) {
  LOG(INFO) << "Loading " << location;
  auto& blob_client = options_.blob_client;
  auto record_reader =
      options_.delta_stream_reader_factory.CreateConcurrentReader(
          /*stream_factory=*/[&location, &blob_client]() {
            return std::make_unique<BlobRecordStream>(
                blob_client.GetBlobReader(location));
          });
  DataLoadingStats data_loading_stats;
  const auto process_data_record_fn = [this, &data_loading_stats](
                                          const DataRecord& data_record) {
    if (data_record.record_type() == Record::KeyValueMutationRecord) {
      const auto* kv_mutation_record =
          data_record.record_as_KeyValueMutationRecord();
      if (kv_mutation_record->value_type() == Value::StringValue) {
        KeyValueMutationRecordT kv_record_struct;
        kv_mutation_record->UnPackTo(&kv_record_struct);
        auto insert_status =
            realtime_message_batcher_->Insert(std::move(kv_record_struct));
        if (!insert_status.ok()) {
          LOG(ERROR) << "Error inserting KV record: " << insert_status;
        }
        if (kv_record_struct.mutation_type == KeyValueMutationType::Update) {
          data_loading_stats.total_updated_records++;
        } else if (kv_record_struct.mutation_type ==
                   KeyValueMutationType::Delete) {
          data_loading_stats.total_deleted_records++;
        }
        return absl::OkStatus();
      }
    }
  };
  PS_RETURN_IF_ERROR(record_reader->ReadStreamRecords(
      [&process_data_record_fn](std::string_view raw) {
        return DeserializeRecord(raw, process_data_record_fn);
      }));
  return data_loading_stats;
}
bool DeltaBasedRealtimeUpdatesPublisher::HasNewEventToProcess() const {
  return !unprocessed_basenames_.empty() || stop_;
}
absl::StatusOr<DataLoadingStats>
DeltaBasedRealtimeUpdatesPublisher::TraceCreateRealtimeMessagesAndAddToQueue(
    BlobStorageClient::DataLocation location) {
  return TraceWithStatusOr(
      [location, this] {
        return CreateRealtimeMessagesAndAddToQueue(location);
      },
      "CreateRealtimeMessagesAndAddToQueue",
      {{"bucket", std::move(location.bucket)},
       {"key", std::move(location.key)}});
}

}  // namespace kv_server
