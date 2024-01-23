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

#ifndef TOOLS_REQUEST_SIMULATION_REALTIME_MESSAGE_BATCHER_H_
#define TOOLS_REQUEST_SIMULATION_REALTIME_MESSAGE_BATCHER_H_

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "components/tools/concurrent_publishing_engine.h"
#include "public/data_loading/writers/delta_record_limiting_file_writer.h"
#include "public/sharding/key_sharder.h"

namespace kv_server {
// RealtimeMessageBatcher receives key-value pairs from the Insert method.
// It then puts them in per shard queue. If on insert the per shard queue gets
// over the `message_size_kb` limit, RealtimeMessageBatcher batches all the
// messages together in one realtime message, tags it with a shard number, and
// inserts it into `realtime_messages_`.
// Not thread safe.
class RealtimeMessageBatcher {
 public:
  // Not thread safe.
  absl::Status Insert(
      kv_server::KeyValueMutationRecordStruct key_value_mutation);
  ~RealtimeMessageBatcher();
  // `queue_mutex` and `realtime_messages` are not owned by
  // RealtimeMessageBatcher and must outlive it.
  static absl::StatusOr<std::unique_ptr<RealtimeMessageBatcher>> Create(
      std::queue<RealtimeMessage>& realtime_messages, absl::Mutex& queue_mutex,
      int num_shards, int message_size_kb);

 private:
  class FileHandler {
   public:
    FileHandler(std::string file_name, int file_descriptor);
    ~FileHandler();
    const std::string file_name;
    static absl::StatusOr<std::unique_ptr<FileHandler>> Create();

   private:
    const int file_descriptor_;
  };

  struct DeltaRecordLimitingFileWriterWrapper {
    std::unique_ptr<FileHandler> file_handler;
    std::unique_ptr<DeltaRecordLimitingFileWriter> file_writer;
  };

  RealtimeMessageBatcher(
      std::vector<DeltaRecordLimitingFileWriterWrapper> writer_wrappers,
      std::queue<RealtimeMessage>& realtime_messages, absl::Mutex& queue_mutex,
      int num_shards, int message_size_kb);

  DeltaRecordLimitingFileWriter& GetWriter(int shard_num) const;
  RealtimeMessage GetMessage(int shard_num) const;
  absl::Mutex& mutex_;
  std::queue<RealtimeMessage>& realtime_messages_ ABSL_GUARDED_BY(mutex_);
  std::vector<DeltaRecordLimitingFileWriterWrapper> writer_wrappers_;
  const kv_server::KeySharder key_sharder_;

  const int num_shards_;
  const int message_size_kb_;
};

}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_REALTIME_MESSAGE_BATCHER_H_
