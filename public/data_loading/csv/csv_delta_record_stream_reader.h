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

#include <utility>
#include <vector>

#include "public/data_loading/csv/constants.h"
#include "public/data_loading/readers/delta_record_reader.h"
#include "public/data_loading/records_utils.h"
#include "riegeli/bytes/istream_reader.h"
#include "riegeli/csv/csv_reader.h"

namespace kv_server {

// A `CsvDeltaRecordStreamReader` reads CSV records as
// `DataRecordStruct` records from a `std::iostream` or
// `std::istream` with CSV formatted data.
//
// A `CsvDeltaRecordStreamReader` can be used to read records as follows:
// ```
// std::ifstream csv_file(my_filename);
// CsvDeltaRecordStreamReader record_reader(csv_file);
// absl::Status status = record_reader.ReadRecords(
//  [](const DataRecordStruct& record) {
//    UseRecord(record);
//    return absl::OkStatus();
//  }
// );
// ```
//
// The record reader has the following default options, which can be overriden
// by specifying `Options` when initializing the record reader.
//
// - `record_type` (Default `DataRecordType::kKeyValueMutationRecord`):
//   (1) If DataRecordType::kKeyValueMutationRecord, records are assumed to be
//   key value mutation records with the following header:
//   ["mutation_type", "logical_commit_time", "key", "value", "value_type"]`.
//
//   (2) If DataRecordType::kUserDefinedFunctionsConfig, records are assumed to
//   be user-defined function configs with the following header:
//   `["code_snippet", "handler_name", "language", "logical_commit_time",
//   "version"]`.
//
//  (3) If DataRecordType::kShardMappingRecord, records are assumed to
//   be shard mapping records with the following header:
//   `["logical_shard", "physical_shard"]`.
//
// - `field_separator`: CSV delimiter
//   Default ','.
//
// - `value_separator`: For set values, the delimiter for values in a set.
//   Default `|`.

template <typename SrcStreamT = std::iostream>
class CsvDeltaRecordStreamReader : public DeltaRecordReader {
 public:
  struct Options {
    char field_separator = ',';
    // Used as a separator for set value elements.
    char value_separator = '|';
    DataRecordType record_type = DataRecordType::kKeyValueMutationRecord;
  };

  CsvDeltaRecordStreamReader(SrcStreamT& src_stream,
                             Options options = Options());
  ~CsvDeltaRecordStreamReader() { record_reader_.Close(); }
  CsvDeltaRecordStreamReader(const CsvDeltaRecordStreamReader&) = delete;
  CsvDeltaRecordStreamReader& operator=(const CsvDeltaRecordStreamReader&) =
      delete;

  absl::Status ReadRecords(const std::function<absl::Status(DataRecordStruct)>&
                               record_callback) override;
  bool IsOpen() const override { return record_reader_.is_open(); };
  absl::Status Status() const override { return record_reader_.status(); }

 private:
  Options options_;
  riegeli::CsvReader<riegeli::IStreamReader<SrcStreamT*>> record_reader_;
};

namespace internal {
absl::StatusOr<DataRecordStruct> MakeDeltaFileRecordStruct(
    const riegeli::CsvRecord& csv_record, const DataRecordType& record_type,
    char value_separator);

template <typename DestStreamT>
riegeli::CsvReaderBase::Options GetRecordReaderOptions(
    const typename CsvDeltaRecordStreamReader<DestStreamT>::Options& options) {
  riegeli::CsvReaderBase::Options reader_options;
  reader_options.set_field_separator(options.field_separator);

  std::vector<std::string_view> header;
  switch (options.record_type) {
    case DataRecordType::kKeyValueMutationRecord:
      header =
          std::vector<std::string_view>(kKeyValueMutationRecordHeader.begin(),
                                        kKeyValueMutationRecordHeader.end());
      break;
    case DataRecordType::kUserDefinedFunctionsConfig:
      header = std::vector<std::string_view>(
          kUserDefinedFunctionsConfigHeader.begin(),
          kUserDefinedFunctionsConfigHeader.end());
      break;
    case DataRecordType::kShardMappingRecord:
      header = std::vector<std::string_view>(kShardMappingRecordHeader.begin(),
                                             kShardMappingRecordHeader.end());
      break;
  }
  reader_options.set_required_header(std::move(header));
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
    const std::function<absl::Status(DataRecordStruct)>& record_callback) {
  riegeli::CsvRecord csv_record;
  absl::Status overall_status;
  while (record_reader_.ReadRecord(csv_record)) {
    absl::StatusOr<DataRecordStruct> delta_record =
        internal::MakeDeltaFileRecordStruct(csv_record, options_.record_type,
                                            options_.value_separator);
    if (!delta_record.ok()) {
      overall_status.Update(delta_record.status());
      continue;
    }
    overall_status.Update(record_callback(*delta_record));
  }
  overall_status.Update(record_reader_.status());
  return overall_status;
}

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_CSV_CSV_DELTA_RECORD_STREAM_READER_H_
