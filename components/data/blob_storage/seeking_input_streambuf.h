/*
 * Copyright 2023 Google LLC
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

#include <memory>
#include <streambuf>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/logger/request_context_logger.h"
#include "src/telemetry/telemetry_provider.h"

#ifndef COMPONENTS_DATA_BLOB_STORAGE_SEEKING_INPUT_STREAMBUF_H_
#define COMPONENTS_DATA_BLOB_STORAGE_SEEKING_INPUT_STREAMBUF_H_

namespace kv_server {

// Defines a base class that adds seeking support to custom streambufs of
// blobs of bytes. A child streambuf that needs seeking support should inherit
// from this class and implement the following functions:
//
// * SizeImpl()
// * ReadChuck(...)
//
// For example, the following class implements a streambuf for a byte buffer
// (note this example is for illustration purposes only because (1) the C++
// standard lib already provides string streams and (2) the example class does
// a lot of unnecessary string copying.)
//
// class StringBlobInputStreambuf : public SeekingInputStreambuf {
//  public:
//   explicit StringBlobInputStreambuf(std::string_view blob)
//       : SeekingInputStreambuf(), blob_(blob) {}
//
//  protected:
//   absl::StatusOr<int64_t> ReadChunk(int64_t offset, int64_t chunk_size,
//                                      char* dest_buffer) override {
//     auto begin = blob_.data() + offset;
//     std::copy(begin, begin + chunk_size, dest_buffer);
//     return chunk_size;
//   }
//   absl::StatusOr<int64_t> SizeImpl() override { return blob_.length(); }
//
//  private:
//   std::string blob_;
// };
// ...
// StringBlobInputStreambuf sstreambuf("test blob.");
// std::istream blob_stream(&sstreambuf);
// ...
class SeekingInputStreambuf : public std::streambuf {
 public:
  struct Options {
    Options() {}
    // Buffering can be disabled by setting `buffer_size <= 0`, but when
    // buffering is disabled, data is read one character at a time from the
    // underlying source which can be painfully slow and expensive.
    std::int64_t buffer_size = 8 * 1024 * 1024;  // 8MB
    std::function<void(absl::Status)> error_callback = [](absl::Status) {};
    privacy_sandbox::server_common::log::PSLogContext& log_context =
        const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
            privacy_sandbox::server_common::log::kNoOpContext);
  };

  explicit SeekingInputStreambuf(Options options = Options());
  virtual ~SeekingInputStreambuf() = default;
  SeekingInputStreambuf(const SeekingInputStreambuf&) = delete;
  SeekingInputStreambuf& operator=(const SeekingInputStreambuf&) = delete;
  absl::StatusOr<int64_t> Size();
  const Options& GetOptions() const { return options_; }

 protected:
  std::streampos seekoff(std::streamoff off, std::ios_base::seekdir dir,
                         std::ios_base::openmode which =
                             std::ios_base::in | std::ios_base::out) override;
  std::streampos seekpos(std::streampos pos,
                         std::ios_base::openmode which =
                             std::ios_base::in | std::ios_base::out) override;
  int_type underflow() override;
  std::streamsize showmanyc() override;

  // Returns the number of bytes in the underlying blob of bytes. Or if there
  // is an error, the returned status will contain a detailed message
  // describing the error.
  virtual absl::StatusOr<int64_t> SizeImpl() = 0;
  // Reads `chunk_size` bytes from the underlying blob starting a byte indexed
  // by `offset` into `dest_buffer`. This function will always be called with
  // `offset` and `chunk_size` such that:
  //
  // * [0 <= offset < size] and [0 <= offset+chunk_size <= size]
  //
  // Returns:
  //  when `ok()`  - The actual number of bytes read.
  //  when `!ok()` - An error status with error description.
  virtual absl::StatusOr<int64_t> ReadChunk(int64_t offset, int64_t chunk_size,
                                            char* dest_buffer) = 0;

 private:
  int64_t BufferAvailableChars();
  int64_t BufferStartPosition();
  int64_t BufferCursorPosition();
  void MaybeReportError(absl::Status error);

  Options options_;
  std::string buffer_;
  // `src_limit_position_` points to the next byte to be read from the
  // underlying source. All bytes before this have been read or buffered
  // already.
  int64_t src_limit_position_ = 0;
  int64_t src_cached_size_ = -1;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_BLOB_STORAGE_SEEKING_INPUT_STREAMBUF_H_
