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

#include "tools/bidding_auction_data_generator/json_to_proto_util.h"

#include <filesystem>
#include <fstream>

#include "gmock/gmock.h"
#include "google/protobuf/util/json_util.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

namespace kv_server {
namespace {

constexpr std::string_view buyer_byos_kv_response_dir =
    "tools/bidding_auction_data_generator/data/test/buyer_kv_byos_response";
constexpr std::string_view seller_byos_kv_response_dir =
    "tools/bidding_auction_data_generator/data/test/seller_kv_byos_response";

class ParametrizedReadBYOSKVResponseTest
    : public ::testing::TestWithParam<std::string> {};

INSTANTIATE_TEST_SUITE_P(FileDirectories, ParametrizedReadBYOSKVResponseTest,
                         testing::Values(buyer_byos_kv_response_dir,
                                         seller_byos_kv_response_dir));

TEST_P(ParametrizedReadBYOSKVResponseTest, ReadResponseSuccess) {
  const std::string file_dir = GetParam();
  auto file_paths = kv_server::GetInputJsonFilePaths(file_dir);
  EXPECT_TRUE(file_paths.ok());
  EXPECT_EQ(file_paths->size(), 1);
  auto input_file = file_paths->begin();
  std::cerr << "file path " << *input_file;
  std::ifstream in_stream(*input_file);
  std::ostringstream input_json_stream;
  input_json_stream << in_stream.rdbuf();
  in_stream.close();
  auto input_json = nlohmann::json::parse(input_json_stream.str());
  // Remove spaces and backslashes
  const std::string input_formatted_json_string = input_json.dump(0);
  auto response_proto = kv_server::ReadGetValueResponseJsonFile(*input_file);
  EXPECT_TRUE(response_proto.ok());
  std::string output_in_json;
  EXPECT_TRUE(google::protobuf::util::MessageToJsonString(*response_proto,
                                                          &output_in_json)
                  .ok());
  auto output_formatted_json_string =
      nlohmann::json::parse(output_in_json).dump(0);
  EXPECT_EQ(input_formatted_json_string, output_formatted_json_string);
}
}  // namespace
}  // namespace kv_server
