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

#ifndef PUBLIC_DATA_LOADING_WRITERS_DELTA_RECORD_STREAM_WRITER_H_
#define PUBLIC_DATA_LOADING_WRITERS_DELTA_RECORD_STREAM_WRITER_H_

#include <memory>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "public/data_loading/record_utils.h"
#include "public/data_loading/records_utils.h"
#include "public/data_loading/writers/delta_record_writer.h"
#include "riegeli/bytes/ostream_writer.h"
#include "riegeli/records/record_writer.h"

namespace kv_server {

template <typename DestStreamT = std::iostream>
class DeltaRecordStreamWriter : public DeltaRecordWriter {
 public:
  ~DeltaRecordStreamWriter() override { Close(); }

  DeltaRecordStreamWriter(const DeltaRecordStreamWriter&) = delete;
  DeltaRecordStreamWriter& operator=(const DeltaRecordStreamWriter&) = delete;

  static absl::StatusOr<std::unique_ptr<DeltaRecordStreamWriter>> Create(
      DestStreamT& dest_stream, Options options);
  absl::Status WriteRecord(const DataRecordT& data_record) override;
  [[deprecated("Use corresponding DataRecordT-based function")]]
  absl::Status WriteRecord(const DataRecordStruct& record) override {
    return absl::UnimplementedError(
        "DeltaRecordStreamWriter is updated to use newer data structures");
  };
  const Options& GetOptions() const override { return options_; }
  absl::Status Flush() override;
  void Close() override { record_writer_->Close(); }
  bool IsOpen() override { return record_writer_->is_open(); }
  absl::Status Status() override { return record_writer_->status(); }

 private:
  DeltaRecordStreamWriter(DestStreamT& dest_stream, Options options);

  Options options_;
  std::unique_ptr<riegeli::RecordWriter<riegeli::OStreamWriter<DestStreamT*>>>
      record_writer_;
};

riegeli::RecordWriterBase::Options GetRecordWriterOptions(
    const DeltaRecordWriter::Options& options);

template <typename DestStreamT>
DeltaRecordStreamWriter<DestStreamT>::DeltaRecordStreamWriter(
    DestStreamT& dest_stream, Options options)
    : options_(std::move(options)),
      record_writer_(
          std::make_unique<
              riegeli::RecordWriter<riegeli::OStreamWriter<DestStreamT*>>>(
              riegeli::OStreamWriter(&dest_stream),
              GetRecordWriterOptions(options_))) {}

template <typename DestStreamT>
absl::StatusOr<std::unique_ptr<DeltaRecordStreamWriter<DestStreamT>>>
DeltaRecordStreamWriter<DestStreamT>::Create(DestStreamT& dest_stream,
                                             Options options) {
  return absl::WrapUnique(new DeltaRecordStreamWriter(dest_stream, options));
}

template <typename DestStreamT>
absl::Status DeltaRecordStreamWriter<DestStreamT>::WriteRecord(
    const DataRecordT& data_record) {
  auto [fbs_buffer, bytes_to_write] = Serialize(data_record);
  if (!record_writer_->WriteRecord(bytes_to_write) &&
      options_.fb_struct_recovery_function) {
    options_.fb_struct_recovery_function(data_record);
  }
  return record_writer_->status();
}

template <typename DestStreamT>
absl::Status DeltaRecordStreamWriter<DestStreamT>::Flush() {
  if (!record_writer_->Flush()) {
    return record_writer_->status();
  }
  return absl::OkStatus();
}

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_WRITERS_DELTA_RECORD_STREAM_WRITER_H_
