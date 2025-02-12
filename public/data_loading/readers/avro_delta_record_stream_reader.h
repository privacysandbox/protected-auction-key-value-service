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

#ifndef PUBLIC_DATA_LOADING_READERS_AVRO_DELTA_RECORD_STREAM_READER_H_
#define PUBLIC_DATA_LOADING_READERS_AVRO_DELTA_RECORD_STREAM_READER_H_

#include <memory>
#include <string>
#include <utility>

#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/readers/delta_record_reader.h"
#include "public/data_loading/record_utils.h"
#include "public/data_loading/riegeli_metadata.pb.h"
#include "third_party/avro/api/DataFile.hh"
#include "third_party/avro/api/Schema.hh"
#include "third_party/avro/api/Stream.hh"

namespace kv_server {

// A `AvroDeltaRecordStreamReader` reads records as `DataRecordT`s
// from a delta record input stream source.
//
// A `AvroDeltaRecordStreamReader` can be used to read records as follows:
// ```
// std::ifstream delta_file(my_filename);
// AvroDeltaRecordStreamReader record_reader(delta_file);
// absl::Status status = record_reader.ReadRecords(
//  [](const DataRecordT& record) {
//    UseRecord(record);
//    return absl::OkStatus();
//  }
// );
// ```
// Note that this class incurs a copy of the records,
// consider using `RiegeliStreamReader` directly to avoid copies.
template <typename SrcStreamT = std::iostream>
class AvroDeltaRecordStreamReader : public DeltaRecordReader {
 public:
  explicit AvroDeltaRecordStreamReader(SrcStreamT& src_stream)
      : src_stream_(src_stream) {}

  AvroDeltaRecordStreamReader(const AvroDeltaRecordStreamReader&) = delete;
  AvroDeltaRecordStreamReader& operator=(const AvroDeltaRecordStreamReader&) =
      delete;

  absl::Status ReadRecords(const std::function<absl::Status(const DataRecord&)>&
                               record_callback) override;
  bool IsOpen() const override { return true; };
  absl::Status Status() const override { return absl::OkStatus(); }
  absl::StatusOr<KVFileMetadata> ReadMetadata();

 private:
  SrcStreamT& src_stream_;
};

template <typename SrcStreamT>
absl::Status AvroDeltaRecordStreamReader<SrcStreamT>::ReadRecords(
    const std::function<absl::Status(const DataRecord&)>& record_callback) {
  std::string record_string;
  absl::Status status = absl::OkStatus();
  avro::DataFileReader<std::string> reader(
      avro::istreamInputStream(src_stream_));
  while (reader.read(record_string)) {
    const DataRecord* fbs_record =
        flatbuffers::GetRoot<DataRecord>(record_string.data());
    status.Update(record_callback(*fbs_record));
  }
  return status;
}

template <typename SrcStreamT>
absl::StatusOr<KVFileMetadata>
AvroDeltaRecordStreamReader<SrcStreamT>::ReadMetadata() {
  try {
    // Reset stream to beginning
    src_stream_.clear();
    src_stream_.seekg(0);
    avro::DataFileReader<std::string> reader(
        avro::istreamInputStream(src_stream_));
    std::string serialized_metadata =
        reader.getMetadata(kAvroKVFileMetadataKey);
    src_stream_.clear();
    src_stream_.seekg(0);
    return GetKVFileMetadataFromString(serialized_metadata);
  } catch (const std::exception& e) {
    return absl::InternalError(e.what());
  }
}

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_READERS_AVRO_DELTA_RECORD_STREAM_READER_H_
