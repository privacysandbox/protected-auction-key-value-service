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

#ifndef PUBLIC_DATA_LOADING_READERS_RIEGELI_STREAM_IO_H_
#define PUBLIC_DATA_LOADING_READERS_RIEGELI_STREAM_IO_H_

#include <algorithm>
#include <cmath>
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/optimization.h"
#include "absl/cleanup/cleanup.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "components/telemetry/server_definition.h"
#include "public/data_loading/readers/stream_record_reader.h"
#include "public/data_loading/riegeli_metadata.pb.h"
#include "riegeli/bytes/istream_reader.h"
#include "riegeli/records/record_reader.h"
#include "src/logger/request_context_logger.h"
#include "src/telemetry/telemetry_provider.h"

namespace kv_server {

// Reader that can read streams in Riegeli format.
template <typename RecordT>
class RiegeliStreamReader : public StreamRecordReader {
 public:
  // `data_input` must be at the file beginning when passed in.
  explicit RiegeliStreamReader(
      std::istream& data_input,
      std::function<bool(const riegeli::SkippedRegion&,
                         riegeli::RecordReaderBase& record_reader)>
          recover,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : reader_(riegeli::RecordReader(
            riegeli::IStreamReader(&data_input),
            riegeli::RecordReaderBase::Options().set_recovery(
                std::move(recover)))),
        log_context_(log_context) {}

  absl::StatusOr<KVFileMetadata> GetKVFileMetadata() override {
    riegeli::RecordsMetadata metadata;
    if (!reader_.ReadMetadata(metadata)) {
      if (reader_.ok()) {
        return absl::UnavailableError(
            "Metadata not found. Please ensure metadata is set properly.");
      }
      return reader_.status();
    }

    auto file_metadata = metadata.GetExtension(kv_file_metadata);
    PS_VLOG(2, log_context_)
        << "File metadata: " << file_metadata.DebugString();
    return file_metadata;
  }

  ~RiegeliStreamReader() { reader_.Close(); }

  absl::Status ReadStreamRecords(
      const std::function<absl::Status(const RecordT&)>& callback) override {
    RecordT record;
    absl::Status overall_status;
    while (reader_.ReadRecord(record)) {
      const auto callback_status = callback(record);
      LOG_IF(WARNING, !callback_status.ok());
      overall_status.Update(callback_status);
    }
    if (!overall_status.ok()) {
      PS_LOG(ERROR, log_context_) << overall_status;
    }
    return reader_.status();
  }

  bool IsOpen() const { return reader_.is_open(); }
  absl::Status Status() const { return reader_.status(); }

 private:
  riegeli::RecordReader<riegeli::IStreamReader<>> reader_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

const int64_t kDefaultNumWorkerThreads = std::thread::hardware_concurrency();
constexpr int64_t kDefaultMinShardSize = 8 * 1024 * 1024;  // 8MB
constexpr std::string_view kReadShardRecordsLatencyEvent =
    "ConcurrentStreamRecordReader::ReadShardRecords";
constexpr std::string_view kReadStreamRecordsLatencyEvent =
    "ConcurrentStreamRecordReader::ReadStreamRecords";

// A `ConcurrentStreamRecordReader` reads a Riegeli data stream containing
// `RecordT` records concurrently. The reader splits the data stream
// into shards with an approximately equal number of records and reads the
// shards in parallel. Each record in the underlying data stream is guaranteed
// to be read exactly once. The concurrency level can be configured using
// `ConcurrentStreamRecordReader<RecordT>::Options`.
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
// ConcurrentStreamRecordReader<std::string_view> record_reader([&data_blob]()
// {
//  return std::make_unique<StringBlobStream>(data_blob);
// });
// auto status = record_reader.ReadStreamRecords(...);
//
// Note that the input `stream_factory` is required to produce streams that
// support seeking, can be read independently and point to the same
// underlying underlying Riegeli data stream, e.g., multiple `std::ifstream`
// streams pointing to the same underlying file.
template <typename RecordT>
class ConcurrentStreamRecordReader : public StreamRecordReader {
 public:
  struct Options {
    int64_t num_worker_threads = kDefaultNumWorkerThreads;
    int64_t min_shard_size_bytes = kDefaultMinShardSize;
    privacy_sandbox::server_common::log::PSLogContext& log_context =
        const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
            privacy_sandbox::server_common::log::kNoOpContext);
    std::function<bool(const riegeli::SkippedRegion&,
                       riegeli::RecordReaderBase& record_reader)>
        recovery_callback = [log_context = &this->log_context](
                                const riegeli::SkippedRegion& region,
                                riegeli::RecordReaderBase& record_reader) {
          PS_LOG(WARNING, *log_context)
              << "Skipping over corrupted region: " << region;
          return true;
        };
  };
  ConcurrentStreamRecordReader(
      std::function<std::unique_ptr<RecordStream>()> stream_factory,
      Options options = Options());
  ~ConcurrentStreamRecordReader() = default;
  ConcurrentStreamRecordReader(const ConcurrentStreamRecordReader&) = delete;
  ConcurrentStreamRecordReader& operator=(const ConcurrentStreamRecordReader&) =
      delete;
  absl::StatusOr<KVFileMetadata> GetKVFileMetadata() override;
  absl::Status ReadStreamRecords(
      const std::function<absl::Status(const RecordT&)>& callback) override;

 private:
  // Defines a byte range in the underlying record stream that will be read
  // concurrently with other shards.
  struct ShardRange {
    int64_t start_pos;
    int64_t end_pos;
  };
  // Defines metadata/stats returned by a shard reading task. This is useful
  // for correctness checks.
  struct ShardResult {
    int64_t first_record_pos;
    int64_t next_shard_first_record_pos;
    int64_t num_records_read;
  };
  absl::StatusOr<ShardResult> ReadShardRecords(
      const ShardRange& shard,
      const std::function<absl::Status(const RecordT&)>& record_callback);
  absl::StatusOr<std::vector<ShardRange>> BuildShards();
  absl::StatusOr<int64_t> RecordStreamSize();
  std::function<std::unique_ptr<RecordStream>()> stream_factory_;
  Options options_;
};

template <typename RecordT>
ConcurrentStreamRecordReader<RecordT>::ConcurrentStreamRecordReader(
    std::function<std::unique_ptr<RecordStream>()> stream_factory,
    Options options)
    : stream_factory_(std::move(stream_factory)), options_(std::move(options)) {
  CHECK(options.num_worker_threads >= 1)
      << "Number of work threads must be at least 1.";
}

template <typename RecordT>
absl::StatusOr<KVFileMetadata>
ConcurrentStreamRecordReader<RecordT>::GetKVFileMetadata() {
  auto record_stream = stream_factory_();
  RiegeliStreamReader<RecordT> metadata_reader(
      record_stream->Stream(),
      [log_context = &options_.log_context](
          const riegeli::SkippedRegion& region,
          riegeli::RecordReaderBase& record_reader) {
        PS_LOG(WARNING, *log_context)
            << "Skipping over corrupted region: " << region;
        return true;
      },
      options_.log_context);
  return metadata_reader.GetKVFileMetadata();
}
template <typename RecordT>
absl::StatusOr<int64_t>
ConcurrentStreamRecordReader<RecordT>::RecordStreamSize() {
  auto record_stream = stream_factory_();
  auto& stream = record_stream->Stream();
  stream.seekg(0, std::ios_base::end);
  int64_t size = stream.tellg();
  if (size == -1) {
    return absl::InvalidArgumentError("Input streams do not support seeking.");
  }
  return size;
}

template <typename RecordT>
absl::StatusOr<
    std::vector<typename ConcurrentStreamRecordReader<RecordT>::ShardRange>>
ConcurrentStreamRecordReader<RecordT>::BuildShards() {
  using ShardRangeT =
      typename ConcurrentStreamRecordReader<RecordT>::ShardRange;
  absl::StatusOr<int64_t> stream_size = RecordStreamSize();
  if (!stream_size.ok()) {
    return stream_size.status();
  }
  if (options_.num_worker_threads < 1) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Num worker threads %d must be at least 1.",
                        options_.num_worker_threads));
  }
  // The shard size must be at least `options_.min_shard_size_bytes` and
  // at most `*stream_size`.
  int64_t shard_size = std::min(
      *stream_size, std::max(int64_t(std::ceil((double)*stream_size /
                                               options_.num_worker_threads)),
                             options_.min_shard_size_bytes));
  int64_t shard_start_pos = 0;
  std::vector<ShardRangeT> shards;
  shards.reserve(options_.num_worker_threads);
  while (shard_start_pos < *stream_size) {
    int64_t shard_end_pos = shard_start_pos + shard_size;
    shard_end_pos = std::min(shard_end_pos, *stream_size);
    shards.push_back(ShardRangeT{
        .start_pos = shard_start_pos,
        .end_pos = shard_end_pos,
    });
    shard_start_pos = shard_end_pos + 1;
  }
  if (shards.empty() || shards.back().end_pos != *stream_size) {
    return absl::InternalError("Failed to generate shards.");
  }
  return shards;
}

// Note that this function blocks until all records in the underlying record
// stream are read.
template <typename RecordT>
absl::Status ConcurrentStreamRecordReader<RecordT>::ReadStreamRecords(
    const std::function<absl::Status(const RecordT&)>& callback) {
  ScopeLatencyMetricsRecorder<
      ServerSafeMetricsContext,
      kConcurrentStreamRecordReaderReadStreamRecordsLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  auto shards = BuildShards();
  if (!shards.ok() || shards->empty()) {
    return shards.status();
  }
  std::vector<std::future<absl::StatusOr<ShardResult>>> shard_reader_tasks;
  for (const auto& shard : *shards) {
    // TODO: b/268339067 - Investigate using an executor because
    // std::async is generally not preffered, but works fine as an
    // initial implementation.
    shard_reader_tasks.push_back(
        std::async(std::launch::async,
                   &ConcurrentStreamRecordReader<RecordT>::ReadShardRecords,
                   this, std::ref(shard), std::ref(callback)));
  }
  absl::StatusOr<ShardResult> prev_shard_result = shard_reader_tasks[0].get();
  if (!prev_shard_result.ok()) {
    return prev_shard_result.status();
  }
  int64_t total_records_read = prev_shard_result->num_records_read;
  for (int i = 1; i < shard_reader_tasks.size(); i++) {
    absl::StatusOr<ShardResult> curr_shard_result = shard_reader_tasks[i].get();
    // TODO: The stuff below should be handled more gracefully,
    // e.g., only retry the shard that failed or skipped some
    // records.
    if (!curr_shard_result.ok()) {
      return curr_shard_result.status();
    }
    if (prev_shard_result->next_shard_first_record_pos <
        curr_shard_result->first_record_pos) {
      return absl::InternalError(
          absl::StrFormat("Skipped some records between byte=%d and byte=%d.",
                          prev_shard_result->next_shard_first_record_pos,
                          curr_shard_result->first_record_pos));
    }
    total_records_read += curr_shard_result->num_records_read;
    prev_shard_result = curr_shard_result;
  }
  PS_VLOG(2, options_.log_context)
      << "Done reading " << total_records_read << " records in "
      << absl::ToDoubleMilliseconds(latency_recorder.GetLatency()) << " ms.";
  return absl::OkStatus();
}

template <typename RecordT>
absl::StatusOr<typename ConcurrentStreamRecordReader<RecordT>::ShardResult>
ConcurrentStreamRecordReader<RecordT>::ReadShardRecords(
    const ShardRange& shard,
    const std::function<absl::Status(const RecordT&)>& record_callback) {
  PS_VLOG(2, options_.log_context)
      << "Reading shard: " << "[" << shard.start_pos << "," << shard.end_pos
      << "]";
  ScopeLatencyMetricsRecorder<
      ServerSafeMetricsContext,
      kConcurrentStreamRecordReaderReadShardRecordsLatency>
      latency_recorder(KVServerContextMap()->SafeMetric());
  auto record_stream = stream_factory_();
  riegeli::RecordReader<riegeli::IStreamReader<>> record_reader(
      riegeli::IStreamReader(&record_stream->Stream()),
      riegeli::RecordReaderBase::Options().set_recovery(
          options_.recovery_callback));
  if (auto result = record_reader.Seek(shard.start_pos); !result) {
    return record_reader.status();
  }
  auto next_record_pos = record_reader.pos().numeric();
  ShardResult shard_result;
  shard_result.first_record_pos = next_record_pos;
  int64_t num_records_read = 0;
  RecordT record;
  absl::Status overall_status;
  while (next_record_pos <= shard.end_pos && record_reader.ReadRecord(record)) {
    overall_status.Update(record_callback(record));
    num_records_read++;
    next_record_pos = record_reader.pos().numeric();
  }
  // TODO: b/269119466 - Figure out how to handle this better. Maybe add
  // metrics to track callback failures (??).
  if (!overall_status.ok()) {
    PS_LOG(ERROR, options_.log_context)
        << "Record callback failed to process some records with: "
        << overall_status;
  }
  if (!record_reader.ok()) {
    return record_reader.status();
  }
  shard_result.next_shard_first_record_pos = next_record_pos;
  shard_result.num_records_read = num_records_read;
  PS_VLOG(2, options_.log_context)
      << "Done reading " << num_records_read << " records in shard: ["
      << shard.start_pos << "," << shard.end_pos << "] in "
      << absl::ToDoubleMilliseconds(latency_recorder.GetLatency()) << " ms.";
  return shard_result;
}

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_READERS_RIEGELI_STREAM_IO_H_
