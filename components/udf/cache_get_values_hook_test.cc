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
#include "components/udf/cache_get_values_hook.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/key_value_cache.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using google::scp::roma::proto::FunctionBindingIoProto;
using testing::_;
using testing::Return;

TEST(GetValuesHookTest, SuccessfullyReturnsKVPairs) {
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  cache->UpdateKeyValue("key1", "value1", 1);
  cache->UpdateKeyValue("key2", "value2", 1);

  auto get_values_hook = NewCacheGetValuesHook(*cache);

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(
      R"pb(input_list_of_string { data: "key1" data: "key2" })pb", &io);
  (*get_values_hook)(io);

  nlohmann::json result_json =
      nlohmann::json::parse(io.output_string(), nullptr,
                            /*allow_exceptions=*/false,
                            /*ignore_comments=*/true);
  EXPECT_FALSE(result_json.is_discarded());
  nlohmann::json expected_value1 = R"({"value":"value1"})"_json;
  nlohmann::json expected_value2 = R"({"value":"value2"})"_json;
  EXPECT_TRUE(result_json.contains("kvPairs"));
  EXPECT_EQ(result_json["kvPairs"]["key1"], expected_value1);
  EXPECT_EQ(result_json["kvPairs"]["key2"], expected_value2);
}

}  // namespace
}  // namespace kv_server
