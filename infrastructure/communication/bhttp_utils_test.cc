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

#include "infrastructure/communication/bhttp_utils.h"

#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"

namespace {

using google::protobuf::Struct;
using quiche::BinaryHttpRequest;
using quiche::BinaryHttpResponse;

absl::StatusOr<std::string> CreateBHTTPRequest(std::string body) {
  std::string_view host = "fixme.example.com";

  BinaryHttpRequest::ControlData cd;
  cd.method = "POST";
  cd.authority = host;
  cd.scheme = "https";
  cd.path = "/v1/getvalues?kv_internal=hi";
  BinaryHttpRequest req(std::move(cd));
  req.set_body(std::move(body));

  return req.Serialize();
}

TEST(BinaryHttpTest, DeserializeBHttpToProto) {
  const auto maybe_req = CreateBHTTPRequest(R"({"key": "value"})");
  ASSERT_TRUE(maybe_req.ok());

  const auto maybe_proto =
      kv_server::DeserializeBHttpToProto<BinaryHttpRequest, Struct>(
          maybe_req.value());
  ASSERT_TRUE(maybe_proto.ok());
  EXPECT_EQ(maybe_proto->fields().at("key").string_value(), "value");
}

TEST(BinaryHttpTest, SerializeProtoToBHttpResponse) {
  Struct struct_proto;
  (*struct_proto.mutable_fields())["key"].set_string_value("value");

  auto maybe_serialized_bresp =
      kv_server::SerializeProtoToBHttp<BinaryHttpResponse, Struct>(struct_proto,
                                                                   200);
  ASSERT_TRUE(maybe_serialized_bresp.ok());

  auto maybe_bhttp = BinaryHttpResponse::Create(maybe_serialized_bresp.value());
  ASSERT_TRUE(maybe_bhttp.ok());

  EXPECT_EQ(maybe_bhttp->body(), R"({"key":"value"})");
  EXPECT_EQ(maybe_bhttp->status_code(), 200);
}

TEST(BinaryHttpTest, SerializeProtoToBHttpRequest) {
  Struct struct_proto;
  (*struct_proto.mutable_fields())["key"].set_string_value("value");

  auto maybe_serialized_breq =
      kv_server::SerializeProtoToBHttp<BinaryHttpRequest, Struct>(struct_proto,
                                                                  {});
  ASSERT_TRUE(maybe_serialized_breq.ok());

  auto maybe_bhttp = BinaryHttpRequest::Create(maybe_serialized_breq.value());
  ASSERT_TRUE(maybe_bhttp.ok()) << maybe_bhttp.status();

  EXPECT_EQ(maybe_bhttp->body(), R"({"key":"value"})");
}

}  // namespace
