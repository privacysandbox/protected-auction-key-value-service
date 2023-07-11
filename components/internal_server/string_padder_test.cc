// Copyright 2023 Google LLC
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

#include "components/internal_server/string_padder.h"

#include <string_view>

#include "gtest/gtest.h"

namespace kv_server {
namespace {

TEST(PadUnpad, Success) {
  const std::string_view kTestString = "string to pad";
  const int32_t padding_size = 100;
  auto padded_string = kv_server::Pad(kTestString, padding_size);
  int32_t expected_length =
      sizeof(u_int32_t) + kTestString.size() + padding_size;
  EXPECT_EQ(expected_length, padded_string.size());
  auto original_string_status = Unpad(padded_string);
  ASSERT_TRUE(original_string_status.ok());
  EXPECT_EQ(*original_string_status, kTestString);
}

TEST(PadUnpadZeroPadding, Success) {
  const std::string_view kTestString = "string to pad";
  const int32_t padding_size = 0;
  auto padded_string = kv_server::Pad(kTestString, padding_size);
  int32_t expected_length =
      sizeof(u_int32_t) + kTestString.size() + padding_size;
  EXPECT_EQ(expected_length, padded_string.size());
  auto original_string_status = Unpad(padded_string);
  ASSERT_TRUE(original_string_status.ok());
  EXPECT_EQ(*original_string_status, kTestString);
}

TEST(PadUnpadEmtpyString, Success) {
  const std::string_view kTestString = "";
  const int32_t padding_size = 100;
  auto padded_string = kv_server::Pad(kTestString, padding_size);
  int32_t expected_length =
      sizeof(u_int32_t) + kTestString.size() + padding_size;
  EXPECT_EQ(expected_length, padded_string.size());
  auto original_string_status = Unpad(padded_string);
  ASSERT_TRUE(original_string_status.ok());
  EXPECT_EQ(*original_string_status, kTestString);
}

TEST(UnpadFailure, Success) {
  auto original_string_status = Unpad("garbage");
  ASSERT_FALSE(original_string_status.ok());
}

}  // namespace
}  // namespace kv_server
