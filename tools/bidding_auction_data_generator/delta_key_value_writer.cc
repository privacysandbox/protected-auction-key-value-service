/*
 * Copyright 2022 Google LLC
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

#include "tools/bidding_auction_data_generator/delta_key_value_writer.h"

#include "public/constants.h"
#include "public/data_loading/writers/delta_record_stream_writer.h"

namespace kv_server {
absl::StatusOr<std::unique_ptr<DeltaKeyValueWriter>>
DeltaKeyValueWriter::Create(std::ostream& output_stream) {
  KVFileMetadata metadata;
  auto delta_record_writer = DeltaRecordStreamWriter<std::ostream>::Create(
      output_stream, DeltaRecordWriter::Options{.metadata = metadata});
  if (!delta_record_writer.ok()) {
    return delta_record_writer.status();
  }
  return absl::WrapUnique(
      new DeltaKeyValueWriter(std::move(*delta_record_writer)));
}

absl::Status DeltaKeyValueWriter::Write(
    const absl::flat_hash_map<std::string, std::string>& key_value_map,
    int64_t logical_commit_time, DeltaMutationType mutation_type) {
  for (const auto& [k, v] : key_value_map) {
    DeltaFileRecordStruct record_struct;
    record_struct.key = k;
    record_struct.value = v;
    record_struct.logical_commit_time = logical_commit_time;
    record_struct.mutation_type = mutation_type;

    if (const auto status = delta_record_writer_->WriteRecord(record_struct);
        !status.ok()) {
      LOG(ERROR) << "Failed to write a delta record with key: " << k
                 << " and value: " << v << status;
      return status;
    }
  }
  if (const auto status = delta_record_writer_->Flush(); !status.ok()) {
    LOG(ERROR) << "Failed to flush delta record writer";
    return status;
  }
  return absl::OkStatus();
}
}  // namespace kv_server
