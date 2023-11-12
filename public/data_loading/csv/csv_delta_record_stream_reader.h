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

#ifndef PUBLIC_DATA_LOADING_CSV_CSV_DELTA_RECORD_STREAM_READER_H_
#define PUBLIC_DATA_LOADING_CSV_CSV_DELTA_RECORD_STREAM_READER_H_

#include <string>
#include <utility>
#include <vector>

#include "glog/logging.h"
#include "public/data_loading/csv/constants.h"
#include "public/data_loading/readers/delta_record_reader.h"
#include "public/data_loading/record_utils.h"
#include "riegeli/bytes/istream_reader.h"
#include "riegeli/csv/csv_reader.h"

namespace kv_server {

// A `CsvDeltaRecordStreamReader` reads CSV records as
// `DataRecord` records from a `std::iostream` or
// `std::istream` with CSV formatted data.
//
// A `CsvDeltaRecordStreamReader` can be used to read records as follows:
// ```
// std::ifstream csv_file(my_filename);
// CsvDeltaRecordStreamReader record_reader(csv_file);
// absl::Status status = record_reader.ReadRecords(
//  [](const DataRecord& record) {
//    UseRecord(record);
//    return absl::OkStatus();
//  }
// );
// ```
//
// The record reader has the following default options, which can be overridden
// by specifying `Options` when initializing the record reader.
//
// - `record_type` (Default `Record::KeyValueMutationRecord`):
//   (1) If Record::KeyValueMutationRecord, records are assumed to be
//   key value mutation records with the following header:
//   ["mutation_type", "logical_commit_time", "key", "value", "value_type"]`.
//
//   (2) If Record::UserDefinedFunctionsConfig, records are assumed to
//   be user-defined function configs with the following header:
//   `["code_snippet", "handler_name", "language", "logical_commit_time",
//   "version"]`.
//
//  (3) If Record::ShardMappingRecord, records are assumed to
//   be shard mapping records with the following header:
//   `["logical_shard", "physical_shard"]`.
//
// - `field_separator`: CSV delimiter
//   Default ','.
//
// - `value_separator`: For set values, the delimiter for values in a set.
//   Default `|`.

struct CsvDeltaRecordStreamReaderOptions {
  char field_separator = ',';
  // Used as a separator for set value elements.
  char value_separator = '|';
  Record record_type = Record::KeyValueMutationRecord;
  CsvEncoding csv_encoding = CsvEncoding::kPlaintext;
};

template <typename SrcStreamT = std::iostream>
class CsvDeltaRecordStreamReader : public DeltaRecordReader {
 public:
  using Options = CsvDeltaRecordStreamReaderOptions;
  CsvDeltaRecordStreamReader(SrcStreamT& src_stream,
                             Options options = Options());
  ~CsvDeltaRecordStreamReader() { record_reader_.Close(); }
  CsvDeltaRecordStreamReader(const CsvDeltaRecordStreamReader&) = delete;
  CsvDeltaRecordStreamReader& operator=(const CsvDeltaRecordStreamReader&) =
      delete;

  absl::Status ReadRecords(const std::function<absl::Status(const DataRecord&)>&
                               record_callback) override;
  absl::Status ReadRecords(const std::function<absl::Status(DataRecordStruct)>&
                               record_callback) override {
    return absl::UnimplementedError(
        "CSV reader is updated to use newer data structures");
  }
  bool IsOpen() const override { return record_reader_.is_open(); };
  absl::Status Status() const override { return record_reader_.status(); }

 private:
  Options options_;
  riegeli::CsvReader<riegeli::IStreamReader<SrcStreamT*>> record_reader_;
};

namespace internal {

absl::StatusOr<DataRecordT> MakeDeltaFileRecordStruct(
    const riegeli::CsvRecord& csv_record,
    const CsvDeltaRecordStreamReaderOptions& options);

template <typename DestStreamT>
riegeli::CsvReaderBase::Options GetRecordReaderOptions(
    const typename CsvDeltaRecordStreamReader<DestStreamT>::Options& options) {
  LOG(INFO) << "Building CSV Record reader options.";
  riegeli::CsvReaderBase::Options reader_options;
  reader_options.set_field_separator(options.field_separator);

  std::vector<std::string_view> header;
  switch (options.record_type) {
    case Record::KeyValueMutationRecord:
      header =
          std::vector<std::string_view>(kKeyValueMutationRecordHeader.begin(),
                                        kKeyValueMutationRecordHeader.end());
      break;
    case Record::UserDefinedFunctionsConfig:
      header = std::vector<std::string_view>(
          kUserDefinedFunctionsConfigHeader.begin(),
          kUserDefinedFunctionsConfigHeader.end());
      break;
    case Record::ShardMappingRecord:
      header = std::vector<std::string_view>(kShardMappingRecordHeader.begin(),
                                             kShardMappingRecordHeader.end());
      break;
    default:
      LOG(ERROR) << "Unable to set CSV reader header";
  }
  reader_options.set_required_header(riegeli::CsvHeader(std::move(header)));
  return reader_options;
}
}  // namespace internal

template <typename SrcStreamT>
CsvDeltaRecordStreamReader<SrcStreamT>::CsvDeltaRecordStreamReader(
    SrcStreamT& src_stream, Options options)
    : options_(std::move(options)),
      record_reader_(riegeli::CsvReader<riegeli::IStreamReader<SrcStreamT*>>(
          riegeli::IStreamReader(&src_stream),
          internal::GetRecordReaderOptions<SrcStreamT>(options_))) {}

template <typename SrcStreamT>
absl::Status CsvDeltaRecordStreamReader<SrcStreamT>::ReadRecords(
    const std::function<absl::Status(const DataRecord&)>& record_callback) {
  VLOG(9) << "Reading CSV records. Record type: "
          << EnumNameRecord(options_.record_type);
  riegeli::CsvRecord csv_record;
  absl::Status overall_status;
  while (record_reader_.ReadRecord(csv_record)) {
    VLOG(9) << "Read CSV record: " << csv_record.DebugString();
    absl::StatusOr<DataRecordT> delta_record =
        internal::MakeDeltaFileRecordStruct(csv_record, options_);
    if (!delta_record.ok()) {
      overall_status.Update(delta_record.status());
      continue;
    }
    VLOG(9) << "Converted CSV record to C++ DataRecord: " << *delta_record;

    auto [builder, serialized_string_view] = Serialize(*delta_record);
    overall_status.Update(record_callback(
        *flatbuffers::GetRoot<DataRecord>(serialized_string_view.data())));
  }
  overall_status.Update(record_reader_.status());
  return overall_status;
}

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_CSV_CSV_DELTA_RECORD_STREAM_READER_H_
