/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TOOLS_REQUEST_SIMULATION_DELTA_BASED_REALTIME_UPDATES_PUBLISHER_H_
#define TOOLS_REQUEST_SIMULATION_DELTA_BASED_REALTIME_UPDATES_PUBLISHER_H_

#include <deque>
#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "components/data/blob_storage/blob_storage_change_notifier.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/data/blob_storage/delta_file_notifier.h"
#include "components/data/realtime/realtime_notifier.h"
#include "public/data_loading/readers/stream_record_reader_factory.h"
#include "tools/request_simulation/realtime_message_batcher.h"
#include "tools/request_simulation/request_generation_util.h"

namespace kv_server {

// This class will continuously watch for new delta files in the bucket, read
// key-value pairs from delta file, batch them in a single realtime update
// and insert them in the realtime Pubsub/SNS.
class DeltaBasedRealtimeUpdatesPublisher {
 public:
  struct Options {
    // Bucket to keep loading data from.
    std::string data_bucket;
    // Message queue to hold generated requests
    std::queue<RealtimeMessage>& realtime_messages;
    absl::Mutex& realtime_messages_mutex;
    BlobStorageClient& blob_client;
    DeltaFileNotifier& delta_notifier;
    BlobStorageChangeNotifier& change_notifier;
    StreamRecordReaderFactory& delta_stream_reader_factory;
  };
  DeltaBasedRealtimeUpdatesPublisher(
      std::unique_ptr<RealtimeMessageBatcher> realtime_message_batcher,
      Options options)
      : realtime_message_batcher_(std::move(realtime_message_batcher)),
        options_(std::move(options)),
        data_load_thread_manager_(
            TheadManager::Create("Realtime sharded publisher thread")) {}
  ~DeltaBasedRealtimeUpdatesPublisher() = default;

  // DeltaBasedRealtimeUpdatesPublisher is neither copyable nor movable.
  DeltaBasedRealtimeUpdatesPublisher(
      const DeltaBasedRealtimeUpdatesPublisher&) = delete;
  DeltaBasedRealtimeUpdatesPublisher& operator=(
      const DeltaBasedRealtimeUpdatesPublisher&) = delete;

  // Starts the thread of data loading of existing delta files
  // and starts delta file notifier to watch
  // incoming new file
  absl::Status Start();
  // Stops the thread of data loading and stops delta file notifier
  absl::Status Stop();
  // Check if the thread of data loading is running
  bool IsRunning() const;

 private:
  std::unique_ptr<RealtimeMessageBatcher> realtime_message_batcher_;
  // Checks if there is a new event such as new file or stop event to process
  bool HasNewEventToProcess() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  absl::Mutex mu_;
  Options options_;
  // Adds the new files to the queue of unprocessed files
  void EnqueueNewFilesToProcess(const std::string& basename);
  // Loads and processes new file
  void ProcessNewFiles();
  // Creates realtime messages from delta file and put them in the message queue
  absl::StatusOr<DataLoadingStats> CreateRealtimeMessagesAndAddToQueue(
      const BlobStorageClient::DataLocation& location);
  absl::StatusOr<DataLoadingStats> TraceCreateRealtimeMessagesAndAddToQueue(
      BlobStorageClient::DataLocation location);
  std::deque<std::string> unprocessed_basenames_ ABSL_GUARDED_BY(mu_);
  bool stop_ ABSL_GUARDED_BY(mu_) = false;
  std::unique_ptr<TheadManager> data_load_thread_manager_;
};

}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_DELTA_BASED_REALTIME_UPDATES_PUBLISHER_H_
