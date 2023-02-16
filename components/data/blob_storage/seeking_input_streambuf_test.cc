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
#include <limits>
#include <sstream>
#include <streambuf>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

SeekingInputStreambuf::Options GetOptions(int64_t buffer_size) {
  SeekingInputStreambuf::Options options;
  options.buffer_size = buffer_size;
  return options;
}

class StringBlobInputStreambuf : public SeekingInputStreambuf {
 public:
  StringBlobInputStreambuf(std::string_view blob,
                           SeekingInputStreambuf::Options options)
      : SeekingInputStreambuf(std::move(options)), blob_(blob) {}

 protected:
  absl::StatusOr<int64_t> ReadChunk(int64_t offset, int64_t chunk_size,
                                    char* dest_buffer) override {
    if (offset < 0 || offset >= blob_.length()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Offset must be in range [%d, %d] inclusive.", 0,
                          blob_.length() - 1));
    }
    auto begin = blob_.data() + offset;
    std::copy(begin, begin + chunk_size, dest_buffer);
    return chunk_size;
  }
  absl::StatusOr<int64_t> SizeImpl() override { return blob_.length(); }

 private:
  std::string blob_;
};

class SeekingInputStreambufTest
    : public testing::TestWithParam<SeekingInputStreambuf::Options> {
 protected:
  StringBlobInputStreambuf CreateStringBlobStreambuf(std::string_view blob) {
    return StringBlobInputStreambuf(blob, GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(
    BufferSize, SeekingInputStreambufTest,
    testing::Values(
        GetOptions(/*buffer_size=*/0), GetOptions(/*buffer_size=*/1 << 4),
        GetOptions(/*buffer_size=*/std::numeric_limits<int64_t>::max())));

TEST_P(SeekingInputStreambufTest, VerifyCanReadEntireBlob) {
  constexpr std::string_view blob =
      "I am a very random blob with random bits of data.";
  auto streambuf = SeekingInputStreambufTest::CreateStringBlobStreambuf(blob);
  std::istream blob_stream(&streambuf);
  // Read entire blob into a string stream.
  std::stringstream ss;
  ss << blob_stream.rdbuf();
  EXPECT_EQ(std::string(blob), ss.str());
}

TEST_P(SeekingInputStreambufTest, VerifySeekingByOffsetFromDirection) {
  constexpr std::string_view blob =
      "I am a very random blob with random bits of data.";
  auto streambuf = SeekingInputStreambufTest::CreateStringBlobStreambuf(blob);
  std::istream blob_stream(&streambuf);
  // Calculate blob size using seeking.
  blob_stream.seekg(0, std::ios_base::end);
  auto blob_size = blob_stream.tellg();
  EXPECT_EQ(blob_size, blob.length());
  // Rewind to beginning of stream.
  blob_stream.seekg(0, std::ios_base::beg);
  EXPECT_EQ(blob_stream.tellg(), 0);
  // Read entire blob into a string stream.
  std::stringstream ss1;
  ss1 << blob_stream.rdbuf();
  EXPECT_EQ(ss1.str(), blob);
  EXPECT_EQ(blob_stream.tellg(), blob.length());
  // Rewind 10 characters from the end of the blob.
  blob_stream.seekg(-10, std::ios_base::end);
  EXPECT_EQ(blob_stream.tellg(), blob.length() - 10);
  // Seek forward by 5 from beginning (skips "I am ") and then 7 more characters
  // forward from current position (skips "a very ").
  blob_stream.seekg(5, std::ios_base::beg);
  blob_stream.seekg(7, std::ios_base::cur);
  std::stringstream ss2;
  ss2 << blob_stream.rdbuf();
  EXPECT_EQ(ss2.str(), "random blob with random bits of data.");
  // Seek forward by 5 from beginning (skips "I am ") and then -5 characters
  // from current position (goes back to beginning).
  blob_stream.seekg(5, std::ios_base::beg);
  blob_stream.seekg(-5, std::ios_base::cur);
  std::stringstream ss3;
  ss3 << blob_stream.rdbuf();
  EXPECT_EQ(ss3.str(), blob);
}

TEST_P(SeekingInputStreambufTest, VerifySeekingToAbsolutePosition) {
  constexpr std::string_view blob =
      "I am a very random blob with random bits of data.";
  auto streambuf = SeekingInputStreambufTest::CreateStringBlobStreambuf(blob);
  std::istream blob_stream(&streambuf);
  blob_stream.seekg(12);
  std::stringstream ss1;
  ss1 << blob_stream.rdbuf();
  EXPECT_EQ(ss1.str(), "random blob with random bits of data.");
  // Seeking to the maximum possible position should leave us at the end of the
  // blob.
  blob_stream.seekg(std::numeric_limits<int64_t>::max());
  std::stringstream ss2;
  ss2 << blob_stream.rdbuf();
  EXPECT_EQ(ss2.str(), "");
  EXPECT_EQ(blob_stream.tellg(), blob.length());
}

TEST_P(SeekingInputStreambufTest, VerifySeekingWithWrongSignedOffsets) {
  constexpr std::string_view blob =
      "I am a very random blob with random bits of data.";
  auto streambuf = SeekingInputStreambufTest::CreateStringBlobStreambuf(blob);
  std::istream blob_stream(&streambuf);
  blob_stream.seekg(-10, std::ios_base::beg);
  EXPECT_EQ(blob_stream.tellg(), -1);
  // Reset stream and seek to beginning.
  blob_stream.clear();
  blob_stream.seekg(0, std::ios_base::beg);
  EXPECT_EQ(blob_stream.tellg(), 0);
  // Now seek to a positive offset from the end.
  blob_stream.seekg(10, std::ios_base::end);
  EXPECT_EQ(blob_stream.tellg(), -1);
}

}  // namespace
}  // namespace kv_server
