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

#include "tools/request_simulation/realtime_message_batcher.h"

#include <fstream>
#include <utility>

#include "absl/log/check.h"
#include "absl/strings/substitute.h"
#include "src/util/status_macro/status_macros.h"

namespace kv_server {
namespace {
DeltaRecordWriter::Options GetOptions(int num_shards, int shard_num) {
  DeltaRecordWriter::Options options;
  options.enable_compression = false;
  if (num_shards > 1) {
    KVFileMetadata file_metadata;
    ShardingMetadata sharding_metadata;
    sharding_metadata.set_shard_num(shard_num);
    *file_metadata.mutable_sharding_metadata() = std::move(sharding_metadata);
    options.metadata = std::move(file_metadata);
  }
  return options;
}
}  // namespace

RealtimeMessageBatcher::FileHandler::FileHandler(std::string file_name,
                                                 int file_descriptor)
    : file_name(file_name), file_descriptor_(file_descriptor) {}

RealtimeMessageBatcher::FileHandler::~FileHandler() {
  // Close and unlink, which will delete the tmp file.
  unlink(file_name.c_str());
  close(file_descriptor_);
}

absl::StatusOr<std::unique_ptr<RealtimeMessageBatcher::FileHandler>>
RealtimeMessageBatcher::FileHandler::Create() {
  char file_name_template[16] = "/tmp/fileXXXXXX";
  auto file_descriptor = mkstemp(file_name_template);
  if (file_descriptor == -1) {
    return absl::InternalError("Failed creating a temporary file.");
  }
  return std::make_unique<RealtimeMessageBatcher::FileHandler>(
      std::string(file_name_template), file_descriptor);
}

RealtimeMessageBatcher::RealtimeMessageBatcher(
    std::vector<DeltaRecordLimitingFileWriterWrapper> writer_wrappers,
    std::queue<RealtimeMessage>& realtime_messages, absl::Mutex& queue_mutex,
    int num_shards, int message_size_kb)
    : writer_wrappers_(std::move(writer_wrappers)),
      realtime_messages_(realtime_messages),
      mutex_(queue_mutex),
      num_shards_(num_shards),
      message_size_kb_(message_size_kb),
      key_sharder_(
          kv_server::KeySharder(kv_server::ShardingFunction{/*seed=*/""})) {}

DeltaRecordLimitingFileWriter& RealtimeMessageBatcher::GetWriter(
    int shard_num) const {
  CHECK(shard_num >= 0) << "shard_num must be >= 0";
  CHECK(shard_num < num_shards_) << "shard_num must be < num_shards_";
  if (num_shards_ > 1) {
    return *(writer_wrappers_[shard_num].file_writer);
  } else {
    return *(writer_wrappers_[0].file_writer);
  }
}

RealtimeMessage RealtimeMessageBatcher::GetMessage(int shard_num) const {
  auto file_stream =
      std::ifstream(writer_wrappers_[shard_num].file_handler->file_name);
  std::optional<int> shard_num_opt = std::nullopt;
  if (num_shards_ > 1) {
    shard_num_opt = shard_num;
  }
  RealtimeMessage rm{
      .message = absl::Base64Escape(
          std::string(std::istreambuf_iterator<char>(file_stream),
                      std::istreambuf_iterator<char>())),
      .shard_num = shard_num_opt,
  };
  return rm;
}

absl::Status RealtimeMessageBatcher::Insert(
    kv_server::KeyValueMutationRecordT key_value_mutation) {
  const int shard_num =
      key_sharder_.GetShardNumForKey(key_value_mutation.key, num_shards_)
          .shard_num;
  DataRecordT data_record;
  data_record.record.Set(std::move(key_value_mutation));
  auto write_result = GetWriter(shard_num).WriteRecord(data_record);
  if (write_result.ok()) {
    return absl::OkStatus();
  }
  if (!absl::IsResourceExhausted(write_result)) {
    VLOG(5) << "Failed to write to the file. Did not hit the size limit.";
    return write_result;
  }
  // This per shard queue got over the `message_size_kb` limit.
  // Batch all messages in a realtime message, tag it with a shard number, and
  // insert it into `realtime_messages_`.
  auto message = GetMessage(shard_num);
  {
    absl::MutexLock lock(&mutex_);
    realtime_messages_.push(std::move(message));
  }
  // recreate the writer, and the underlying file, since the old one is
  // exhausted.
  writer_wrappers_[shard_num].file_writer->Close();
  PS_ASSIGN_OR_RETURN(std::unique_ptr<FileHandler> file_handler,
                      FileHandler::Create());
  writer_wrappers_[shard_num].file_handler = std::move(file_handler);
  PS_ASSIGN_OR_RETURN(
      std::unique_ptr<kv_server::DeltaRecordLimitingFileWriter> record_writer,
      kv_server::DeltaRecordLimitingFileWriter::Create(
          writer_wrappers_[shard_num].file_handler->file_name,
          GetOptions(num_shards_, shard_num), message_size_kb_ * 1024));
  writer_wrappers_[shard_num].file_writer = std::move(record_writer);
  return absl::OkStatus();
}

RealtimeMessageBatcher::~RealtimeMessageBatcher() {
  for (int i = 0; i < num_shards_; i++) {
    writer_wrappers_[i].file_writer->Close();
    auto message = GetMessage(i);
    {
      absl::MutexLock lock(&mutex_);
      realtime_messages_.push(std::move(message));
    }
  }
}

absl::StatusOr<std::unique_ptr<RealtimeMessageBatcher>>
RealtimeMessageBatcher::Create(std::queue<RealtimeMessage>& realtime_messages,
                               absl::Mutex& queue_mutex, int num_shards,
                               int message_size_kb) {
  std::vector<DeltaRecordLimitingFileWriterWrapper> writer_wrappers;
  for (int i = 0; i < num_shards; i++) {
    PS_ASSIGN_OR_RETURN(std::unique_ptr<FileHandler> file_handler,
                        FileHandler::Create());
    DeltaRecordLimitingFileWriterWrapper ww{.file_handler =
                                                std::move(file_handler)};
    PS_ASSIGN_OR_RETURN(
        std::unique_ptr<kv_server::DeltaRecordLimitingFileWriter> record_writer,
        kv_server::DeltaRecordLimitingFileWriter::Create(
            ww.file_handler->file_name, GetOptions(num_shards, i),
            message_size_kb * 1024));
    ww.file_writer = std::move(record_writer);
    writer_wrappers.push_back(std::move(ww));
  }
  return absl::WrapUnique(
      new RealtimeMessageBatcher(std::move(writer_wrappers), realtime_messages,
                                 queue_mutex, num_shards, message_size_kb));
}

}  // namespace kv_server
