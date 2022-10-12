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

#ifndef TOOLS_DATA_CLI_CSV_CSV_DELTA_RECORD_STREAM_READER_H_
#define TOOLS_DATA_CLI_CSV_CSV_DELTA_RECORD_STREAM_READER_H_

#include <utility>
#include <vector>

#include "public/data_loading/csv/constants.h"
#include "public/data_loading/readers/delta_record_reader.h"
#include "public/data_loading/records_utils.h"
#include "riegeli/bytes/istream_reader.h"
#include "riegeli/csv/csv_reader.h"

namespace fledge::kv_server {

// A `CsvDeltaRecordStreamReader` reads CSV records as `DeltaFileRecordStruct`
// records from a `std::iostream` or `std::istream` with CSV formatted data.
//
// A `CsvDeltaRecordStreamReader` can be used to read records as follows:
// ```
// std::ifstream csv_file(my_filename);
// CsvDeltaRecordStreamReader record_reader(csv_file);
// absl::Status status = record_reader.ReadRecords(
//  [](const DeltaFileRecordStruct& record) {
//    UseRecord(record);
//    return absl::OkStatus();
//  }
// );
// ```
// The default delimiter is assumed to be a ',' and records are assumed to have
// the following fields:
// `header = ["mutation_type", "logical_commit_time", "key", "subkey", "value"]`
// These defaults can be overriden by specifying `Options` when initializing the
// record reader.
template <typename SrcStreamT = std::iostream>
class CsvDeltaRecordStreamReader : public DeltaRecordReader {
 public:
  struct Options {
    char field_separator = ',';
    std::vector<std::string_view> header = {kKeyColumn, kSubKeyColumn,
                                            kLogicalCommitTimeColumn,
                                            kMutationTypeColumn, kValueColumn};
  };

  CsvDeltaRecordStreamReader(SrcStreamT& src_stream,
                             Options options = Options());
  ~CsvDeltaRecordStreamReader() { record_reader_.Close(); }
  CsvDeltaRecordStreamReader(const CsvDeltaRecordStreamReader&) = delete;
  CsvDeltaRecordStreamReader& operator=(const CsvDeltaRecordStreamReader&) =
      delete;

  absl::Status ReadRecords(
      const std::function<absl::Status(DeltaFileRecordStruct)>& record_callback)
      override;
  bool IsOpen() const override { return record_reader_.is_open(); };
  absl::Status Status() const override { return record_reader_.status(); }

 private:
  Options options_;
  riegeli::CsvReader<riegeli::IStreamReader<SrcStreamT*>> record_reader_;
};

namespace internal {
absl::StatusOr<DeltaFileRecordStruct> MakeDeltaFileRecordStruct(
    const riegeli::CsvRecord& csv_record);

template <typename DestStreamT>
riegeli::CsvReaderBase::Options GetRecordReaderOptions(
    const typename CsvDeltaRecordStreamReader<DestStreamT>::Options& options) {
  riegeli::CsvReaderBase::Options reader_options;
  reader_options.set_field_separator(options.field_separator);
  reader_options.set_required_header(options.header);
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
    const std::function<absl::Status(DeltaFileRecordStruct)>& record_callback) {
  riegeli::CsvRecord csv_record;
  absl::Status overall_status;
  while (record_reader_.ReadRecord(csv_record)) {
    absl::StatusOr<DeltaFileRecordStruct> delta_record =
        internal::MakeDeltaFileRecordStruct(csv_record);
    if (!delta_record.ok()) {
      overall_status.Update(delta_record.status());
      continue;
    }
    overall_status.Update(record_callback(*delta_record));
  }
  overall_status.Update(record_reader_.status());
  return overall_status;
}

}  // namespace fledge::kv_server

#endif  // TOOLS_DATA_CLI_CSV_CSV_DELTA_RECORD_STREAM_READER_H_
