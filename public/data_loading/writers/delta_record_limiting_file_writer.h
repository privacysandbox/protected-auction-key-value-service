/*
 * Copyright 2024 Google LLC
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

#ifndef PUBLIC_DATA_LOADING_WRITERS_delta_record_limiting_file_writer_H_
#define PUBLIC_DATA_LOADING_WRITERS_delta_record_limiting_file_writer_H_

#include <limits>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "public/data_loading/writers/delta_record_writer.h"
#include "riegeli/bytes/fd_dependency.h"
#include "riegeli/bytes/fd_writer.h"
#include "riegeli/bytes/limiting_writer.h"
#include "riegeli/records/record_writer.h"

namespace kv_server {

// Use `DeltaRecordLimitingFileWriter` if you need to create a well formed
// riegli file that does not exceed a certain file size.
// Note that during flushing
// to the underlying file, which can happen during Write, Flush, Close --
// multiple records might not make it to the underlying file. This
// implementation does not support recovering those records.
class DeltaRecordLimitingFileWriter : public DeltaRecordWriter {
 public:
  ~DeltaRecordLimitingFileWriter() override { Close(); }
  DeltaRecordLimitingFileWriter(const DeltaRecordLimitingFileWriter&) = delete;
  DeltaRecordLimitingFileWriter& operator=(
      const DeltaRecordLimitingFileWriter&) = delete;
  // Create the DeltaRecordLimitingFileWriter. The `file_name` must refer to
  // real file. Usually `DeltaRecordLimitingFileWriter` is used to set the
  // maximum byte size of the output file. that can be done with
  // `max_file_size_bytes`.
  static absl::StatusOr<std::unique_ptr<DeltaRecordLimitingFileWriter>> Create(
      std::string file_name, Options options,
      int64_t max_file_size_bytes = std::numeric_limits<int64_t>::max());
  // If ResourceExhaustedStatus is returned, it means that the underlying file
  // has reached it's hard size limit. Please create a new
  // `DeltaRecordLimitingFileWriter` writing to a new file. Note that multiple
  // records might be dropped, and not just the latest one.
  absl::Status WriteRecord(const DataRecordStruct& data_record) override;
  const Options& GetOptions() const override;
  // If ResourceExhaustedStatus is returned, it means that the underlying file
  // has reached it's hard size limit. Please create a new
  // `DeltaRecordLimitingFileWriter` writing to a new file. Note that multiple
  // records might be dropped.
  absl::Status Flush() override;
  void Close() override;
  bool IsOpen() override;
  absl::Status Status() override;
  absl::Status ProcessWritingFailure();

 private:
  DeltaRecordLimitingFileWriter(
      std::string file_name, Options options,
      int64_t max_file_size_bytes = std::numeric_limits<int64_t>::max());
  Options options_;
  std::unique_ptr<riegeli::FdWriter<riegeli::OwnedFd>> file_writer_;
  std::unique_ptr<riegeli::RecordWriter<
      riegeli::LimitingWriter<riegeli::FdWriter<riegeli::OwnedFd>*>>>
      record_writer_;
  int file_writer_pos_;
};
}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_WRITERS_delta_record_limiting_file_writer_H_
