// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "public/data_loading/readers/avro_stream_io.h"

#include "absl/log/check.h"
#include "third_party/avro/api/DataFile.hh"
#include "third_party/avro/api/Schema.hh"
#include "third_party/avro/api/Stream.hh"

namespace kv_server {

AvroStreamReader::AvroStreamReader(std::istream& data_input)
    : data_input_(data_input) {}

absl::Status AvroStreamReader::ReadStreamRecords(
    const std::function<absl::Status(const std::string_view&)>& callback) {
  try {
    avro::InputStreamPtr input_stream = avro::istreamInputStream(data_input_);
    avro::DataFileReader<std::string> reader(std::move(input_stream));

    std::string record;
    absl::Status overall_status;
    while (reader.read(record)) {
      overall_status.Update(callback(record));
    }
    return overall_status;
  } catch (const std::exception& e) {
    return absl::InternalError(e.what());
  }
}

AvroConcurrentStreamRecordReader::AvroConcurrentStreamRecordReader(
    std::function<std::unique_ptr<RecordStream>()> stream_factory,
    Options options)
    : stream_factory_(std::move(stream_factory)), options_(std::move(options)) {
  CHECK(options.num_worker_threads >= 1)
      << "Number of work threads must be at least 1.";
}

// TODO(b/313468899): support metadata
absl::StatusOr<KVFileMetadata>
AvroConcurrentStreamRecordReader::GetKVFileMetadata() {
  return KVFileMetadata();
}

absl::StatusOr<int64_t> AvroConcurrentStreamRecordReader::RecordStreamSize() {
  auto record_stream = stream_factory_();
  auto& stream = record_stream->Stream();
  stream.seekg(0, std::ios_base::end);
  int64_t size = stream.tellg();
  if (size == -1) {
    return absl::InvalidArgumentError("Input streams do not support seeking.");
  }
  return size;
}

absl::StatusOr<std::vector<AvroConcurrentStreamRecordReader::ByteRange>>
AvroConcurrentStreamRecordReader::BuildByteRanges() {
  absl::StatusOr<int64_t> stream_size = RecordStreamSize();
  if (!stream_size.ok()) {
    return stream_size.status();
  }
  // The chunk size must be at least `options_.min_chunk_size_bytes` and
  // at most `*stream_size`.
  int64_t byte_range_size = std::min(
      *stream_size, std::max(int64_t(std::ceil((double)*stream_size /
                                               options_.num_worker_threads)),
                             options_.min_byte_range_size_bytes));
  int64_t byte_range_begin_offset = 0;
  std::vector<ByteRange> byte_ranges;
  byte_ranges.reserve(options_.num_worker_threads);
  while (byte_range_begin_offset < *stream_size) {
    int64_t end_offset = byte_range_begin_offset + byte_range_size;
    end_offset = std::min(end_offset, *stream_size);
    byte_ranges.push_back(ByteRange{
        .begin_offset = byte_range_begin_offset,
        .end_offset = end_offset,
    });
    byte_range_begin_offset = end_offset + 1;
  }
  if (byte_ranges.empty() || byte_ranges.back().end_offset != *stream_size) {
    return absl::InternalError("Failed to generate byte_ranges.");
  }
  return byte_ranges;
}

// Note that this function blocks until all records in the underlying record
// stream are read.
absl::Status AvroConcurrentStreamRecordReader::ReadStreamRecords(
    const std::function<absl::Status(const std::string_view&)>& callback) {
  ScopeLatencyMetricsRecorder<
      ServerSafeMetricsContext,
      kConcurrentStreamRecordReaderReadStreamRecordsLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  auto byte_ranges = BuildByteRanges();
  if (!byte_ranges.ok() || byte_ranges->empty()) {
    return byte_ranges.status();
  }
  std::vector<std::future<absl::StatusOr<ByteRangeResult>>>
      byte_range_reader_tasks;
  for (const auto& byte_range : *byte_ranges) {
    // TODO: b/268339067 - Investigate using an executor because
    // std::async is generally not preferred, but works fine as an
    // initial implementation.
    byte_range_reader_tasks.push_back(std::async(
        std::launch::async,
        &AvroConcurrentStreamRecordReader::ReadByteRangeExceptionless, this,
        std::ref(byte_range), std::ref(callback)));
  }
  int64_t total_records_read = 0;
  for (auto& task : byte_range_reader_tasks) {
    absl::StatusOr<ByteRangeResult> curr_byte_range_result = task.get();
    // TODO: The stuff below should be handled more gracefully,
    // e.g., only retry the byte_range that failed or skipped some
    // records.
    if (!curr_byte_range_result.ok()) {
      return curr_byte_range_result.status();
    }
    total_records_read += curr_byte_range_result->num_records_read;
  }
  PS_VLOG(2, options_.log_context)
      << "Done reading " << total_records_read << " records in "
      << absl::ToDoubleMilliseconds(latency_recorder.GetLatency()) << " ms.";
  return absl::OkStatus();
}

absl::StatusOr<typename AvroConcurrentStreamRecordReader::ByteRangeResult>
AvroConcurrentStreamRecordReader::ReadByteRangeExceptionless(
    const ByteRange& byte_range,
    const std::function<absl::Status(const std::string_view&)>&
        record_callback) noexcept {
  try {
    return ReadByteRange(byte_range, record_callback);
  } catch (const std::exception& e) {
    return absl::InternalError(e.what());
  }
}

absl::StatusOr<typename AvroConcurrentStreamRecordReader::ByteRangeResult>
AvroConcurrentStreamRecordReader::ReadByteRange(
    const ByteRange& byte_range,
    const std::function<absl::Status(const std::string_view&)>&
        record_callback) {
  PS_VLOG(2, options_.log_context)
      << "Reading byte_range: " << "[" << byte_range.begin_offset << ","
      << byte_range.end_offset << "]";
  ScopeLatencyMetricsRecorder<ServerSafeMetricsContext,
                              kConcurrentStreamRecordReaderReadByteRangeLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  auto record_stream = stream_factory_();
  PS_VLOG(9, options_.log_context) << "creating input stream";
  avro::InputStreamPtr input_stream =
      avro::istreamInputStream(record_stream->Stream());
  PS_VLOG(9, options_.log_context) << "creating reader";
  auto record_reader = std::make_unique<avro::DataFileReader<std::string>>(
      std::move(input_stream));
  PS_VLOG(9, options_.log_context) << "syncing to block";
  if (record_stream->Stream().bad()) {
    return absl::InternalError("Avro stream is bad");
  }
  record_stream->Stream().clear();
  record_reader->sync(byte_range.begin_offset);
  int64_t num_records_read = 0;
  std::string record;
  absl::Status overall_status;
  while (!record_reader->pastSync(byte_range.end_offset) &&
         record_reader->read(record)) {
    overall_status.Update(record_callback(record));
    num_records_read++;
  }
  // TODO: b/269119466 - Figure out how to handle this better. Maybe add
  // metrics to track callback failures (??).
  if (!overall_status.ok()) {
    PS_LOG(ERROR, options_.log_context)
        << "Record callback failed to process some records with: "
        << overall_status;
    return overall_status;
  }
  PS_VLOG(2, options_.log_context)
      << "Done reading " << num_records_read << " records in byte_range: ["
      << byte_range.begin_offset << "," << byte_range.end_offset << "] in "
      << absl::ToDoubleMilliseconds(latency_recorder.GetLatency()) << " ms.";
  ByteRangeResult result;
  result.num_records_read = num_records_read;
  return result;
}

}  // namespace kv_server
