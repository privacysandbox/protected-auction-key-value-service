// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "public/data_loading/readers/riegeli_stream_io.h"

#include <sstream>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "public/test_util/mocks.h"
#include "public/test_util/proto_matcher.h"
#include "riegeli/bytes/string_writer.h"
#include "riegeli/records/record_writer.h"
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using privacy_sandbox::server_common::MockMetricsRecorder;

enum class ReaderType : int8_t {
  kSequential = 1,
  kConcurrent,
};

// Holds a stream that can be used to read `blob` contents.
class StringBlobStream : public RecordStream {
 public:
  explicit StringBlobStream(const std::string& blob) : stream_(blob) {}
  std::istream& Stream() { return stream_; }

 private:
  std::stringstream stream_;
};

class StreamRecordReaderTest : public ::testing::TestWithParam<ReaderType> {
 protected:
  template <typename RecordT>
  std::unique_ptr<StreamRecordReader<RecordT>> CreateReader(
      std::stringstream& stream) {
    auto reader_factory = StreamRecordReaderFactory<std::string_view>::Create(
        {.num_worker_threads = 1});
    if (ReaderType::kConcurrent == GetParam()) {
      return reader_factory->CreateConcurrentReader(
          metrics_recorder_, [&stream]() {
            auto stream_handle =
                std::make_unique<StringBlobStream>(stream.str());
            if (stream.bad()) {
              stream_handle->Stream().setstate(std::ios_base::badbit);
            }
            return stream_handle;
          });
    }
    return reader_factory->CreateReader(stream);
  }
  MockMetricsRecorder metrics_recorder_;
};

using ConcurrentReaderOptions =
    ConcurrentStreamRecordReader<std::string_view>::Options;
class ConcurrentStreamRecordReaderTest
    : public ::testing::TestWithParam<ConcurrentReaderOptions> {
 protected:
  std::unique_ptr<StreamRecordReader<std::string_view>> CreateConcurrentReader(
      const std::string& blob_content) {
    auto reader_factory =
        StreamRecordReaderFactory<std::string_view>::Create(GetParam());
    return reader_factory->CreateConcurrentReader(
        metrics_recorder_, [&blob_content]() {
          return std::make_unique<StringBlobStream>(blob_content);
        });
  }
  MockMetricsRecorder metrics_recorder_;
};

INSTANTIATE_TEST_SUITE_P(ReaderType, StreamRecordReaderTest,
                         testing::Values(ReaderType::kSequential,
                                         ReaderType::kConcurrent));

TEST_P(StreamRecordReaderTest, ReadRecord) {
  std::string content;
  riegeli::RecordWriterBase::Options options;
  riegeli::RecordsMetadata metadata;
  KVFileMetadata file_metadata;

  *metadata.MutableExtension(kv_server::kv_file_metadata) = file_metadata;
  options.set_metadata(std::move(metadata));
  auto writer = riegeli::RecordWriter(riegeli::StringWriter(&content), options);

  std::string record("test");

  writer.WriteRecord(record);
  writer.WriteRecord(record);
  ASSERT_TRUE(writer.Close());
  std::stringstream ss(content);

  testing::MockFunction<absl::Status(std::string_view)> callback;
  EXPECT_CALL(callback, Call)
      .Times(2)
      .WillRepeatedly([](std::string_view record_read) {
        EXPECT_THAT(record_read, testing::Eq("test"));
        return absl::OkStatus();
      });
  auto reader = CreateReader<std::string_view>(ss);
  EXPECT_THAT(reader->GetKVFileMetadata().value(), EqualsProto(file_metadata));
  EXPECT_TRUE(reader->ReadStreamRecords(callback.AsStdFunction()).ok());
}

TEST_P(StreamRecordReaderTest, ContinuesReadingRecordsOnError) {
  std::string content;
  riegeli::RecordWriterBase::Options options;
  riegeli::RecordsMetadata metadata;
  KVFileMetadata file_metadata;
  *metadata.MutableExtension(kv_server::kv_file_metadata) = file_metadata;
  options.set_metadata(std::move(metadata));
  auto writer = riegeli::RecordWriter(riegeli::StringWriter(&content), options);

  std::string record("test");

  writer.WriteRecord(record);
  writer.WriteRecord(record);
  ASSERT_TRUE(writer.Close());
  std::stringstream ss(content);

  testing::MockFunction<absl::Status(std::string_view)> callback;
  EXPECT_CALL(callback, Call)
      .Times(2)
      .WillOnce([](std::string_view record_read) {
        return absl::InvalidArgumentError("Error");
      })
      .WillOnce([](std::string_view record_read) { return absl::OkStatus(); });
  auto reader = CreateReader<std::string_view>(ss);
  EXPECT_TRUE(reader->ReadStreamRecords(callback.AsStdFunction()).ok());
}

TEST_P(StreamRecordReaderTest, BadStreamFailure) {
  std::stringstream ss;
  ss.setstate(std::ios_base::badbit);
  auto reader = CreateReader<std::string_view>(ss);
  EXPECT_FALSE(
      reader
          ->ReadStreamRecords([](std::string_view) { return absl::OkStatus(); })
          .ok());
}

TEST_P(StreamRecordReaderTest, SkipsOverCorruption) {
  std::string uncorrupted_content;
  riegeli::RecordWriterBase::Options options;

  auto writer = riegeli::RecordWriter(
      riegeli::StringWriter(&uncorrupted_content), options);
  std::string record("test corruption");
  for (int i = 0; i < 100000; i++) {
    writer.WriteRecord(record);
  }
  ASSERT_TRUE(writer.Close());

  testing::MockFunction<absl::Status(std::string_view)> callback;

  // Uncorrupted
  std::stringstream uncorrupted_ss(uncorrupted_content);
  EXPECT_CALL(callback, Call)
      .Times(testing::Exactly(100000))
      .WillRepeatedly(
          [](std::string_view record_read) { return absl::OkStatus(); });
  auto uncorrupted_reader = CreateReader<std::string_view>(uncorrupted_ss);
  EXPECT_TRUE(
      uncorrupted_reader->ReadStreamRecords(callback.AsStdFunction()).ok());

  // Mimic corruption
  std::string corrupted_content =
      uncorrupted_content.replace(100, 10, "xxxxxxxxxx");
  std::stringstream corrupted_ss(corrupted_content);
  EXPECT_CALL(callback, Call)
      .Times(testing::Between(1000, 10000))
      .WillRepeatedly(
          [](std::string_view record_read) { return absl::OkStatus(); });
  auto corrupted_reader = CreateReader<std::string_view>(corrupted_ss);
  EXPECT_TRUE(
      corrupted_reader->ReadStreamRecords(callback.AsStdFunction()).ok());
}

INSTANTIATE_TEST_SUITE_P(ConcurrentOptions, ConcurrentStreamRecordReaderTest,
                         testing::Values(
                             ConcurrentReaderOptions{
                                 .num_worker_threads = 1,
                                 .min_shard_size_bytes = 1024,
                             },
                             ConcurrentReaderOptions{
                                 .num_worker_threads = 2,
                                 .min_shard_size_bytes = 1024,
                             },
                             ConcurrentReaderOptions{
                                 .num_worker_threads = 5,
                                 .min_shard_size_bytes = 1024,
                             },
                             ConcurrentReaderOptions{
                                 .num_worker_threads = 5,
                                 .min_shard_size_bytes = 1024 * 1024,
                             }));

TEST_P(ConcurrentStreamRecordReaderTest, ReadsAllRecordsExactlyOnce) {
  std::vector<riegeli::RecordWriterBase::Options> options_list{
      riegeli::RecordWriterBase::Options(),
      riegeli::RecordWriterBase::Options().set_uncompressed(),
  };
  for (auto& options : options_list) {
    std::string content;
    auto writer =
        riegeli::RecordWriter(riegeli::StringWriter(&content), options);
    testing::MockFunction<absl::Status(std::string_view)> callback;
    for (int i = 0; i < 2500; i++) {
      auto record = absl::StrCat(i);
      writer.WriteRecord(record);
      EXPECT_CALL(callback, Call(record))
          .Times(testing::Exactly(1))
          .WillOnce(
              [](std::string_view record_read) { return absl::OkStatus(); });
    }
    ASSERT_TRUE(writer.Close());
    auto record_reader = CreateConcurrentReader(content);
    EXPECT_TRUE(
        record_reader->ReadStreamRecords(callback.AsStdFunction()).ok());
  }
}

// Disables seeking from stringbufs.
class NonSeekingSStreamBuf : public std::stringbuf {
 public:
  explicit NonSeekingSStreamBuf(const std::string& blob)
      : std::stringbuf(blob) {}

 protected:
  std::streampos seekoff(std::streamoff off, std::ios_base::seekdir dir,
                         std::ios_base::openmode which =
                             std::ios_base::in | std::ios_base::out) override {
    return -1;
  }
};

// Returns a stringstream that does not support seeking.
class NonSeekingStringBlobStream : public RecordStream {
 public:
  explicit NonSeekingStringBlobStream(const std::string& blob)
      : stringbuf_(blob), stream_(&stringbuf_) {}
  std::istream& Stream() { return stream_; }

 private:
  NonSeekingSStreamBuf stringbuf_;
  std::istream stream_;
};

TEST(ConcurrentStreamRecordReaderTest, FailsToReadNonSeekingStream) {
  std::string content;
  auto writer = riegeli::RecordWriter(riegeli::StringWriter(&content),
                                      riegeli::RecordWriterBase::Options());
  testing::MockFunction<absl::Status(std::string_view)> callback;
  for (int i = 0; i < 1; i++) {
    auto record = absl::StrCat(i);
    writer.WriteRecord(record);
  }
  ASSERT_TRUE(writer.Close());
  MockMetricsRecorder metrics_recorder;
  ConcurrentStreamRecordReader<std::string_view> record_reader(
      metrics_recorder, [&content]() {
        return std::make_unique<NonSeekingStringBlobStream>(content);
      });
  auto status = record_reader.ReadStreamRecords(callback.AsStdFunction());
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(status.message(), "Input streams do not support seeking.");
}

}  // namespace
}  // namespace kv_server
