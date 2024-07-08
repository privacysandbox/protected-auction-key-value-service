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

#ifndef PUBLIC_DATA_LOADING_READERS_DELTA_RECORD_STREAM_READER_H_
#define PUBLIC_DATA_LOADING_READERS_DELTA_RECORD_STREAM_READER_H_

#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/readers/delta_record_reader.h"
#include "public/data_loading/readers/riegeli_stream_io.h"
#include "public/data_loading/records_utils.h"

namespace kv_server {

// A `DeltaRecordStreamReader` reads records as `DataRecordStruct`s
// from a delta record input stream source.
//
// A `DeltaRecordStreamReader` can be used to read records as follows:
// ```
// std::ifstream delta_file(my_filename);
// DeltaRecordStreamReader record_reader(delta_file);
// absl::Status status = record_reader.ReadRecords(
//  [](const DataRecordStruct& record) {
//    UseRecord(record);
//    return absl::OkStatus();
//  }
// );
// ```
// Note that this class incurs a copy of the records,
// consider using `RiegeliStreamReader` directly to avoid copies.
template <typename SrcStreamT = std::iostream>
class DeltaRecordStreamReader : public DeltaRecordReader {
 public:
  explicit DeltaRecordStreamReader(
      SrcStreamT& src_stream,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext))
      : stream_reader_(RiegeliStreamReader<std::string_view>(
            src_stream,
            [&log_context](const riegeli::SkippedRegion& region,
                           riegeli::RecordReaderBase& record_reader) {
              PS_LOG(ERROR, log_context) << "Failed to read region: " << region;
              return true;
            },
            log_context)),
        log_context_(log_context) {}
  DeltaRecordStreamReader(const DeltaRecordStreamReader&) = delete;
  DeltaRecordStreamReader& operator=(const DeltaRecordStreamReader&) = delete;

  absl::Status ReadRecords(const std::function<absl::Status(DataRecordStruct)>&
                               record_callback) override;
  absl::Status ReadRecords(const std::function<absl::Status(const DataRecord&)>&
                               record_callback) override;
  bool IsOpen() const override { return stream_reader_.IsOpen(); };
  absl::Status Status() const override { return stream_reader_.Status(); }
  absl::StatusOr<KVFileMetadata> ReadMetadata() {
    return stream_reader_.GetKVFileMetadata();
  }

 private:
  RiegeliStreamReader<std::string_view> stream_reader_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

template <typename SrcStreamT>
absl::Status DeltaRecordStreamReader<SrcStreamT>::ReadRecords(
    const std::function<absl::Status(DataRecordStruct)>& record_callback) {
  return stream_reader_.ReadStreamRecords(
      [&record_callback](std::string_view record_string) {
        return DeserializeDataRecord(record_string, record_callback);
      });
}

template <typename SrcStreamT>
absl::Status DeltaRecordStreamReader<SrcStreamT>::ReadRecords(
    const std::function<absl::Status(const DataRecord&)>& record_callback) {
  return stream_reader_.ReadStreamRecords(
      [&record_callback](std::string_view record_string) {
        return DeserializeRecord(record_string, record_callback);
      });
}

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_READERS_DELTA_RECORD_STREAM_READER_H_
