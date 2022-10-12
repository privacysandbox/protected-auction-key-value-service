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

#ifndef TOOLS_DATA_CLI_CSV_CSV_DELTA_RECORD_STREAM_WRITER_H_
#define TOOLS_DATA_CLI_CSV_CSV_DELTA_RECORD_STREAM_WRITER_H_

#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "public/data_loading/csv/constants.h"
#include "public/data_loading/writers/delta_record_writer.h"
#include "riegeli/bytes/ostream_writer.h"
#include "riegeli/csv/csv_record.h"
#include "riegeli/csv/csv_writer.h"

namespace fledge::kv_server {

// A `CsvDeltaRecordStreamWriter` writes `DeltaFileRecordStruct` records as CSV
// records to a `std::iostream` or `std::ostream.` or other subclasses of these
// two streams.
//
// A `CsvDeltaRecordStreamWriter` can be used to write CSV records as follows:
// ```
// std::stringstream ostream;
// CsvDeltaRecordStreamWriter record_writer(ostream);
// DeltaFileRecordStruct record = ...;
// if (absl::Status status = record_writer.WriteRecord(); !status.ok()) {
//   LOG(ERROR) << "Failed to write record: " << status;
// }
// ```
template <typename DestStreamT = std::iostream>
class CsvDeltaRecordStreamWriter : public DeltaRecordWriter {
 public:
  struct Options : public DeltaRecordWriter::Options {
    char field_separator = ',';
    std::vector<std::string_view> header = {kKeyColumn, kSubKeyColumn,
                                            kLogicalCommitTimeColumn,
                                            kMutationTypeColumn, kValueColumn};
  };

  CsvDeltaRecordStreamWriter(DestStreamT& dest_stream,
                             Options options = Options());
  ~CsvDeltaRecordStreamWriter() { Close(); }
  CsvDeltaRecordStreamWriter(const CsvDeltaRecordStreamWriter&) = delete;
  CsvDeltaRecordStreamWriter& operator=(const CsvDeltaRecordStreamWriter&) =
      delete;

  absl::Status WriteRecord(const DeltaFileRecordStruct& record) override;
  absl::Status Flush() override;
  const Options& GetOptions() const override { return options_; }
  void Close() override { record_writer_.Close(); }
  bool IsOpen() override { return record_writer_.is_open(); }
  absl::Status Status() override { return record_writer_.status(); }

 private:
  Options options_;
  riegeli::CsvWriter<riegeli::OStreamWriter<DestStreamT*>> record_writer_;
};

namespace internal {
absl::StatusOr<riegeli::CsvRecord> MakeCsvRecord(
    const DeltaFileRecordStruct& record,
    const std::vector<std::string_view>& header);

template <typename DestStreamT>
riegeli::CsvWriterBase::Options GetRecordWriterOptions(
    const typename CsvDeltaRecordStreamWriter<DestStreamT>::Options& options) {
  riegeli::CsvWriterBase::Options writer_options;
  writer_options.set_field_separator(options.field_separator);
  writer_options.set_header(options.header);
  return writer_options;
}
}  // namespace internal

template <typename DestStreamT>
CsvDeltaRecordStreamWriter<DestStreamT>::CsvDeltaRecordStreamWriter(
    DestStreamT& dest_stream, Options options)
    : options_(std::move(options)),
      record_writer_(riegeli::CsvWriter<riegeli::OStreamWriter<DestStreamT*>>(
          riegeli::OStreamWriter(&dest_stream),
          internal::GetRecordWriterOptions<DestStreamT>(options_))) {}

template <typename DestStreamT>
absl::Status CsvDeltaRecordStreamWriter<DestStreamT>::WriteRecord(
    const DeltaFileRecordStruct& record) {
  absl::StatusOr<riegeli::CsvRecord> csv_record =
      internal::MakeCsvRecord(record, options_.header);
  if (!csv_record.ok()) {
    return csv_record.status();
  }
  if (!record_writer_.WriteRecord(*csv_record) && options_.recovery_function) {
    options_.recovery_function(record);
  }
  return record_writer_.status();
}

template <typename DestStreamT>
absl::Status CsvDeltaRecordStreamWriter<DestStreamT>::Flush() {
  record_writer_.dest_writer()->Flush();
  return record_writer_.status();
}

}  // namespace fledge::kv_server

#endif  // TOOLS_DATA_CLI_CSV_CSV_DELTA_RECORD_STREAM_WRITER_H_
