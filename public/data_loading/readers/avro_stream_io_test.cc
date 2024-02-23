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
#include "third_party/avro/api/DataFile.hh"
#include "third_party/avro/api/Schema.hh"
#include "third_party/avro/api/ValidSchema.hh"

namespace kv_server {
namespace {

constexpr std::string_view kTestRecord = "testrecord";
constexpr int64_t kIterations = 1024 * 1024 * 9;

void WriteInvalidFile(const std::vector<std::string_view>& records,
                      std::ostream& dest_stream) {
  dest_stream << "invalid";
}

void WriteAvroToFile(const std::vector<std::string_view>& records,
                     std::ostream& dest_stream) {
  avro::OutputStreamPtr avro_output_stream =
      avro::ostreamOutputStream(dest_stream);
  avro::DataFileWriter<std::string> record_writer(
      std::move(avro_output_stream), avro::ValidSchema(avro::BytesSchema()));
  for (int64_t i = 0; i < kIterations; i++) {
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
  WriteAvroToFile({kTestRecord}, output_stream);
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
  WriteAvroToFile({kTestRecord}, output_stream);
  output_stream.close();

  std::ifstream is(path);
  AvroStreamReader record_reader(is);

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
  AvroStreamReader record_reader(is);

  testing::MockFunction<absl::Status(const std::string_view&)> record_callback;
  EXPECT_CALL(record_callback, Call).Times(0);
  auto status =
      record_reader.ReadStreamRecords(record_callback.AsStdFunction());
  EXPECT_FALSE(status.ok());
}

}  // namespace
}  // namespace kv_server
