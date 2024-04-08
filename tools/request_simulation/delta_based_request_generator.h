/*
 * Copyright 2023 Google LLC
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

#ifndef TOOLS_REQUEST_SIMULATION_DELTA_BASED_REQUEST_GENERATOR_H_
#define TOOLS_REQUEST_SIMULATION_DELTA_BASED_REQUEST_GENERATOR_H_

#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "components/data/blob_storage/blob_storage_change_notifier.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/data/blob_storage/delta_file_notifier.h"
#include "components/data/realtime/realtime_notifier.h"
#include "public/data_loading/readers/riegeli_stream_io.h"
#include "public/data_loading/readers/stream_record_reader_factory.h"
#include "tools/request_simulation/message_queue.h"
#include "tools/request_simulation/request_generation_util.h"

namespace kv_server {

// This class will continuously watch for new delta file, read
// keys from delta file, create KV request message from each key and put message
// in the message queue.
class DeltaBasedRequestGenerator {
 public:
  struct Options {
    // Bucket to keep loading data from.
    std::string data_bucket;
    // Message queue to hold generated requests
    MessageQueue& message_queue;
    BlobStorageClient& blob_client;
    DeltaFileNotifier& delta_notifier;
    BlobStorageChangeNotifier& change_notifier;
    StreamRecordReaderFactory& delta_stream_reader_factory;
  };
  DeltaBasedRequestGenerator(
      Options options,
      absl::AnyInvocable<std::string(std::string_view)> request_generation_fn)
      : options_(std::move(options)),
        data_load_thread_manager_(
            ThreadManager::Create("Delta file loading thread")),
        request_generation_fn_(std::move(request_generation_fn)) {}
  ~DeltaBasedRequestGenerator() = default;

  // DeltaBasedRequestGenerator is neither copyable nor movable.
  DeltaBasedRequestGenerator(const DeltaBasedRequestGenerator&) = delete;
  DeltaBasedRequestGenerator& operator=(const DeltaBasedRequestGenerator&) =
      delete;

  // Starts the thread of data loading of existing delta files
  // and starts delta file notifier to watch
  // incoming new file
  absl::Status Start();
  // Stops the thread of data loading and stops delta file notifier
  absl::Status Stop();
  // Check if the thread of data loading is running
  bool IsRunning() const;

 private:
  // Checks if there is new event such as new file or stop event to process
  bool HasNewEventToProcess() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  // Adds the new files to the queue of unprocessed files
  void EnqueueNewFilesToProcess(const std::string& basename);
  // Loads and processes new file
  void ProcessNewFiles();
  // Creates requests from delta file and put them in the message queue
  absl::StatusOr<DataLoadingStats> CreateRequestsAndAddToMessageQueue(
      const BlobStorageClient::DataLocation& location);
  absl::StatusOr<DataLoadingStats> TraceCreateRequestsAndAddToMessageQueue(
      BlobStorageClient::DataLocation location);
  Options options_;
  absl::Mutex mu_;
  std::deque<std::string> unprocessed_basenames_ ABSL_GUARDED_BY(mu_);
  bool stop_ ABSL_GUARDED_BY(mu_) = false;
  std::unique_ptr<ThreadManager> data_load_thread_manager_;
  // Callback function to generate KV request from a given key
  absl::AnyInvocable<std::string(std::string_view)> request_generation_fn_;
};

}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_DELTA_BASED_REQUEST_GENERATOR_H_
