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

#ifndef TOOLS_BIDDING_AUCTION_DATA_GENERATOR_DELTA_KEY_VALUE_WRITER_H_
#define TOOLS_BIDDING_AUCTION_DATA_GENERATOR_DELTA_KEY_VALUE_WRITER_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "public/data_loading/writers/delta_record_writer.h"

namespace kv_server {
// Defines a class that writes key value pairs
// for the given logical_commit_time and mutation type to the delta output
// stream
class DeltaKeyValueWriter {
 public:
  static absl::StatusOr<std::unique_ptr<DeltaKeyValueWriter>> Create(
      std::ostream& output_stream);
  absl::Status Write(
      const absl::flat_hash_map<std::string, std::string>& key_value_map,
      int64_t logical_commit_time, DeltaMutationType mutation_type);

 private:
  explicit DeltaKeyValueWriter(
      std::unique_ptr<DeltaRecordWriter> delta_record_writer)
      : delta_record_writer_(std::move(delta_record_writer)) {}
  std::unique_ptr<DeltaRecordWriter> delta_record_writer_;
};

}  // namespace kv_server

#endif  // TOOLS_BIDDING_AUCTION_DATA_GENERATOR_DELTA_KEY_VALUE_WRITER_H_
