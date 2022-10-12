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

#ifndef PUBLIC_DATA_LOADING_WRITERS_DELTA_RECORD_WRITER_H_
#define PUBLIC_DATA_LOADING_WRITERS_DELTA_RECORD_WRITER_H_

#include <functional>
#include <vector>

#include "absl/status/status.h"
#include "public/data_loading/records_utils.h"
#include "public/data_loading/riegeli_metadata.pb.h"

namespace fledge::kv_server {

// A `DeltaRecordWriter` defines an interface for writing delta records to a
// destination specified by a concrete implementations of this class.
// Destinations can be files, streams, string buffers, e.t.c.
//
// Delta records can be written using the following loop:
//
// ```
// DeltaRecordWriter record_writer = ...
// while(more records to write) {
//    DeltaFileRecordStruct record = ...
//    if (absl::Status status = record_writer.WriteRecord(record); !status.ok())
//    {
//        LOG(WARN) << "Failed to write record.";
//        return status;
//    }
// }
// record_writer.Close(); // Flush all data and close writer.
// ```
class DeltaRecordWriter {
 public:
  // Options for writing delta files.
  struct Options {
    // If true, record compression will be enabled.
    bool enable_compression;
    // If writing a record fails, this function will be called with the failed
    // record.
    std::function<void(const DeltaFileRecordStruct&)> recovery_function;

    // Metadata required for delta files.
    KVFileMetadata metadata;
  };
  virtual ~DeltaRecordWriter() = default;

  // Writes a `DeltaFileRecordStruct` record to the underlying destination.
  virtual absl::Status WriteRecord(const DeltaFileRecordStruct& record) = 0;
  // Flushes any written data to the underlying destination and makes it
  // visible outside the writing process. `Flush()` is different from
  // `Close()` in that it allows for more records to be written after some data
  // has been flushed.
  virtual absl::Status Flush() = 0;
  virtual const Options& GetOptions() const = 0;
  // Similar to `Flush()` but closes the `DeltaRecordWriter` object from
  // accepting new record writes. `Close()` must be called after successfully
  // writing records unless the `DeltaRecordWriter` object is going out of
  // scope, in which case it will be `Closed()` automatically as part of
  // recourse cleanup. Trying to `WriteRecord` or `WriteRecords` after `Close()`
  // has been called will result in an error.
  virtual void Close() = 0;
  // Return true if the writer is open to writing more records.
  virtual bool IsOpen() = 0;
  // Returns status of the `DeltaRecordWriter`.
  virtual absl::Status Status() = 0;
};

}  // namespace fledge::kv_server

#endif  // PUBLIC_DATA_LOADING_WRITERS_DELTA_RECORD_WRITER_H_
