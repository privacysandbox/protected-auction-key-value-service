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

#include "components/udf/run_query_hook.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/internal_server/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using testing::_;
using testing::Return;
using testing::UnorderedElementsAre;

TEST(RunQueryHookTest, SuccessfullyProcessesValue) {
  std::string query = "Q";
  InternalRunQueryResponse run_query_response;
  TextFormat::ParseFromString(R"pb(elements: "a" elements: "b")pb",
                              &run_query_response);
  auto mrq = std::make_unique<MockRunQueryClient>();
  MockRunQueryClient* mock_run_query_client = mrq.get();
  EXPECT_CALL(*mock_run_query_client, RunQuery(query))
      .WillOnce(Return(run_query_response));

  auto input = std::make_tuple(query);
  auto run_query_hook = RunQueryHook::Create(
      [mrq = std::move(mrq)]() mutable { return std::move(mrq); });

  std::vector<std::string> result = (*run_query_hook)(input);
  EXPECT_THAT(result, UnorderedElementsAre("a", "b"));
}

TEST(GetValuesHookTest, RunQueryClientReturnsError) {
  std::string query = "Q";
  auto mrq = std::make_unique<MockRunQueryClient>();
  MockRunQueryClient* mock_run_query_client = mrq.get();
  EXPECT_CALL(*mock_run_query_client, RunQuery(query))
      .WillOnce(Return(absl::UnknownError("Some error")));

  auto input = std::make_tuple(query);
  auto run_query_hook = RunQueryHook::Create(
      [mrq = std::move(mrq)]() mutable { return std::move(mrq); });

  std::vector<std::string> result = (*run_query_hook)(input);
  EXPECT_TRUE(result.empty());
}

}  // namespace
}  // namespace kv_server
