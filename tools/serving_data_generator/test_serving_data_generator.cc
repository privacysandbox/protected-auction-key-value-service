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
#include "absl/strings/substitute.h"
#include "glog/logging.h"
#include "google/protobuf/text_format.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/filename_utils.h"
#include "public/data_loading/records_utils.h"
#include "public/data_loading/riegeli_metadata.pb.h"
#include "riegeli/bytes/ostream_writer.h"
#include "riegeli/records/record_writer.h"

ABSL_FLAG(std::string, key, "foo", "Specify the key for lookups");
ABSL_FLAG(int, value_size, 10, "Specify the size of value for the key");
ABSL_FLAG(std::string, output_dir, "", "Output file directory");
ABSL_FLAG(int, num_records, 5, "Number of records to generate");
ABSL_FLAG(int64_t, timestamp, absl::ToUnixMicros(absl::Now()),
          "Record timestamp");
ABSL_FLAG(bool, generate_set_record, false,
          "Whether to generate set record or not");
ABSL_FLAG(int, num_values_in_set, 10,
          "Number of values in the set to generate");

using kv_server::DataRecordStruct;
using kv_server::KeyValueMutationRecordStruct;
using kv_server::KeyValueMutationType;
using kv_server::KVFileMetadata;
using kv_server::ToDeltaFileName;
using kv_server::ToFlatBufferBuilder;
using kv_server::ToStringView;

void WriteKeyValueRecords(std::string_view key, int value_size,
                          riegeli::RecordWriterBase& writer) {
  const int repetition = absl::GetFlag(FLAGS_num_records);
  int64_t timestamp = absl::GetFlag(FLAGS_timestamp);
  std::string query(" ");
  for (int i = 0; i < repetition; ++i) {
    const std::string value(value_size, 'A' + (i % 50));
    const std::string actual_key = absl::StrCat(key, i);
    auto kv_record = KeyValueMutationRecordStruct{
        KeyValueMutationType::Update, timestamp++, actual_key, value};
    writer.WriteRecord(ToStringView(
        ToFlatBufferBuilder(DataRecordStruct{.record = std::move(kv_record)})));
    absl::StrAppend(&query, "\"", actual_key, "\"", ", ");
  }
  LOG(INFO) << "Print keys to query " << query;
  LOG(INFO) << "write done";
}

void WriteKeyValueSetRecords(std::string_view key, int value_size,
                             riegeli::RecordWriterBase& writer) {
  const int repetition = absl::GetFlag(FLAGS_num_records);
  int64_t timestamp = absl::GetFlag(FLAGS_timestamp);
  const int num_values_in_set = absl::GetFlag(FLAGS_num_values_in_set);
  std::string query(" ");
  for (int i = 0; i < repetition; ++i) {
    std::vector<std::string> set_copy;
    for (int j = 0; j < num_values_in_set; ++j) {
      const std::string value(value_size, 'A' + (j % 50));
      set_copy.emplace_back(
          absl::StrCat(value, std::to_string(std::rand() % num_values_in_set)));
    }
    std::vector<std::string_view> set;
    for (const auto& v : set_copy) {
      set.emplace_back(v);
    }
    absl::StrAppend(&query, absl::StrCat(key, i), " | ");
    KeyValueMutationRecordStruct record;
    record.value = set;
    record.mutation_type = KeyValueMutationType::Update;
    record.logical_commit_time = timestamp++;
    record.key = absl::StrCat(key, i);
    writer.WriteRecord(ToStringView(
        ToFlatBufferBuilder(DataRecordStruct{.record = std::move(record)})));
  }
  LOG(INFO) << "Example set query for all keys" << query;
  LOG(INFO) << "write done for set records";
}

int main(int argc, char** argv) {
  const std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  google::InitGoogleLogging(argv[0]);
  const std::string output_dir = absl::GetFlag(FLAGS_output_dir);

  auto write_records = [](std::ostream* os) {
    const std::string key = absl::GetFlag(FLAGS_key);
    const int value_size = absl::GetFlag(FLAGS_value_size);

    auto os_writer = riegeli::OStreamWriter(os);
    riegeli::RecordWriterBase::Options options;
    options.set_uncompressed();
    riegeli::RecordsMetadata metadata;
    KVFileMetadata file_metadata;

    *metadata.MutableExtension(kv_server::kv_file_metadata) = file_metadata;
    options.set_metadata(std::move(metadata));
    auto record_writer = riegeli::RecordWriter(std::move(os_writer), options);
    if (absl::GetFlag(FLAGS_generate_set_record)) {
      WriteKeyValueSetRecords(key, value_size, record_writer);
    } else {
      WriteKeyValueRecords(key, value_size, record_writer);
    }

    record_writer.Close();
  };

  if (output_dir == "-") {
    LOG(INFO) << "Writing records to console";

    write_records(&std::cout);
  } else {
    absl::Time now = absl::Now();
    if (const auto maybe_name = ToDeltaFileName(absl::ToUnixMicros(now));
        !maybe_name.ok()) {
      LOG(ERROR) << "Unable to construct file name: " << maybe_name.status();
      return -1;
    } else {
      const std::string outfile =
          absl::StrCat(output_dir, "/", maybe_name.value());
      LOG(INFO) << "Writing records to " << outfile;

      std::ofstream ofs(outfile);
      write_records(&ofs);
      ofs.close();
    }
  }
  return 0;
}
