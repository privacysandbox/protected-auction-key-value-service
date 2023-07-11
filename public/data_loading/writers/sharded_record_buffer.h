/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PUBLIC_DATA_LOADING_WRITERS_SHARDED_RECORD_BUFFER_H_
#define PUBLIC_DATA_LOADING_WRITERS_SHARDED_RECORD_BUFFER_H_

#include <iostream>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "public/data_loading/records_utils.h"
#include "public/sharding/sharding_function.h"

namespace kv_server {

// A `RecordBuffer` buffers `DataRecordStruct` records serialized as
// `data_loading.fbs:DataRecord` flatbuffers. Records can be read
// out of the buffer from `RecordStream()`.
class RecordBuffer {
 public:
  virtual ~RecordBuffer() = default;
  static std::unique_ptr<RecordBuffer> Create();
  // Returns an error status if adding a record to the buffer fails for some
  // reason.
  virtual absl::Status AddRecord(const DataRecordStruct& record) = 0;
  // Flushes buffered records so that they are visible for reading via
  // `RecordStream()`.
  virtual absl::Status Flush() = 0;
  // Call `Flush()` to guarantee that all buffered records are visible before
  // reading.
  virtual std::istream* RecordStream() = 0;
};

// A `ShardedRecordBuffer` buffers `DataRecordStruct` records
// serialized as `data_loading.fbs:DataRecord` flatbuffers in
// separate sharded streams
class ShardedRecordBuffer {
 public:
  ~ShardedRecordBuffer() = default;
  ShardedRecordBuffer(const ShardedRecordBuffer&) = delete;
  ShardedRecordBuffer& operator=(const ShardedRecordBuffer&) = delete;

  static absl::StatusOr<std::unique_ptr<ShardedRecordBuffer>> Create(
      int num_shards, ShardingFunction sharding_func = ShardingFunction(""));
  absl::StatusOr<std::istream*> GetShardRecordStream(int shard_id);
  absl::Status AddRecord(const DataRecordStruct& record);
  // Flushes buffered records so that they are visible for reading via
  // `RecordStream()`. Specify a `shard_id` to flush records buffered for a
  // specific shard or -1 to flush all buffered records.
  absl::Status Flush(int shard_id = -1);

 private:
  ShardedRecordBuffer(ShardingFunction sharding_func,
                      std::vector<std::unique_ptr<RecordBuffer>> shard_buffers);
  ShardingFunction sharding_func_;
  std::vector<std::unique_ptr<RecordBuffer>> shard_buffers_;
};

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_WRITERS_SHARDED_RECORD_BUFFER_H_
