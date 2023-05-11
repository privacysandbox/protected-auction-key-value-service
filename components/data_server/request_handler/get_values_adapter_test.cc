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

#include "components/data_server/request_handler/get_values_adapter.h"

#include <string>
#include <vector>

#include "components/udf/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "public/test_util/proto_matcher.h"
#include "src/cpp/telemetry/metrics_recorder.h"
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;

using privacy_sandbox::server_common::MockMetricsRecorder;
using testing::_;
using testing::Return;

class GetValuesAdapterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    GetValuesV2Handler v2_handler(mock_udf_client_, mock_metrics_recorder_);
    get_values_adapter_ = GetValuesAdapter::Create(v2_handler);
  }

  std::unique_ptr<GetValuesAdapter> get_values_adapter_;
  MockUdfClient mock_udf_client_;
  MockMetricsRecorder mock_metrics_recorder_;
};

TEST_F(GetValuesAdapterTest, EmptyRequestReturnsEmptyResponse) {
  v1::GetValuesRequest v1_request;
  v1::GetValuesResponse v1_response;
  auto status = get_values_adapter_->CallV2Handler(v1_request, v1_response);
  EXPECT_TRUE(status.ok());
  v1::GetValuesResponse v1_expected;
  TextFormat::ParseFromString(R"pb()pb", &v1_expected);
  EXPECT_THAT(v1_response, EqualsProto(v1_expected));
}

}  // namespace
}  // namespace kv_server
