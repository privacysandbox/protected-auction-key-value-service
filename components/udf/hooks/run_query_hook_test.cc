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
#include <utility>

#include "absl/status/status.h"
#include "components/internal_server/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "public/test_util/proto_matcher.h"

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
  RunQueryHookTest() {
    InitMetricsContextMap();
    request_context_ = std::make_unique<RequestContext>();
    request_context_->UpdateLogContext(
        privacy_sandbox::server_common::LogContext(),
        privacy_sandbox::server_common::ConsentedDebugConfiguration());
  }
  std::shared_ptr<RequestContext> GetRequestContext() {
    return request_context_;
  }
  std::shared_ptr<RequestContext> request_context_;
};

TEST_F(RunQueryHookTest, SuccessfullyProcessesValue) {
  std::string query = "Q";
  InternalRunQueryResponse run_query_response;
  TextFormat::ParseFromString(R"pb(elements: "a" elements: "b")pb",
                              &run_query_response);
  auto mock_lookup = std::make_unique<MockLookup>();
  EXPECT_CALL(*mock_lookup, RunQuery(_, query))
      .WillOnce(Return(run_query_response));

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(R"pb(input_string: "Q")pb", &io);
  auto run_query_hook = RunSetQueryStringHook::Create();
  run_query_hook->FinishInit(std::move(mock_lookup));
  FunctionBindingPayload<std::weak_ptr<RequestContext>> payload{
      io, GetRequestContext()};
  (*run_query_hook)(payload);
  EXPECT_THAT(io.output_list_of_string().data(),
              UnorderedElementsAreArray({"a", "b"}));
}

TEST_F(RunQueryHookTest, VerifyProcessingIntSetsSuccessfully) {
  InternalRunSetQueryIntResponse run_query_response;
  TextFormat::ParseFromString(R"pb(elements: 1000 elements: 1001)pb",
                              &run_query_response);
  auto mock_lookup = std::make_unique<MockLookup>();
  EXPECT_CALL(*mock_lookup, RunSetQueryInt(_, "Q"))
      .WillOnce(Return(run_query_response));
  FunctionBindingIoProto io;
  TextFormat::ParseFromString(R"pb(input_string: "Q")pb", &io);
  auto run_query_hook = RunSetQueryIntHook::Create();
  run_query_hook->FinishInit(std::move(mock_lookup));
  FunctionBindingPayload<std::weak_ptr<RequestContext>> payload{
      io, GetRequestContext()};
  (*run_query_hook)(payload);
  ASSERT_TRUE(io.has_output_bytes());
  InternalRunSetQueryIntResponse actual_response;
  actual_response.mutable_elements()->Resize(
      io.output_bytes().size() / sizeof(uint32_t), 0);
  std::memcpy(actual_response.mutable_elements()->mutable_data(),
              io.output_bytes().data(), io.output_bytes().size());
  EXPECT_THAT(actual_response, EqualsProto(run_query_response));
}

TEST_F(RunQueryHookTest, RunQueryClientReturnsError) {
  std::string query = "Q";
  auto mock_lookup = std::make_unique<MockLookup>();
  EXPECT_CALL(*mock_lookup, RunQuery(_, query))
      .WillOnce(Return(absl::UnknownError("Some error")));

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(R"pb(input_string: "Q")pb", &io);
  auto run_query_hook = RunSetQueryStringHook::Create();
  run_query_hook->FinishInit(std::move(mock_lookup));
  FunctionBindingPayload<std::weak_ptr<RequestContext>> payload{
      io, GetRequestContext()};
  (*run_query_hook)(payload);
  EXPECT_THAT(
      io.output_list_of_string().data(),
      UnorderedElementsAreArray(
          {R"({"code":2,"message":"runQuery failed with error: Some error"})"}));
}

TEST_F(RunQueryHookTest, RunSetQueryIntClientReturnsError) {
  auto mock_lookup = std::make_unique<MockLookup>();
  EXPECT_CALL(*mock_lookup, RunSetQueryInt(_, "Q"))
      .WillOnce(Return(absl::UnknownError("Some error")));
  FunctionBindingIoProto io;
  TextFormat::ParseFromString(R"pb(input_string: "Q")pb", &io);
  auto run_query_hook = RunSetQueryIntHook::Create();
  run_query_hook->FinishInit(std::move(mock_lookup));
  FunctionBindingPayload<std::weak_ptr<RequestContext>> payload{
      io, GetRequestContext()};
  (*run_query_hook)(payload);
  EXPECT_THAT(
      io.output_list_of_string().data(),
      UnorderedElementsAreArray(
          {R"({"code":2,"message":"runSetQueryInt failed with error: Some error"})"}));
}

TEST_F(RunQueryHookTest, InputIsNotString) {
  auto mock_lookup = std::make_unique<MockLookup>();

  FunctionBindingIoProto io;
  TextFormat::ParseFromString(R"pb(input_list_of_string { data: "key1" })pb",
                              &io);
  auto run_query_hook = RunSetQueryStringHook::Create();
  run_query_hook->FinishInit(std::move(mock_lookup));
  FunctionBindingPayload<std::weak_ptr<RequestContext>> payload{
      io, GetRequestContext()};
  (*run_query_hook)(payload);
  EXPECT_THAT(
      io.output_list_of_string().data(),
      UnorderedElementsAreArray(
          {R"({"code":3,"message":"runQuery input must be a string"})"}));
}

}  // namespace
}  // namespace kv_server
