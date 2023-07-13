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

#ifndef PUBLIC_DATA_LOADING_READERS_DELTA_RECORD_READER_H_
#define PUBLIC_DATA_LOADING_READERS_DELTA_RECORD_READER_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "public/data_loading/records_utils.h"

namespace kv_server {

// A `DeltaRecordReader` defines an interface for reading delta records from a
// source specified by a concrete implementations of this class. Record sources
// can be files, in-memory streams, e.t.c.
//
// A `DeltaRecordReader` can be used to read records as follows:
//
// ```
// DeltaRecordReader record_reader = ...
// absl::Status status = record_reader.ReadRecords(
//     [](const DataRecordStruct& record) {
//        UseRecord(record);
//        return absl::OkStatus();
//     }
// );
// if (!status.ok) {
//    LOG(ERROR) << "Failed to read all records: " << status;
// }
// ```
class DeltaRecordReader {
 public:
  virtual ~DeltaRecordReader() = default;
  // Reads `DataRecordStruct` records from the underlying record
  // source and passes them to `record_callback` function.
  virtual absl::Status ReadRecords(
      const std::function<absl::Status(DataRecordStruct)>& record_callback) = 0;
  // Returns true if the reader is open for reading records.
  virtual bool IsOpen() const = 0;
  // Returns status of the `DeltaRecordReader`.
  virtual absl::Status Status() const = 0;
};

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_READERS_DELTA_RECORD_READER_H_
