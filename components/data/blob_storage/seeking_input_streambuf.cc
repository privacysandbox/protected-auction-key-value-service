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

#include "components/data/blob_storage/seeking_input_streambuf.h"

#include <algorithm>
#include <streambuf>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "components/telemetry/server_definition.h"

namespace kv_server {
namespace {

constexpr std::string_view kSizeEventName = "SeekingInputStreambuf::Size";
constexpr std::string_view kUnderflowEventName =
    "SeekingInputStreambuf::underflow";
constexpr std::string_view kSeekoffEventName = "SeekingInputStreambuf::seekoff";

void MaybeVerboseLogLatency(
    std::string_view event_name, absl::Duration latency,
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    double sampling_threshold = 0.05) {
  if ((double)std::rand() / RAND_MAX <= sampling_threshold) {
    PS_VLOG(3, log_context)
        << event_name << " latency: " << absl::ToDoubleMilliseconds(latency)
        << " ms.";
  }
}
}  // namespace

SeekingInputStreambuf::SeekingInputStreambuf(Options options)
    : options_(std::move(options)) {
  setg(buffer_.data(), buffer_.data(), buffer_.data() + buffer_.length());
}

std::streampos SeekingInputStreambuf::seekpos(std::streampos pos,
                                              std::ios_base::openmode which) {
  return seekoff(std::streamoff(pos), std::ios_base::beg, which);
}

std::streampos SeekingInputStreambuf::seekoff(std::streamoff off,
                                              std::ios_base::seekdir dir,
                                              std::ios_base::openmode which) {
  ScopeLatencyMetricsRecorder<ServerSafeMetricsContext,
                              kSeekingInputStreambufSeekoffLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  const auto size = Size();
  if (ABSL_PREDICT_FALSE(!size.ok())) {
    MaybeReportError(size.status());
    return std::streampos(std::streamoff(-1));
  }
  int64_t new_position;
  switch (dir) {
    case std::ios_base::beg:
      if (ABSL_PREDICT_FALSE(off < 0)) {
        return std::streampos(std::streamoff(-1));
      }
      new_position = static_cast<int64_t>(off);
      break;
    case std::ios_base::cur:
      new_position = BufferCursorPosition() + static_cast<int64_t>(off);
      break;
    case std::ios_base::end:
      if (ABSL_PREDICT_FALSE(off > 0)) {
        return std::streampos(std::streamoff(-1));
      }
      new_position = *size + static_cast<int64_t>(off);
      break;
    default:
      MaybeReportError(absl::InvalidArgumentError(absl::StrFormat(
          "Unkown seeking direction: int(%d)", static_cast<int>(dir))));
      return std::streampos(std::streamoff(-1));
  }
  // If we try to seek to offsets larger than the underlying blob size, round
  // down to the blob size (which is `traits_type::eof()`). If we try to seek to
  // offsets before the beginning of the underlying blob, round up to blob byte
  // at index=0.
  new_position = std::min(*size, std::max(0l, new_position));
  // If new_position falls outside the buffer, we need to reset the get area
  // by resetting the buffer so that the next read will call `underflow()`.
  if (new_position < BufferStartPosition() ||
      new_position >= src_limit_position_) {
    buffer_.clear();
    src_limit_position_ = new_position;
    setg(buffer_.data(), buffer_.data(), buffer_.data() + buffer_.length());
  } else {
    // Here, we are seeking inside the buffer so we need to reset the cursor to
    // the new_position and `src_limit_position_` remains the same, i.e.,
    // pointing right after the end of the buffer.
    setg(buffer_.data(),
         buffer_.data() + (new_position - BufferStartPosition()),
         buffer_.data() + buffer_.length());
  }
  MaybeVerboseLogLatency(kSeekoffEventName, latency_recorder.GetLatency(),
                         options_.log_context);
  return std::streampos(std::streamoff(new_position));
}

std::streambuf::int_type SeekingInputStreambuf::underflow() {
  ScopeLatencyMetricsRecorder<ServerSafeMetricsContext,
                              kSeekingInputStreambufUnderflowLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  const auto size = Size();
  if (ABSL_PREDICT_FALSE(!size.ok())) {
    MaybeReportError(size.status());
    return traits_type::eof();
  }
  if (src_limit_position_ >= *size) {
    return traits_type::eof();
  }
  const int64_t total_bytes_to_read =
      std::max(std::min(*size - src_limit_position_, options_.buffer_size), 1l);
  int64_t total_bytes_read = 0;
  buffer_.resize(total_bytes_to_read);
  while (total_bytes_read < total_bytes_to_read) {
    int64_t chunk_size = total_bytes_to_read - total_bytes_read;
    auto actual_bytes_read =
        ReadChunk(src_limit_position_, chunk_size, buffer_.data());
    if (ABSL_PREDICT_FALSE(!actual_bytes_read.ok())) {
      MaybeReportError(actual_bytes_read.status());
      return traits_type::eof();
    }
    if (ABSL_PREDICT_FALSE(*actual_bytes_read < 0)) {
      break;
    }
    src_limit_position_ += *actual_bytes_read;
    total_bytes_read += *actual_bytes_read;
  }
  if (total_bytes_read == 0) {
    return traits_type::eof();
  }
  if (ABSL_PREDICT_FALSE(total_bytes_read < total_bytes_to_read)) {
    buffer_.resize(total_bytes_read);
  }
  setg(buffer_.data(), buffer_.data(), buffer_.data() + buffer_.length());
  LogIfError(
      KVServerContextMap()
          ->SafeMetric()
          .template LogHistogram<kBlobStorageReadBytes>((int)total_bytes_read));
  MaybeVerboseLogLatency(kUnderflowEventName, latency_recorder.GetLatency(),
                         options_.log_context);
  return traits_type::to_int_type(buffer_[0]);
}

std::streamsize SeekingInputStreambuf::showmanyc() {
  return std::streamsize(BufferAvailableChars());
}

int64_t SeekingInputStreambuf::BufferAvailableChars() {
  return static_cast<int64_t>(egptr() - gptr());
}

int64_t SeekingInputStreambuf::BufferStartPosition() {
  return src_limit_position_ - static_cast<int64_t>(egptr() - eback());
}

int64_t SeekingInputStreambuf::BufferCursorPosition() {
  return src_limit_position_ - BufferAvailableChars();
}

absl::StatusOr<int64_t> SeekingInputStreambuf::Size() {
  ScopeLatencyMetricsRecorder<ServerSafeMetricsContext,
                              kSeekingInputStreambufSizeLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  if (ABSL_PREDICT_TRUE(src_cached_size_ >= 0)) {
    return src_cached_size_;
  }
  const auto size = SizeImpl();
  if (!size.ok()) {
    return size.status();
  }
  src_cached_size_ = *size;
  MaybeVerboseLogLatency(kSizeEventName, latency_recorder.GetLatency(),
                         options_.log_context);
  return *size;
}

void SeekingInputStreambuf::MaybeReportError(absl::Status error) {
  if (ABSL_PREDICT_TRUE(options_.error_callback)) {
    options_.error_callback(error);
  }
}

}  // namespace kv_server
