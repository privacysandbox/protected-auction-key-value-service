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

#include "public/query/cpp/grpc_client.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/query/v2/get_values_v2_mock.grpc.pb.h"
#include "public/test_util/proto_matcher.h"

namespace kv_server {
namespace {

using testing::_;

TEST(GrpcClient, Success) {
  v2::MockKeyValueServiceStub stub;
  v2::GetValuesResponse resp;
  resp.mutable_single_partition()->set_string_output("hello world");

  v2::GetValuesRequest request;
  request.set_client_version("client version");

  EXPECT_CALL(stub, GetValues(_, EqualsProto(request), _))
      .Times(1)
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(resp),
                               testing::Return(grpc::Status::OK)));

  GrpcClient client(stub);
  auto maybe_response = client.GetValues(request);

  ASSERT_TRUE(maybe_response.ok());
  EXPECT_THAT(maybe_response.value(), EqualsProto(resp));
}
}  // namespace
}  // namespace kv_server
