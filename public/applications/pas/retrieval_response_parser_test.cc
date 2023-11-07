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

#include "public/applications/pas/retrieval_response_parser.h"

#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"

namespace kv_server::application_pas {
namespace {

using google::protobuf::TextFormat;

TEST(RetrievalResponseParser, GetRetrievalOutput) {
  v2::GetValuesResponse response;
  TextFormat::ParseFromString(
      R"pb(
single_partition {
  string_output: "output"
}
        })pb",
      &response);
  auto maybe_output = GetRetrievalOutput(response);
  ASSERT_TRUE(maybe_output.ok());
  EXPECT_EQ(*maybe_output, "output");
}

TEST(RetrievalResponseParser, GetRetrievalOutputError) {
  v2::GetValuesResponse response;
  TextFormat::ParseFromString(
      R"pb(
single_partition {
  status {
    code: 1
  }
}
        })pb",
      &response);
  auto maybe_output = GetRetrievalOutput(response);
  ASSERT_FALSE(maybe_output.ok());
  EXPECT_EQ(maybe_output.status().code(), absl::StatusCode::kCancelled);
}

}  // namespace
}  // namespace kv_server::application_pas
