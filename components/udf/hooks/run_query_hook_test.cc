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

#include "components/udf/hooks/run_query_hook.h"

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
using google::scp::roma::FunctionBindingPayload;
using google::scp::roma::proto::FunctionBindingIoProto;
using testing::_;
using testing::Return;
using testing::UnorderedElementsAreArray;

class RunQueryHookTest : public ::testing::Test {
 protected:
  void SetUp() override { InitMetricsContextMap(); }
};

TEST_F(RunQueryHookTest, SuccessfullyProcessesValue) {
  std::string query = "Q";
  InternalRunQueryResponse run_query_response;
  TextFormat::ParseFromString(R"pb(elements: "a" elements: "b")pb",
                              &run_query_response);
  auto mock_lookup = std::make_unique<MockLookup>();
  EXPECT_CALL(*mock_lookup, RunQuery(query))
      .WillOnce(Return(run_query_response));

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(R"pb(input_string: "Q")pb", &io);
  auto run_query_hook = RunQueryHook::Create();
  run_query_hook->FinishInit(std::move(mock_lookup));
  ScopeMetricsContext metrics_context;
  RequestContext request_context(metrics_context.GetMetricsContext());
  FunctionBindingPayload<RequestContext> payload{io, request_context};
  (*run_query_hook)(payload);
  EXPECT_THAT(io.output_list_of_string().data(),
              UnorderedElementsAreArray({"a", "b"}));
}

TEST_F(RunQueryHookTest, RunQueryClientReturnsError) {
  std::string query = "Q";
  auto mock_lookup = std::make_unique<MockLookup>();
  EXPECT_CALL(*mock_lookup, RunQuery(query))
      .WillOnce(Return(absl::UnknownError("Some error")));

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(R"pb(input_string: "Q")pb", &io);
  auto run_query_hook = RunQueryHook::Create();
  run_query_hook->FinishInit(std::move(mock_lookup));
  ScopeMetricsContext metrics_context;
  RequestContext request_context(metrics_context.GetMetricsContext());
  FunctionBindingPayload<RequestContext> payload{io, request_context};
  (*run_query_hook)(payload);
  EXPECT_TRUE(io.output_list_of_string().data().empty());
}

TEST_F(RunQueryHookTest, InputIsNotString) {
  auto mock_lookup = std::make_unique<MockLookup>();

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(R"pb(input_list_of_string { data: "key1" })pb",
                              &io);
  auto run_query_hook = RunQueryHook::Create();
  run_query_hook->FinishInit(std::move(mock_lookup));
  ScopeMetricsContext metrics_context;
  RequestContext request_context(metrics_context.GetMetricsContext());
  FunctionBindingPayload<RequestContext> payload{io, request_context};
  (*run_query_hook)(payload);

  EXPECT_THAT(
      io.output_list_of_string().data(),
      UnorderedElementsAreArray(
          {R"({"code":3,"message":"runQuery input must be a string"})"}));
}

}  // namespace
}  // namespace kv_server
