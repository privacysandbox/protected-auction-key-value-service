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

namespace kv_server {
namespace {

using google::protobuf::TextFormat;

TEST(ReadRiegeliStreamRecordsTest, ReadRecord) {
  std::string content;
  riegeli::RecordWriterBase::Options options;
  riegeli::RecordsMetadata metadata;
  KVFileMetadata file_metadata;

  file_metadata.set_key_namespace(KeyNamespace::KEYS);
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
  auto reader =
      StreamRecordReaderFactory<std::string_view>::Create()->CreateReader(ss);
  EXPECT_THAT(reader->GetKVFileMetadata().value(), EqualsProto(file_metadata));
  EXPECT_TRUE(reader->ReadStreamRecords(callback.AsStdFunction()).ok());
}

TEST(ReadRiegeliStreamRecordsTest, ContinuesReadingRecordsOnError) {
  std::string content;
  riegeli::RecordWriterBase::Options options;
  riegeli::RecordsMetadata metadata;
  KVFileMetadata file_metadata;

  file_metadata.set_key_namespace(KeyNamespace::KEYS);
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
  auto reader =
      StreamRecordReaderFactory<std::string_view>::Create()->CreateReader(ss);
  EXPECT_TRUE(reader->ReadStreamRecords(callback.AsStdFunction()).ok());
}

TEST(ReadRiegeliStreamRecordsTest, BadStreamFailure) {
  std::stringstream ss;
  ss.setstate(std::ios_base::badbit);
  auto reader =
      StreamRecordReaderFactory<std::string_view>::Create()->CreateReader(ss);
  EXPECT_FALSE(
      reader
          ->ReadStreamRecords([](std::string_view) { return absl::OkStatus(); })
          .ok());
}

TEST(ReadRiegeliStreamRecordsTest, SkipsOverCorruption) {
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
  auto uncorrupted_reader =
      StreamRecordReaderFactory<std::string_view>::Create()->CreateReader(
          uncorrupted_ss);
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
  auto corrupted_reader =
      StreamRecordReaderFactory<std::string_view>::Create()->CreateReader(
          corrupted_ss);
  EXPECT_TRUE(
      corrupted_reader->ReadStreamRecords(callback.AsStdFunction()).ok());
}

}  // namespace
}  // namespace kv_server
