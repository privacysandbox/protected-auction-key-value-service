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

#ifndef PUBLIC_DATA_LOADING_READERS_AVRO_STREAM_IO_H_
#define PUBLIC_DATA_LOADING_READERS_AVRO_STREAM_IO_H_

#include <algorithm>
#include <cmath>
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/optimization.h"
#include "absl/cleanup/cleanup.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "components/telemetry/server_definition.h"
#include "public/data_loading/readers/stream_record_reader.h"
#include "public/data_loading/riegeli_metadata.pb.h"
#include "src/logger/request_context_logger.h"
#include "src/telemetry/telemetry_provider.h"

namespace kv_server {

// Single thread reader that can read streams in Avro format.
class AvroStreamReader : public StreamRecordReader {
 public:
  // `data_input` must be at the file beginning when passed in.
  explicit AvroStreamReader(std::istream& data_input);

  absl::StatusOr<KVFileMetadata> GetKVFileMetadata() override {
    return KVFileMetadata();
  }

  absl::Status ReadStreamRecords(
      const std::function<absl::Status(const std::string_view&)>& callback)
      override;

  bool IsOpen() const { return true; }
  absl::Status Status() const { return absl::OkStatus(); }

 private:
  std::istream& data_input_;
};

// An `AvroConcurrentStreamRecordReader` reads a Avro data stream containing
// string records concurrently. The reader splits the data stream
// into byte ranges with an approximately equal number of bytes and reads the
// chunks in parallel. Each record in the underlying data stream is guaranteed
// to be read exactly once. The concurrency level can be configured using
// `AvroConcurrentStreamRecordReader::Options`.
//
// Sample usage:
//
// class StringBlobStream : public RecordStream {
//  public:
//   explicit StringBlobStream(const std::string& blob) : stream_(blob) {}
//   std::istream& Stream() { return stream_; }
//  private:
//   std::stringstream stream_;
// };
//
// std::string data_blob = ...;
// AvroConcurrentStreamRecordReader
// record_reader([&data_blob]()
// {
//  return std::make_unique<StringBlobStream>(data_blob);
// });
// auto status = record_reader.ReadStreamRecords(...);
//
// Note that the input `stream_factory` is required to produce streams that
// support seeking, can be read independently and point to the same
// underlying underlying Avro data stream, e.g., multiple `std::ifstream`
// streams pointing to the same underlying file.
class AvroConcurrentStreamRecordReader : public StreamRecordReader {
 public:
  struct Options {
    int64_t num_worker_threads = std::thread::hardware_concurrency();
    int64_t min_byte_range_size_bytes = 8 * 1024 * 1024;  // 8MB
    privacy_sandbox::server_common::log::PSLogContext& log_context =
        const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
            privacy_sandbox::server_common::log::kNoOpContext);
    Options() {}
  };
  AvroConcurrentStreamRecordReader(
      std::function<std::unique_ptr<RecordStream>()> stream_factory,
      Options options = Options{});

  AvroConcurrentStreamRecordReader(const AvroConcurrentStreamRecordReader&) =
      delete;
  AvroConcurrentStreamRecordReader& operator=(
      const AvroConcurrentStreamRecordReader&) = delete;

  absl::StatusOr<KVFileMetadata> GetKVFileMetadata() override;

  absl::Status ReadStreamRecords(
      const std::function<absl::Status(const std::string_view&)>& callback)
      override;

 private:
  // Defines a byte range in the underlying record stream that will be read
  // concurrently with other byte ranges.
  struct ByteRange {
    int64_t begin_offset;
    int64_t end_offset;
  };

  // Defines metadata/stats returned by a byte range reading task. This is
  // useful for correctness checks.
  struct ByteRangeResult {
    int64_t num_records_read;
  };

  absl::StatusOr<ByteRangeResult> ReadByteRange(
      const ByteRange& shard,
      const std::function<absl::Status(const std::string_view&)>&
          record_callback);
  absl::StatusOr<ByteRangeResult> ReadByteRangeExceptionless(
      const ByteRange& shard,
      const std::function<absl::Status(const std::string_view&)>&
          record_callback) noexcept;
  absl::StatusOr<std::vector<ByteRange>> BuildByteRanges();
  absl::StatusOr<int64_t> RecordStreamSize();
  std::function<std::unique_ptr<RecordStream>()> stream_factory_;
  Options options_;
};

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_READERS_AVRO_STREAM_IO_H_
