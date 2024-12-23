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

#ifndef PUBLIC_DATA_LOADING_WRITERS_AVRO_DELTA_RECORD_STREAM_WRITER_H_
#define PUBLIC_DATA_LOADING_WRITERS_AVRO_DELTA_RECORD_STREAM_WRITER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "public/data_loading/records_utils.h"
#include "public/data_loading/writers/delta_record_writer.h"
#include "third_party/avro/api/DataFile.hh"
#include "third_party/avro/api/Schema.hh"
#include "third_party/avro/api/Stream.hh"
#include "third_party/avro/api/ValidSchema.hh"

namespace kv_server {

class AvroDeltaRecordStreamWriter : public DeltaRecordWriter {
 public:
  AvroDeltaRecordStreamWriter(const AvroDeltaRecordStreamWriter&) = delete;
  AvroDeltaRecordStreamWriter& operator=(const AvroDeltaRecordStreamWriter&) =
      delete;

  template <typename DestStreamT = std::iostream>
  static absl::StatusOr<std::unique_ptr<AvroDeltaRecordStreamWriter>> Create(
      DestStreamT& dest_stream, Options options);
  absl::Status WriteRecord(const DataRecordT& data_record) override {
    return absl::UnimplementedError("To be implemented");
  };
  absl::Status WriteRecord(const DataRecordStruct& record) override;
  const Options& GetOptions() const override { return options_; }
  absl::Status Flush() override;
  void Close() override { record_writer_->close(); }
  bool IsOpen() override { return true; }
  absl::Status Status() override { return absl::OkStatus(); }

 private:
  explicit AvroDeltaRecordStreamWriter(
      std::unique_ptr<avro::DataFileWriter<std::string>> record_writer);

  Options options_;
  std::unique_ptr<avro::DataFileWriter<std::string>> record_writer_;
};

AvroDeltaRecordStreamWriter::AvroDeltaRecordStreamWriter(
    std::unique_ptr<avro::DataFileWriter<std::string>> record_writer)
    : record_writer_(std::move(record_writer)) {}

template <typename DestStreamT>
absl::StatusOr<std::unique_ptr<AvroDeltaRecordStreamWriter>>
AvroDeltaRecordStreamWriter::Create(DestStreamT& dest_stream, Options options) {
  avro::OutputStreamPtr avro_output_stream =
      avro::ostreamOutputStream(dest_stream);
  auto stream_writer = std::make_unique<avro::DataFileWriter<std::string>>(
      std::move(avro_output_stream), avro::ValidSchema(avro::BytesSchema()));
  return absl::WrapUnique(
      new AvroDeltaRecordStreamWriter(std::move(stream_writer)));
}

absl::Status AvroDeltaRecordStreamWriter::WriteRecord(
    const DataRecordStruct& data_record) {
  auto fbs_builder = ToFlatBufferBuilder(data_record);
  std::string_view bytes_to_write = ToStringView(fbs_builder);
  record_writer_->write(std::string(bytes_to_write));
  return absl::OkStatus();
}

absl::Status AvroDeltaRecordStreamWriter::Flush() {
  record_writer_->flush();
  return absl::OkStatus();
}

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_WRITERS_AVRO_DELTA_RECORD_STREAM_WRITER_H_
