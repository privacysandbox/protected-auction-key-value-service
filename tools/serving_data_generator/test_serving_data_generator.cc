// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <fstream>
#include <iostream>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/filename_utils.h"
#include "public/data_loading/records_utils.h"
#include "public/data_loading/riegeli_metadata.pb.h"
#include "public/sharding/sharding_function.h"
#include "riegeli/bytes/ostream_writer.h"
#include "riegeli/records/record_writer.h"

ABSL_FLAG(std::string, key, "foo", "Specify the key prefix for lookups");
ABSL_FLAG(int, value_size, 10, "Specify the size of value for the key");
ABSL_FLAG(std::string, output_dir, "", "Output file directory");
ABSL_FLAG(std::string, output_filename, "", "Output file name");
ABSL_FLAG(int, num_records, 5, "Number of records to generate");
ABSL_FLAG(int, num_shards, 1, "Number of shards");
ABSL_FLAG(int, shard_number, 0, "Shard number");
ABSL_FLAG(int64_t, timestamp, absl::ToUnixMicros(absl::Now()),
          "Record timestamp. Increases by 1 for each record.");
ABSL_FLAG(bool, generate_string_set_records, false,
          "Whether to generate string set records or not");
ABSL_FLAG(bool, generate_int_set_records, false,
          "Whether to generate int set records or not");
ABSL_FLAG(bool, use_random_elements, false,
          "Whether to select set elements from a random range. If false, "
          "elements are selected from generated keys.");
ABSL_FLAG(std::string, set_key_prefix, "set",
          "Specify the set value key prefix for lookups");
ABSL_FLAG(int, num_values_in_set, 10,
          "Number of values in the set to generate");
ABSL_FLAG(int, num_set_records, 5, "Number of records to generate");
ABSL_FLAG(uint32_t, range_min, 0, "Minimum element in set records.");
ABSL_FLAG(uint32_t, range_max, 2147483647, "Maximum element in set records.");

using kv_server::DataRecordStruct;
using kv_server::KeyValueMutationRecordStruct;
using kv_server::KeyValueMutationType;
using kv_server::KVFileMetadata;
using kv_server::ShardingMetadata;
using kv_server::ToDeltaFileName;
using kv_server::ToFlatBufferBuilder;
using kv_server::ToStringView;

const std::array<std::string, 3> kSetOps = {" - ", " | ", " & "};

void WriteKeyValueRecord(std::string_view key, std::string_view value,
                         int64_t logical_commit_time,
                         riegeli::RecordWriterBase& writer) {
  auto kv_record = KeyValueMutationRecordStruct{
      KeyValueMutationType::Update, logical_commit_time, key, value};
  writer.WriteRecord(ToStringView(
      ToFlatBufferBuilder(DataRecordStruct{.record = std::move(kv_record)})));
}

std::vector<std::string> WriteKeyValueRecords(
    std::string_view key, int value_size, int64_t timestamp,
    riegeli::RecordWriterBase& writer) {
  const int num_records = absl::GetFlag(FLAGS_num_records);
  const int64_t num_shards = absl::GetFlag(FLAGS_num_shards);
  const int64_t current_shard_number = absl::GetFlag(FLAGS_shard_number);
  std::vector<std::string> keys;
  std::string query(" ");
  for (int i = 0; i < num_records; ++i) {
    const std::string value(value_size, 'A' + (i % 50));
    const std::string actual_key = absl::StrCat(key, i);
    keys.emplace_back(actual_key);
    if (num_shards > 1) {
      kv_server::ShardingFunction sharding_func("");
      auto shard_number =
          sharding_func.GetShardNumForKey(actual_key, num_shards);
      if (shard_number != current_shard_number) {
        continue;
      }
    }
    WriteKeyValueRecord(actual_key, value, timestamp++, writer);
    absl::StrAppend(&query, "\"", actual_key, "\"", ", ");
  }
  LOG(INFO) << "Print keys to query " << query;
  LOG(INFO) << "write done";
  return keys;
}

void WriteKeyValueSetRecords(const std::vector<std::string>& keys,
                             std::string_view set_key_prefix, int64_t timestamp,
                             riegeli::RecordWriterBase& writer) {
  const int num_set_records = absl::GetFlag(FLAGS_num_set_records);
  const int num_values_in_set = absl::GetFlag(FLAGS_num_values_in_set);
  std::string query(" ");
  for (int i = 0; i < num_set_records; ++i) {
    std::vector<uint32_t> uint32_set;
    std::vector<std::string> string_set;
    for (int j = 0; j < num_values_in_set; ++j) {
      // Add a random element
      std::srand(absl::GetCurrentTimeNanos());
      auto element = absl::GetFlag(FLAGS_range_min) +
                     (std::rand() % (absl::GetFlag(FLAGS_range_max) -
                                     absl::GetFlag(FLAGS_range_min)));
      if (absl::GetFlag(FLAGS_generate_int_set_records)) {
        uint32_set.emplace_back(element);
      }
      if (absl::GetFlag(FLAGS_generate_string_set_records)) {
        if (absl::GetFlag(FLAGS_use_random_elements)) {
          string_set.emplace_back(absl::StrCat(element));
        } else {
          string_set.emplace_back(keys[std::rand() % (keys.size() - 1)]);
        }
      }
    }
    auto set_value_key = absl::StrCat(set_key_prefix, i);
    KeyValueMutationRecordStruct record;
    record.mutation_type = KeyValueMutationType::Update;
    record.logical_commit_time = timestamp++;
    record.key = set_value_key;
    if (absl::GetFlag(FLAGS_generate_int_set_records)) {
      record.value = uint32_set;
      writer.WriteRecord(ToStringView(
          ToFlatBufferBuilder(DataRecordStruct{.record = std::move(record)})));
    }
    if (absl::GetFlag(FLAGS_generate_string_set_records)) {
      std::vector<std::string_view> string_set_view;
      for (const auto& v : string_set) {
        string_set_view.emplace_back(v);
      }
      record.value = string_set_view;
      writer.WriteRecord(ToStringView(
          ToFlatBufferBuilder(DataRecordStruct{.record = std::move(record)})));
    }
    absl::StrAppend(&query, set_value_key,
                    kSetOps[std::rand() % kSetOps.size()]);
  }
  LOG(INFO) << "Example set query for all keys" << query;
  LOG(INFO) << "write done for set records";
}

KVFileMetadata GetKVFileMetadata() {
  KVFileMetadata file_metadata;
  const int num_shards = absl::GetFlag(FLAGS_num_shards);
  if (num_shards > 1) {
    const int shard_number = absl::GetFlag(FLAGS_shard_number);
    ShardingMetadata sharding_metadata;
    sharding_metadata.set_shard_num(shard_number);
    *file_metadata.mutable_sharding_metadata() = sharding_metadata;
  }
  return file_metadata;
}

int main(int argc, char** argv) {
  const std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  const std::string output_dir = absl::GetFlag(FLAGS_output_dir);
  std::string output_filename = absl::GetFlag(FLAGS_output_filename);

  auto write_records = [](std::ostream* os) {
    const std::string key = absl::GetFlag(FLAGS_key);
    const int value_size = absl::GetFlag(FLAGS_value_size);
    const std::string set_key_prefix = absl::GetFlag(FLAGS_set_key_prefix);
    int64_t timestamp = absl::GetFlag(FLAGS_timestamp);

    auto os_writer = riegeli::OStreamWriter(os);
    riegeli::RecordWriterBase::Options options;
    options.set_uncompressed();
    riegeli::RecordsMetadata metadata;
    *metadata.MutableExtension(kv_server::kv_file_metadata) =
        GetKVFileMetadata();
    options.set_metadata(std::move(metadata));
    auto record_writer = riegeli::RecordWriter(std::move(os_writer), options);
    const auto keys =
        WriteKeyValueRecords(key, value_size, timestamp, record_writer);
    if (absl::GetFlag(FLAGS_generate_int_set_records) ||
        absl::GetFlag(FLAGS_generate_string_set_records)) {
      WriteKeyValueSetRecords(keys, set_key_prefix, timestamp++, record_writer);
    }
    record_writer.Close();
  };

  if (output_dir == "-") {
    LOG(INFO) << "Writing records to console";
    write_records(&std::cout);
    return 0;
  }

  if (output_filename.empty()) {
    absl::Time now = absl::Now();
    if (const auto maybe_name = ToDeltaFileName(absl::ToUnixMicros(now));
        !maybe_name.ok()) {
      LOG(ERROR) << "Unable to construct file name: " << maybe_name.status();
      return -1;
    } else {
      output_filename = *maybe_name;
    }
  }

  const std::string outfile = absl::StrCat(output_dir, "/", output_filename);
  LOG(INFO) << "Writing records to " << outfile;

  std::ofstream ofs(outfile);
  write_records(&ofs);
  ofs.close();

  return 0;
}
