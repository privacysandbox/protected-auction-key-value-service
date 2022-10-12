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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "glog/logging.h"
#include "public/data_loading/records_utils.h"
#include "public/data_loading/writers/delta_record_writer.h"
#include "riegeli/bytes/ostream_writer.h"
#include "riegeli/records/record_writer.h"

namespace fledge::kv_server {

template <typename DestStreamT = std::iostream>
class DeltaRecordStreamWriter : public DeltaRecordWriter {
 public:
  ~DeltaRecordStreamWriter() override { Close(); }

  DeltaRecordStreamWriter(const DeltaRecordStreamWriter&) = delete;
  DeltaRecordStreamWriter& operator=(const DeltaRecordStreamWriter&) = delete;

  static absl::StatusOr<std::unique_ptr<DeltaRecordStreamWriter>> Create(
      DestStreamT& dest_stream, Options options);
  absl::Status WriteRecord(const DeltaFileRecordStruct& record) override;
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
  if (!options.metadata.has_key_namespace() ||
      options.metadata.key_namespace() ==
          KeyNamespace_Enum_KEY_NAMESPACE_UNSPECIFIED) {
    return absl::InvalidArgumentError(
        "Key namespace is required for writing delta files.");
  }
  return absl::WrapUnique(new DeltaRecordStreamWriter(dest_stream, options));
}

template <typename DestStreamT>
absl::Status DeltaRecordStreamWriter<DestStreamT>::WriteRecord(
    const DeltaFileRecordStruct& record) {
  if (!record_writer_->WriteRecord(ToStringView(record.ToFlatBuffer()))) {
    options_.recovery_function(record);
  }
  return record_writer_->status();
}

template <typename DestStreamT>
absl::Status DeltaRecordStreamWriter<DestStreamT>::Flush() {
  if (!record_writer_->Flush()) {
    return absl::InternalError("Failed to flush data to underlying stream.");
  }
  return absl::OkStatus();
}

}  // namespace fledge::kv_server

#endif  // PUBLIC_DATA_LOADING_WRITERS_DELTA_RECORD_STREAM_WRITER_H_
