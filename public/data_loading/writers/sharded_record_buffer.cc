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

#include "public/data_loading/writers/sharded_record_buffer.h"

#include <iostream>
#include <sstream>

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "riegeli/bytes/ostream_writer.h"
#include "riegeli/records/record_writer.h"

namespace kv_server {
namespace {

class RecordBufferImpl : public RecordBuffer {
 public:
  ~RecordBufferImpl() { record_writer_.Close(); }

  absl::Status AddRecord(const DataRecordStruct& record) override {
    if (!record_writer_.WriteRecord(
            ToStringView(ToFlatBufferBuilder(record)))) {
      return record_writer_.status();
    }
    return absl::OkStatus();
  }

  absl::Status Flush() override {
    if (!record_writer_.Flush()) {
      return record_writer_.status();
    }
    return absl::OkStatus();
  }

  std::istream* RecordStream() override { return record_stream_.get(); }

  static std::unique_ptr<RecordBuffer> Create() {
    auto record_stream = std::make_unique<std::stringstream>();
    riegeli::RecordWriterBase::Options options;
    options.set_uncompressed();
    auto record_writer =
        riegeli::RecordWriter<riegeli::OStreamWriter<std::stringstream*>>(
            riegeli::OStreamWriter(record_stream.get()), options);
    return absl::WrapUnique(new RecordBufferImpl(std::move(record_stream),
                                                 std::move(record_writer)));
  }

 private:
  RecordBufferImpl(
      std::unique_ptr<std::stringstream> record_stream,
      riegeli::RecordWriter<riegeli::OStreamWriter<std::stringstream*>>
          record_writer)
      : record_stream_(std::move(record_stream)),
        record_writer_(std::move(record_writer)) {}

  std::unique_ptr<std::stringstream> record_stream_;
  riegeli::RecordWriter<riegeli::OStreamWriter<std::stringstream*>>
      record_writer_;
};

absl::Status IsWithinBounds(int shard_id, int num_shards) {
  if (shard_id < 0 || shard_id >= num_shards) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Shard id: %d is out of range: [%d, %d)", shard_id, 0, num_shards));
  }
  return absl::OkStatus();
}

}  // namespace

ShardedRecordBuffer::ShardedRecordBuffer(
    ShardingFunction sharding_func,
    std::vector<std::unique_ptr<RecordBuffer>> shard_buffers)
    : sharding_func_(std::move(sharding_func)),
      shard_buffers_(std::move(shard_buffers)) {}

absl::StatusOr<std::unique_ptr<ShardedRecordBuffer>>
ShardedRecordBuffer::Create(int num_shards, ShardingFunction sharding_func) {
  if (num_shards <= 0) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Number of shards: %d must be greater than 0", num_shards));
  }
  std::vector<std::unique_ptr<RecordBuffer>> shard_stores;
  shard_stores.reserve(num_shards);
  for (int shard_id = 0; shard_id < num_shards; shard_id++) {
    shard_stores.push_back(RecordBufferImpl::Create());
  }
  return absl::WrapUnique(new ShardedRecordBuffer(std::move(sharding_func),
                                                  std::move(shard_stores)));
}

absl::StatusOr<std::istream*> ShardedRecordBuffer::GetShardRecordStream(
    int shard_id) {
  if (auto status = IsWithinBounds(shard_id, shard_buffers_.size());
      !status.ok()) {
    return status;
  }
  return shard_buffers_[shard_id]->RecordStream();
}

absl::Status ShardedRecordBuffer::AddRecord(
    const DataRecordStruct& data_record) {
  if (std::holds_alternative<KeyValueMutationRecordStruct>(
          data_record.record)) {
    auto kv_record = std::get<KeyValueMutationRecordStruct>(data_record.record);
    auto shard_id =
        sharding_func_.GetShardNumForKey(kv_record.key, shard_buffers_.size());
    return shard_buffers_[shard_id]->AddRecord(data_record);
  }
  return absl::OkStatus();
}

absl::Status ShardedRecordBuffer::Flush(int shard_id) {
  if (shard_id < 0) {
    for (const auto& buffer : shard_buffers_) {
      if (auto status = buffer->Flush(); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }
  if (auto status = IsWithinBounds(shard_id, shard_buffers_.size());
      !status.ok()) {
    return status;
  }
  return shard_buffers_[shard_id]->Flush();
}

}  // namespace kv_server
