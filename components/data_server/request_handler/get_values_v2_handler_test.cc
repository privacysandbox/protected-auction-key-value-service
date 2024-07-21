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

#include "components/data_server/request_handler/get_values_v2_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/mocks.h"
#include "components/data_server/request_handler/framing_utils.h"
#include "components/udf/mocks.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "public/constants.h"
#include "public/test_util/proto_matcher.h"
#include "public/test_util/request_example.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "quiche/oblivious_http/oblivious_http_client.h"
#include "src/communication/encoding_utils.h"
#include "src/encryption/key_fetcher/fake_key_fetcher_manager.h"

namespace kv_server {
namespace {

using google::protobuf::TextFormat;
using grpc::StatusCode;
using testing::_;
using testing::Return;
using testing::ReturnRef;
using testing::UnorderedElementsAre;
using v2::GetValuesHttpRequest;
using v2::ObliviousGetValuesRequest;

enum class ProtocolType {
  kPlain = 0,
  kObliviousHttp,
};

struct TestingParameters {
  ProtocolType protocol_type;
  const std::string_view content_type;
  const std::string_view core_request_body;
  const bool is_consented;
};

class GetValuesHandlerTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<TestingParameters> {
 protected:
  void SetUp() override {
    privacy_sandbox::server_common::log::ServerToken(
        kExampleConsentedDebugToken);
    InitMetricsContextMap();
  }
  template <ProtocolType protocol_type>
  bool IsUsing() {
    auto param = GetParam();
    return param.protocol_type == protocol_type;
  }

  bool IsProtobufContent() {
    auto param = GetParam();
    return param.content_type == kContentEncodingProtoHeaderValue;
  }

  bool IsRequestExpectConsented() {
    auto param = GetParam();
    return param.is_consented;
  }

  std::string GetTestRequestBody() {
    auto param = GetParam();
    return std::string(param.core_request_body);
  }

  class PlainRequest {
   public:
    explicit PlainRequest(std::string plain_request_body)
        : plain_request_body_(std::move(plain_request_body)) {}

    GetValuesHttpRequest Build() const {
      GetValuesHttpRequest request;
      request.mutable_raw_body()->set_data(plain_request_body_);
      return request;
    }

    const std::string& RequestBody() const { return plain_request_body_; }

   private:
    std::string plain_request_body_;
  };

  class OHTTPRequest;
  class OHTTPResponseUnwrapper {
   public:
    google::api::HttpBody& RawResponse() { return response_; }

    std::string Unwrap() {
      uint8_t key_id = 64;
      auto maybe_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
          key_id, kKEMParameter, kKDFParameter, kAEADParameter);
      EXPECT_TRUE(maybe_config.ok());
      auto client =
          quiche::ObliviousHttpClient::Create(public_key_, *maybe_config);
      EXPECT_TRUE(client.ok());
      auto decrypted_response =
          client->DecryptObliviousHttpResponse(response_.data(), context_);

      auto deframed_req = privacy_sandbox::server_common::DecodeRequestPayload(
          decrypted_response->GetPlaintextData());
      EXPECT_TRUE(deframed_req.ok()) << deframed_req.status();
      return deframed_req->compressed_data;
    }

   private:
    explicit OHTTPResponseUnwrapper(
        quiche::ObliviousHttpRequest::Context context)
        : context_(std::move(context)) {}

    google::api::HttpBody response_;
    quiche::ObliviousHttpRequest::Context context_;
    const std::string public_key_ = absl::HexStringToBytes(kTestPublicKey);

    friend class OHTTPRequest;
  };

  class OHTTPRequest {
   public:
    explicit OHTTPRequest(std::string raw_request)
        : raw_request_(std::move(raw_request)) {}

    std::pair<ObliviousGetValuesRequest, OHTTPResponseUnwrapper> Build() const {
      // matches the test key pair, see common repo:
      // ../encryption/key_fetcher/src/fake_key_fetcher_manager.h
      uint8_t key_id = 64;
      auto maybe_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
          key_id, kKEMParameter, kKDFParameter, kAEADParameter);
      EXPECT_TRUE(maybe_config.ok());

      auto client =
          quiche::ObliviousHttpClient::Create(public_key_, *maybe_config);
      EXPECT_TRUE(client.ok());
      auto encrypted_req = client->CreateObliviousHttpRequest(raw_request_);
      EXPECT_TRUE(encrypted_req.ok());
      auto serialized_encrypted_req = encrypted_req->EncapsulateAndSerialize();
      ObliviousGetValuesRequest ohttp_req;
      ohttp_req.mutable_raw_body()->set_data(serialized_encrypted_req);

      OHTTPResponseUnwrapper response_unwrapper(
          std::move(encrypted_req.value()).ReleaseContext());
      return {std::move(ohttp_req), std::move(response_unwrapper)};
    }

   private:
    const std::string public_key_ = absl::HexStringToBytes(kTestPublicKey);
    std::string raw_request_;
  };

  // For Non-plain protocols, test request and response data are converted
  // to/from the corresponding request/responses.
  grpc::Status GetValuesBasedOnProtocol(
      RequestContextFactory& request_context_factory, std::string request_body,
      google::api::HttpBody* response, int16_t* http_response_code,
      GetValuesV2Handler* handler) {
    PlainRequest plain_request(std::move(request_body));
    ExecutionMetadata execution_metadata;
    auto contentTypeHeader = std::string(kKVContentTypeHeader);
    auto contentEncodingProtoHeaderValue =
        std::string(kContentEncodingProtoHeaderValue);
    auto contentEncodingJsonHeaderValue =
        std::string(kContentEncodingJsonHeaderValue);
    std::multimap<grpc::string_ref, grpc::string_ref> headers;
    if (IsUsing<ProtocolType::kPlain>()) {
      *http_response_code = 200;
      headers.insert({
          contentTypeHeader,
          contentEncodingJsonHeaderValue,
      });
      return handler->GetValuesHttp(request_context_factory, headers,
                                    plain_request.Build(), response,
                                    execution_metadata);
    }
    auto encoded_data_size =
        GetEncodedDataSize(plain_request.RequestBody().size());
    auto maybe_padded_request =
        privacy_sandbox::server_common::EncodeResponsePayload(
            privacy_sandbox::server_common::CompressionType::kUncompressed,
            std::move(plain_request.RequestBody()), encoded_data_size);
    if (!maybe_padded_request.ok()) {
      LOG(ERROR) << "Padding failed: "
                 << maybe_padded_request.status().message();
      return privacy_sandbox::server_common::FromAbslStatus(
          maybe_padded_request.status());
    }

    OHTTPRequest ohttp_request(*maybe_padded_request);
    // get ObliviousGetValuesRequest, OHTTPResponseUnwrapper
    auto [request, response_unwrapper] = ohttp_request.Build();
    if (IsProtobufContent()) {
      headers.insert({
          contentTypeHeader,
          contentEncodingProtoHeaderValue,
      });
    }
    if (const auto s = handler->ObliviousGetValues(
            request_context_factory, headers, request,
            &response_unwrapper.RawResponse(), execution_metadata);
        !s.ok()) {
      *http_response_code = s.error_code();
      LOG(ERROR) << "ObliviousGetValues failed: " << s.error_message();
      return s;
    }

    response->set_data(response_unwrapper.Unwrap());
    *http_response_code = 200;
    return grpc::Status::OK;
  }

  MockUdfClient mock_udf_client_;
  privacy_sandbox::server_common::FakeKeyFetcherManager
      fake_key_fetcher_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    GetValuesHandlerTest, GetValuesHandlerTest,
    testing::Values(
        TestingParameters{
            .protocol_type = ProtocolType::kPlain,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body = kv_server::kExampleV2RequestInJson,
            .is_consented = false,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kPlain,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body = kv_server::kExampleConsentedV2RequestInJson,
            .is_consented = true,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kPlain,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body =
                kv_server::kExampleConsentedV2RequestWithLogContextInJson,
            .is_consented = true,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body = kv_server::kExampleV2RequestInJson,
            .is_consented = false,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body = kv_server::kExampleConsentedV2RequestInJson,
            .is_consented = true,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body =
                kv_server::kExampleConsentedV2RequestWithLogContextInJson,
            .is_consented = true,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingProtoHeaderValue,
            .core_request_body = kv_server::kExampleV2RequestInJson,
            .is_consented = false,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingProtoHeaderValue,
            .core_request_body = kv_server::kExampleConsentedV2RequestInJson,
            .is_consented = true,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingProtoHeaderValue,
            .core_request_body =
                kv_server::kExampleConsentedV2RequestWithLogContextInJson,
            .is_consented = true,
        }));

TEST_P(GetValuesHandlerTest, Success) {
  UDFExecutionMetadata udf_metadata;
  TextFormat::ParseFromString(R"(
request_metadata {
  fields {
    key: "hostname"
    value {
      string_value: "example.com"
    }
  }
}
  )",
                              &udf_metadata);
  UDFArgument arg1, arg2;
  TextFormat::ParseFromString(R"(
tags {
  values {
    string_value: "structured"
  }
  values {
    string_value: "groupNames"
  }
}
data {
  list_value {
    values {
      string_value: "hello"
    }
  }
})",
                              &arg1);
  TextFormat::ParseFromString(R"(
tags {
  values {
    string_value: "custom"
  }
  values {
    string_value: "keys"
  }
}
data {
  list_value {
    values {
      string_value: "key1"
    }
  }
})",
                              &arg2);
  nlohmann::json output = nlohmann::json::parse(R"(
{
  "keyGroupOutputs": [
      {
          "keyValues": {
              "key1": "value1"
          },
          "tags": [
              "custom",
              "keys"
          ]
      },
      {
          "keyValues": {
              "hello": "world"
          },
          "tags": [
              "structured",
              "groupNames"
          ]
      }
  ]
}
  )");
  EXPECT_CALL(
      mock_udf_client_,
      ExecuteCode(_, EqualsProto(udf_metadata),
                  testing::ElementsAre(EqualsProto(arg1), EqualsProto(arg2)),
                  _))
      .WillOnce(Return(output.dump()));

  std::string core_request_body = GetTestRequestBody();
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, fake_key_fetcher_manager_);
  int16_t http_response_code = 0;
  if (IsProtobufContent()) {
    v2::GetValuesRequest request_proto;
    ASSERT_TRUE(google::protobuf::util::JsonStringToMessage(core_request_body,
                                                            &request_proto)
                    .ok());
    ASSERT_TRUE(request_proto.SerializeToString(&core_request_body));
    EXPECT_EQ(request_proto.consented_debug_config().is_consented(),
              IsRequestExpectConsented());
  }
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  const auto result =
      GetValuesBasedOnProtocol(*request_context_factory, core_request_body,
                               &response, &http_response_code, &handler);
  ASSERT_EQ(http_response_code, 200);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();

  v2::GetValuesResponse actual_response, expected_response;
  expected_response.mutable_single_partition()->set_string_output(
      output.dump());
  if (IsProtobufContent()) {
    ASSERT_TRUE(actual_response.ParseFromString(response.data()));
  } else {
    ASSERT_TRUE(google::protobuf::util::JsonStringToMessage(response.data(),
                                                            &actual_response)
                    .ok());
  }
  EXPECT_THAT(actual_response, EqualsProto(expected_response));
}

TEST_P(GetValuesHandlerTest, NoPartition) {
  std::string core_request_body = R"(
{
    "metadata": {
        "hostname": "example.com"
    }
})";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, fake_key_fetcher_manager_);
  int16_t http_response_code = 0;

  if (IsProtobufContent()) {
    v2::GetValuesRequest request_proto;
    ASSERT_TRUE(google::protobuf::util::JsonStringToMessage(core_request_body,
                                                            &request_proto)
                    .ok());
    ASSERT_TRUE(request_proto.SerializeToString(&core_request_body));
  }
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  const auto result =
      GetValuesBasedOnProtocol(*request_context_factory, core_request_body,
                               &response, &http_response_code, &handler);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), grpc::StatusCode::INTERNAL);
}

TEST_P(GetValuesHandlerTest, UdfFailureForOnePartition) {
  EXPECT_CALL(mock_udf_client_, ExecuteCode(_, _, testing::IsEmpty(), _))
      .WillOnce(Return(absl::InternalError("UDF execution error")));

  std::string core_request_body = R"(
{
    "partitions": [
        {
            "id": 0,
        }
    ]
}
  )";

  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, fake_key_fetcher_manager_);
  int16_t http_response_code = 0;

  if (IsProtobufContent()) {
    v2::GetValuesRequest request_proto;
    ASSERT_TRUE(google::protobuf::util::JsonStringToMessage(core_request_body,
                                                            &request_proto)
                    .ok());
    ASSERT_TRUE(request_proto.SerializeToString(&core_request_body));
  }
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  const auto result =
      GetValuesBasedOnProtocol(*request_context_factory, core_request_body,
                               &response, &http_response_code, &handler);
  ASSERT_EQ(http_response_code, 200);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();

  v2::GetValuesResponse actual_response, expected_response;
  auto* resp_status =
      expected_response.mutable_single_partition()->mutable_status();
  resp_status->set_code(13);
  resp_status->set_message("UDF execution error");

  if (IsProtobufContent()) {
    ASSERT_TRUE(actual_response.ParseFromString(response.data()));
  } else {
    ASSERT_TRUE(google::protobuf::util::JsonStringToMessage(response.data(),
                                                            &actual_response)
                    .ok());
  }
  EXPECT_THAT(actual_response, EqualsProto(expected_response));
}

TEST_F(GetValuesHandlerTest, PureGRPCTest) {
  v2::GetValuesRequest req;
  ExecutionMetadata execution_metadata;
  TextFormat::ParseFromString(
      R"pb(partitions {
             id: 9
             arguments { data { string_value: "ECHO" } }
           })pb",
      &req);
  GetValuesV2Handler handler(mock_udf_client_, fake_key_fetcher_manager_);
  EXPECT_CALL(
      mock_udf_client_,
      ExecuteCode(
          _, _,
          testing::ElementsAre(EqualsProto(req.partitions(0).arguments(0))), _))
      .WillOnce(Return("ECHO"));
  v2::GetValuesResponse resp;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  const auto result = handler.GetValues(*request_context_factory, req, &resp,
                                        execution_metadata);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();

  v2::GetValuesResponse res;
  TextFormat::ParseFromString(
      R"pb(single_partition { id: 9 string_output: "ECHO" })pb", &res);
  EXPECT_THAT(resp, EqualsProto(res));
}

TEST_F(GetValuesHandlerTest, PureGRPCTestFailure) {
  v2::GetValuesRequest req;
  ExecutionMetadata execution_metadata;
  TextFormat::ParseFromString(
      R"pb(partitions {
             id: 9
             arguments { data { string_value: "ECHO" } }
           })pb",
      &req);
  GetValuesV2Handler handler(mock_udf_client_, fake_key_fetcher_manager_);
  EXPECT_CALL(
      mock_udf_client_,
      ExecuteCode(
          _, _,
          testing::ElementsAre(EqualsProto(req.partitions(0).arguments(0))), _))
      .WillOnce(Return(absl::InternalError("UDF execution error")));
  v2::GetValuesResponse resp;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  const auto result = handler.GetValues(*request_context_factory, req, &resp,
                                        execution_metadata);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();

  v2::GetValuesResponse res;
  TextFormat::ParseFromString(
      R"pb(single_partition {
             id: 9
             status: { code: 13 message: "UDF execution error" }
           })pb",
      &res);
  EXPECT_THAT(resp, EqualsProto(res));
}

}  // namespace
}  // namespace kv_server
