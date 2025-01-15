// Copyright 2024 Google LLC
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

#include "public/data_loading/writers/delta_record_limiting_file_writer.h"

#include "absl/log/log.h"

namespace kv_server {

riegeli::RecordWriterBase::Options GetRecordWriterOptions(
    const DeltaRecordWriter::Options& options) {
  riegeli::RecordWriterBase::Options writer_options;
  if (!options.enable_compression) {
    writer_options.set_uncompressed();
  }
  riegeli::RecordsMetadata metadata;
  *metadata.MutableExtension(kv_server::kv_file_metadata) = options.metadata;
  writer_options.set_metadata(std::move(metadata));
  return writer_options;
}

riegeli::LimitingWriterBase::Options GetLimitingWriterOptions(
    int max_file_size_bytes) {
  riegeli::LimitingWriterBase::Options limiting_options;
  limiting_options.set_max_length(max_file_size_bytes);
  return limiting_options;
}

DeltaRecordLimitingFileWriter::DeltaRecordLimitingFileWriter(
    std::string file_name, Options options, int64_t max_file_size_bytes)
    : options_(std::move(options)),
      file_writer_(std::move(file_name)),
      record_writer_(riegeli::RecordWriter(
          riegeli::Maker<riegeli::LimitingWriter>(
              &file_writer_, GetLimitingWriterOptions(max_file_size_bytes)),
          GetRecordWriterOptions(options_))),
      file_writer_pos_(file_writer_.pos()) {}

absl::StatusOr<std::unique_ptr<DeltaRecordLimitingFileWriter>>
DeltaRecordLimitingFileWriter::Create(std::string file_name, Options options,
                                      int64_t max_file_size_bytes) {
  return absl::WrapUnique(new DeltaRecordLimitingFileWriter(
      std::move(file_name), std::move(options), max_file_size_bytes));
}

absl::Status DeltaRecordLimitingFileWriter::ProcessWritingFailure() {
  if (!absl::IsResourceExhausted(record_writer_.status())) {
    return record_writer_.status();
  }
  // If writing/flushing the record fails, the limit was exceeded, but the file
  // now contains an incomplete chunk after the remembered position.
  VLOG(5) << "Failed writing a record to the underlying file, truncating to "
             "the last known chunk border: "
          << file_writer_pos_;

  file_writer_.Truncate(file_writer_pos_);
  file_writer_.Close();
  return absl::ResourceExhaustedError(
      "The capacity of the underlying file has been exhausted. Please "
      "recreate limiting file writer.");
}

absl::Status DeltaRecordLimitingFileWriter::WriteRecord(
    const DataRecordT& data_record) {
  file_writer_pos_ = file_writer_.pos();
  auto [fbs_buffer, bytes_to_write] = Serialize(data_record);
  if (!record_writer_.WriteRecord(bytes_to_write)) {
    return ProcessWritingFailure();
  }

  return record_writer_.status();
}

absl::Status DeltaRecordLimitingFileWriter::Flush() {
  if (!record_writer_.Flush()) {
    return ProcessWritingFailure();
  }
  return absl::OkStatus();
}

void DeltaRecordLimitingFileWriter::Close() {
  if (!record_writer_.Close()) {
    if (!absl::IsResourceExhausted(record_writer_.status())) {
      // still attempting to close the file writer.
      file_writer_.Close();
      return;
    }
    VLOG(5) << "Failed flushing to the underlying file, truncating to the last "
               "known chunk border: "
            << file_writer_pos_;
    // If flushing fails, the limit was exceeded, but the file now
    // contains an incomplete chunk after the remembered position.
    file_writer_.Truncate(file_writer_pos_);
  }
  file_writer_.Close();
}

const DeltaRecordLimitingFileWriter::Options&
DeltaRecordLimitingFileWriter::GetOptions() const {
  return options_;
}
bool DeltaRecordLimitingFileWriter::IsOpen() {
  return record_writer_.is_open();
}
absl::Status DeltaRecordLimitingFileWriter::Status() {
  return record_writer_.status();
}

}  // namespace kv_server
