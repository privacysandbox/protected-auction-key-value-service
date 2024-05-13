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

#include "tools/request_simulation/request_generation_util.h"

#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "google/protobuf/util/json_util.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

TEST(TestCreateMessage, ProtoMessageMatchJson) {
  const auto keys = kv_server::GenerateRandomKeys(10, 3);
  const std::string request_in_json =
      kv_server::CreateKVDSPRequestBodyInJson(keys, "debug_token");
  const auto request = kv_server::CreatePlainTextRequest(request_in_json);
  EXPECT_EQ(request_in_json, request.raw_body().data());
  std::string encoded_request_body;
  google::protobuf::util::MessageToJsonString(request.raw_body(),
                                              &encoded_request_body);
  std::string expect_encoded_request_body = absl::StrCat(
      "{\"data\":", "\"", absl::Base64Escape(request_in_json), "\"", "}");
  EXPECT_EQ(encoded_request_body, expect_encoded_request_body);
}
}  // namespace
}  // namespace kv_server
