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

#include <filesystem>
#include <fstream>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/data_loading/record_utils.h"
#include "public/test_util/proto_matcher.h"
#include "third_party/avro/api/DataFile.hh"
#include "third_party/avro/api/Schema.hh"
#include "third_party/avro/api/ValidSchema.hh"

namespace kv_server {
namespace {

constexpr std::string_view kTestRecord = "testrecord";
constexpr int64_t kIterations = 1024 * 1024 * 9;
constexpr size_t kAvroDefaultSyncInterval = 16 * 1024;
constexpr avro::Codec kAvroDefaultCodec = avro::Codec::NULL_CODEC;

void WriteInvalidFile(const std::vector<std::string_view>& records,
                      std::ostream& dest_stream) {
  dest_stream << "invalid";
}

void WriteAvroToFile(const std::vector<std::string_view>& records,
                     std::ostream& dest_stream, int64_t iterations,
                     const std::map<std::string, std::string>& metadata = {}) {
  avro::OutputStreamPtr avro_output_stream =
      avro::ostreamOutputStream(dest_stream);
  avro::DataFileWriter<std::string> record_writer(
      std::move(avro_output_stream), avro::ValidSchema(avro::BytesSchema()),
      kAvroDefaultSyncInterval, kAvroDefaultCodec, metadata);
  for (int64_t i = 0; i < iterations; i++) {
    for (const std::string_view& record : records) {
      record_writer.write(std::string(record));
    }
  }
  record_writer.close();
}

// This test can be used to debug Avro related operations.
// TEST(AvroStreamIO, Avro) {
//   const std::filesystem::path path =
//       std::filesystem::path(::testing::TempDir()) / kFileName;
//   std::ofstream output_stream(path);
//   WriteAvroToFile({kTestRecord}, output_stream);
//   output_stream.close();
//   std::ifstream is(path);
//   avro::InputStreamPtr input_stream = avro::istreamInputStream(is);

//   avro::DataFileReader<std::string> record_reader(std::move(input_stream));
//   // is.clear();
//   record_reader.sync(0);
//   int n = 0;
//   for (std::string record; record_reader.read(record);) {
//     ++n;
//   }
//   LOG(INFO) << "Read " << n << " records.";
//   record_reader.close();
// }

class iStreamRecordStream : public RecordStream {
 public:
  explicit iStreamRecordStream(const std::string& path) : stream_(path) {}
  std::istream& Stream() { return stream_; }

 private:
  std::ifstream stream_;
};

TEST(AvroStreamIO, ConcurrentReading) {
  kv_server::InitMetricsContextMap();
  constexpr std::string_view kFileName = "ConcurrentReading.avro";
  const std::filesystem::path path =
      std::filesystem::path(::testing::TempDir()) / kFileName;
  std::ofstream output_stream(path);
  WriteAvroToFile({kTestRecord}, output_stream, kIterations);
  output_stream.close();

  AvroConcurrentStreamRecordReader::Options options;
  AvroConcurrentStreamRecordReader record_reader(
      [&path] { return std::make_unique<iStreamRecordStream>(path); }, options);

  testing::MockFunction<absl::Status(const std::string_view&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(kIterations)
      .WillRepeatedly([](std::string_view raw) {
        EXPECT_EQ(raw, kTestRecord);
        return absl::OkStatus();
      });
  auto status =
      record_reader.ReadStreamRecords(record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(AvroStreamIO, SequentialReading) {
  kv_server::InitMetricsContextMap();
  constexpr std::string_view kFileName = "SequentialReading.avro";
  const std::filesystem::path path =
      std::filesystem::path(::testing::TempDir()) / kFileName;
  std::ofstream output_stream(path);
  WriteAvroToFile({kTestRecord}, output_stream, kIterations);
  output_stream.close();

  std::ifstream is(path);
  privacy_sandbox::server_common::log::NoOpContext log_context;
  AvroStreamReader record_reader(is, log_context);

  testing::MockFunction<absl::Status(const std::string_view&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(kIterations)
      .WillRepeatedly([](std::string_view raw) {
        EXPECT_EQ(raw, kTestRecord);
        return absl::OkStatus();
      });
  auto status =
      record_reader.ReadStreamRecords(record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(AvroStreamIO, ConcurrentReadingGetKVFileMetadata) {
  kv_server::InitMetricsContextMap();
  constexpr std::string_view kFileName = "ConcurrentReading.avro";
  const std::filesystem::path path =
      std::filesystem::path(::testing::TempDir()) / kFileName;
  std::ofstream output_stream(path);

  KVFileMetadata kv_metadata;
  kv_metadata.mutable_sharding_metadata()->set_shard_num(17);
  std::string kv_metadata_serialized;
  kv_metadata.SerializeToString(&kv_metadata_serialized);

  WriteAvroToFile({kTestRecord}, output_stream, 2,
                  {{kAvroKVFileMetadataKey, kv_metadata_serialized}});
  output_stream.close();

  AvroConcurrentStreamRecordReader::Options options;
  AvroConcurrentStreamRecordReader record_reader(
      [&path] { return std::make_unique<iStreamRecordStream>(path); }, options);

  auto actual_metadata = record_reader.GetKVFileMetadata();
  ASSERT_TRUE(actual_metadata.ok()) << actual_metadata.status();
  EXPECT_THAT(*actual_metadata, EqualsProto(kv_metadata));

  // Check that reading metadata does not interfere with reading records.
  testing::MockFunction<absl::Status(const std::string_view&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(2)
      .WillRepeatedly([](std::string_view raw) {
        EXPECT_EQ(raw, kTestRecord);
        return absl::OkStatus();
      });
  auto status =
      record_reader.ReadStreamRecords(record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(AvroStreamIO, SequentialReadingGetKVFileMetadata) {
  kv_server::InitMetricsContextMap();
  constexpr std::string_view kFileName = "SequentialReading.avro";
  const std::filesystem::path path =
      std::filesystem::path(::testing::TempDir()) / kFileName;
  std::ofstream output_stream(path);

  KVFileMetadata kv_metadata;
  kv_metadata.mutable_sharding_metadata()->set_shard_num(17);
  std::string kv_metadata_serialized;
  kv_metadata.SerializeToString(&kv_metadata_serialized);

  WriteAvroToFile({kTestRecord}, output_stream, 2,
                  {{kAvroKVFileMetadataKey, kv_metadata_serialized}});
  output_stream.close();

  std::ifstream is(path);
  privacy_sandbox::server_common::log::NoOpContext log_context;
  AvroStreamReader record_reader(is, log_context);
  auto actual_metadata = record_reader.GetKVFileMetadata();
  ASSERT_TRUE(actual_metadata.ok()) << actual_metadata.status();
  EXPECT_THAT(*actual_metadata, EqualsProto(kv_metadata));

  // Check that reading metadata does not interfere with reading records.
  testing::MockFunction<absl::Status(const std::string_view&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(2)
      .WillRepeatedly([](std::string_view raw) {
        EXPECT_EQ(raw, kTestRecord);
        return absl::OkStatus();
      });
  auto status =
      record_reader.ReadStreamRecords(record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(AvroStreamIO, ConcurrentReadingKVFileMetadataNotFound) {
  kv_server::InitMetricsContextMap();
  constexpr std::string_view kFileName = "ConcurrentReading.avro";
  const std::filesystem::path path =
      std::filesystem::path(::testing::TempDir()) / kFileName;
  std::ofstream output_stream(path);

  WriteAvroToFile({kTestRecord}, output_stream, 2);
  output_stream.close();

  AvroConcurrentStreamRecordReader::Options options;
  AvroConcurrentStreamRecordReader record_reader(
      [&path] { return std::make_unique<iStreamRecordStream>(path); }, options);

  auto actual_metadata = record_reader.GetKVFileMetadata();
  ASSERT_TRUE(actual_metadata.ok()) << actual_metadata.status();
  EXPECT_THAT(*actual_metadata, EqualsProto(KVFileMetadata()));

  testing::MockFunction<absl::Status(const std::string_view&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(2)
      .WillRepeatedly([](std::string_view raw) {
        EXPECT_EQ(raw, kTestRecord);
        return absl::OkStatus();
      });
  auto status =
      record_reader.ReadStreamRecords(record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(AvroStreamIO, SequentialReadingKVFileMetadataNotFound) {
  kv_server::InitMetricsContextMap();
  constexpr std::string_view kFileName = "SequentialReading.avro";
  const std::filesystem::path path =
      std::filesystem::path(::testing::TempDir()) / kFileName;
  std::ofstream output_stream(path);

  WriteAvroToFile({kTestRecord}, output_stream, 2);
  output_stream.close();

  std::ifstream is(path);
  privacy_sandbox::server_common::log::NoOpContext log_context;
  AvroStreamReader record_reader(is, log_context);
  auto actual_metadata = record_reader.GetKVFileMetadata();
  ASSERT_TRUE(actual_metadata.ok()) << actual_metadata.status();
  EXPECT_THAT(*actual_metadata, EqualsProto(KVFileMetadata()));

  testing::MockFunction<absl::Status(const std::string_view&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(2)
      .WillRepeatedly([](std::string_view raw) {
        EXPECT_EQ(raw, kTestRecord);
        return absl::OkStatus();
      });
  auto status =
      record_reader.ReadStreamRecords(record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(AvroStreamIO, ConcurrentReadingKVMetadataInvalidProto) {
  kv_server::InitMetricsContextMap();
  constexpr std::string_view kFileName = "ConcurrentReading.avro";
  const std::filesystem::path path =
      std::filesystem::path(::testing::TempDir()) / kFileName;
  std::ofstream output_stream(path);

  WriteAvroToFile({kTestRecord}, output_stream, 2,
                  {{kAvroKVFileMetadataKey, "not a KVFileMetadataProto"}});
  output_stream.close();

  AvroConcurrentStreamRecordReader::Options options;
  AvroConcurrentStreamRecordReader record_reader(
      [&path] { return std::make_unique<iStreamRecordStream>(path); }, options);

  auto actual_metadata = record_reader.GetKVFileMetadata();
  ASSERT_FALSE(actual_metadata.ok()) << actual_metadata.status();

  // Check that reading metadata does not interfere with reading records.
  testing::MockFunction<absl::Status(const std::string_view&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(2)
      .WillRepeatedly([](std::string_view raw) {
        EXPECT_EQ(raw, kTestRecord);
        return absl::OkStatus();
      });
  auto status =
      record_reader.ReadStreamRecords(record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(AvroStreamIO, SequentialReadingKVMetadataInvalidProto) {
  kv_server::InitMetricsContextMap();
  constexpr std::string_view kFileName = "SequentialReading.avro";
  const std::filesystem::path path =
      std::filesystem::path(::testing::TempDir()) / kFileName;
  std::ofstream output_stream(path);

  WriteAvroToFile({kTestRecord}, output_stream, 2,
                  {{kAvroKVFileMetadataKey, "not a KVFileMetadataProto"}});
  output_stream.close();

  std::ifstream is(path);
  privacy_sandbox::server_common::log::NoOpContext log_context;
  AvroStreamReader record_reader(is, log_context);
  auto actual_metadata = record_reader.GetKVFileMetadata();
  ASSERT_FALSE(actual_metadata.ok()) << actual_metadata.status();

  // Check that reading metadata does not interfere with reading records.
  testing::MockFunction<absl::Status(const std::string_view&)> record_callback;
  EXPECT_CALL(record_callback, Call)
      .Times(2)
      .WillRepeatedly([](std::string_view raw) {
        EXPECT_EQ(raw, kTestRecord);
        return absl::OkStatus();
      });
  auto status =
      record_reader.ReadStreamRecords(record_callback.AsStdFunction());
  EXPECT_TRUE(status.ok()) << status;
}

TEST(AvroStreamIO, ConcurrentReadingInvalidFile) {
  kv_server::InitMetricsContextMap();
  constexpr std::string_view kFileName = "ConcurrentReading.invalid";
  const std::filesystem::path path =
      std::filesystem::path(::testing::TempDir()) / kFileName;
  std::ofstream output_stream(path);
  WriteInvalidFile({kTestRecord}, output_stream);
  output_stream.close();

  AvroConcurrentStreamRecordReader::Options options;
  AvroConcurrentStreamRecordReader record_reader(
      [&path] { return std::make_unique<iStreamRecordStream>(path); }, options);

  testing::MockFunction<absl::Status(const std::string_view&)> record_callback;
  EXPECT_CALL(record_callback, Call).Times(0);
  auto status =
      record_reader.ReadStreamRecords(record_callback.AsStdFunction());
  EXPECT_FALSE(status.ok());
}

TEST(AvroStreamIO, SequentialReadingInvalidFile) {
  kv_server::InitMetricsContextMap();
  constexpr std::string_view kFileName = "SequentialReading.invalid";
  const std::filesystem::path path =
      std::filesystem::path(::testing::TempDir()) / kFileName;
  std::ofstream output_stream(path);
  WriteInvalidFile({kTestRecord}, output_stream);
  output_stream.close();

  std::ifstream is(path);
  privacy_sandbox::server_common::log::NoOpContext log_context;
  AvroStreamReader record_reader(is, log_context);

  testing::MockFunction<absl::Status(const std::string_view&)> record_callback;
  EXPECT_CALL(record_callback, Call).Times(0);
  auto status =
      record_reader.ReadStreamRecords(record_callback.AsStdFunction());
  EXPECT_FALSE(status.ok());
}

}  // namespace
}  // namespace kv_server
