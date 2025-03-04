/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PUBLIC_DATA_LOADING_READERS_STREAM_RECORD_READER_H_
#define PUBLIC_DATA_LOADING_READERS_STREAM_RECORD_READER_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "public/data_loading/riegeli_metadata.pb.h"

namespace kv_server {
// Reader that can be used to load data from one data file.
//
// Subclasses should accept the data source through constructor and store the
// data source as its state.
//
// Not intended to be used by multiple threads.
class StreamRecordReader {
 public:
  virtual ~StreamRecordReader() = default;

  // Returns the metadata associated with this file. Can only be called once
  // before the first call to `ReadStreamRecords`.
  // Will return empty KVFileMetadata if no KVFileMetadata found.
  virtual absl::StatusOr<KVFileMetadata> GetKVFileMetadata() = 0;

  // Given a `data_input` stream representing a stream of string
  // records, parses the records and calls `callback` once per record.
  // If the callback returns a non-OK status, the function continues
  // reading and logs the error at the end.
  virtual absl::Status ReadStreamRecords(
      const std::function<absl::Status(const std::string_view&)>& callback) = 0;
};

// Holds a stream of data.
class RecordStream {
 public:
  virtual ~RecordStream() = default;
  virtual std::istream& Stream() = 0;
};

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_READERS_STREAM_RECORD_READER_H_
