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
#include "components/data/converters/cbor_converter.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/mocks.h"
#include "components/data_server/request_handler/content_type/json_encoder.h"
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
#include "src/communication/framing_utils.h"
#include "src/encryption/key_fetcher/fake_key_fetcher_manager.h"

#include "cbor.h"

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

// TODO(b/355434272): Refactor
struct TestingParameters {
  ProtocolType protocol_type;
  const std::string_view content_type;
  const std::string_view core_request_body;
};

class BaseTest : public ::testing::Test,
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

  bool IsJsonContent() {
    auto param = GetParam();
    return param.content_type == kContentEncodingJsonHeaderValue;
  }

  bool IsCborContent() {
    auto param = GetParam();
    return param.content_type == kContentEncodingCborHeaderValue;
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
      auto decrypted_response =
          quiche::ObliviousHttpResponse::CreateClientObliviousResponse(
              response_.data(), context_, kKVOhttpResponseLabel);
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
      auto encrypted_req =
          quiche::ObliviousHttpRequest::CreateClientObliviousRequest(
              raw_request_, public_key_, *std::move(maybe_config),
              kKVOhttpRequestLabel);
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

  std::string GetContentTypeHeaderValue() {
    auto contentTypeHeader = std::string(kKVContentTypeHeader);
    auto contentEncodingProtoHeaderValue =
        std::string(kContentEncodingProtoHeaderValue);
    auto contentEncodingJsonHeaderValue =
        std::string(kContentEncodingJsonHeaderValue);
    auto contentEncodingCborHeaderValue =
        std::string(kContentEncodingCborHeaderValue);
    if (IsProtobufContent()) {
      return contentEncodingProtoHeaderValue;
    }
    if (IsJsonContent()) {
      return contentEncodingJsonHeaderValue;
    }
    // CBOR
    return contentEncodingCborHeaderValue;
  }

  void GetRequestWithContentType(std::string& request_body) {
    if (IsJsonContent()) {
      return;
    }

    if (IsProtobufContent()) {
      v2::GetValuesRequest request_proto;
      EXPECT_TRUE(google::protobuf::util::JsonStringToMessage(request_body,
                                                              &request_proto)
                      .ok());
      EXPECT_TRUE(request_proto.SerializeToString(&request_body));
      return;
    }

    // CBOR content type
    auto request_body_json = nlohmann::json::parse(request_body, nullptr,
                                                   /*allow_exceptions=*/false,
                                                   /*ignore_comments=*/true);
    EXPECT_FALSE(request_body_json.is_discarded());
    std::vector<uint8_t> cbor_vector =
        nlohmann::json::to_cbor(request_body_json);
    request_body = std::string(cbor_vector.begin(), cbor_vector.end());
  }

  // For Non-plain protocols, test request and response data are converted
  // to/from the corresponding request/responses.
  grpc::Status GetValuesBasedOnProtocol(
      RequestContextFactory& request_context_factory, std::string request_body,
      google::api::HttpBody* response, int16_t* http_response_code,
      GetValuesV2Handler* handler) {
    GetRequestWithContentType(request_body);
    PlainRequest plain_request(std::move(request_body));

    ExecutionMetadata execution_metadata;
    std::multimap<grpc::string_ref, grpc::string_ref> headers;
    auto contentTypeHeader = std::string(kKVContentTypeHeader);
    std::string contentTypeHeaderValue = GetContentTypeHeaderValue();
    headers.insert({
        contentTypeHeader,
        contentTypeHeaderValue,
    });
    *http_response_code = 200;

    if (IsUsing<ProtocolType::kPlain>()) {
      const auto s = handler->GetValuesHttp(request_context_factory, headers,
                                            plain_request.Build(), response,
                                            execution_metadata);
      if (!s.ok()) {
        *http_response_code = s.error_code();
        LOG(ERROR) << "GetValuesHttp failed: " << s.error_message();
      }
      return s;
    }

    // OHTTP logic
    auto encoded_data_size = privacy_sandbox::server_common::GetEncodedDataSize(
        plain_request.RequestBody().size(), kMinResponsePaddingBytes);
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
    if (const auto s = handler->ObliviousGetValues(
            request_context_factory, headers, request,
            &response_unwrapper.RawResponse(), execution_metadata);
        !s.ok()) {
      *http_response_code = s.error_code();
      LOG(ERROR) << "ObliviousGetValues failed: " << s.error_message();
      return s;
    }
    response->set_data(response_unwrapper.Unwrap());
    return grpc::Status::OK;
  }

  // Parses the http response data into a V2 response proto.
  // The returned compression_group content is in JSON format.
  void ParseResponseByContentType(google::api::HttpBody response,
                                  v2::GetValuesResponse& response_proto,
                                  bool multi_partition) {
    if (IsProtobufContent()) {
      ASSERT_TRUE(response_proto.ParseFromString(response.data()));
    } else if (IsJsonContent()) {
      ASSERT_TRUE(google::protobuf::util::JsonStringToMessage(response.data(),
                                                              &response_proto)
                      .ok());
    } else {  // CBOR
      if (multi_partition) {
        nlohmann::json actual_response_from_cbor = nlohmann::json::from_cbor(
            response.data(), /*strict=*/true, /*allow_exceptions=*/false);
        ASSERT_FALSE(actual_response_from_cbor.is_discarded());
        std::vector<std::string> contents;
        for (auto& compressionGroup :
             actual_response_from_cbor["compressionGroups"]) {
          auto a = GetPartitionOutputsInJson(compressionGroup["content"]);
          ASSERT_TRUE(a.ok());
          auto* compression_group = response_proto.add_compression_groups();
          compression_group->set_compression_group_id(
              compressionGroup["compressionGroupId"]);
          compression_group->set_content(a->dump());
        }
      } else {
        ASSERT_TRUE(
            CborDecodeToNonBytesProto(response.data(), response_proto).ok());
      }
    }
  }

  MockUdfClient mock_udf_client_;
  privacy_sandbox::server_common::FakeKeyFetcherManager
      fake_key_fetcher_manager_;
};

class GetValuesHandlerTest : public BaseTest {};
class GetValuesHandlerMultiplePartitionsTest : public BaseTest {};

INSTANTIATE_TEST_SUITE_P(
    GetValuesHandlerTest, GetValuesHandlerTest,
    testing::Values(
        TestingParameters{
            .protocol_type = ProtocolType::kPlain,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body = kv_server::kExampleV2RequestInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kPlain,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body = kv_server::kExampleConsentedV2RequestInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kPlain,
            .content_type = kContentEncodingProtoHeaderValue,
            .core_request_body = kv_server::kExampleV2RequestInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kPlain,
            .content_type = kContentEncodingProtoHeaderValue,
            .core_request_body = kv_server::kExampleConsentedV2RequestInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body = kv_server::kExampleV2RequestInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body = kv_server::kExampleConsentedV2RequestInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingProtoHeaderValue,
            .core_request_body = kv_server::kExampleV2RequestInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingProtoHeaderValue,
            .core_request_body = kv_server::kExampleConsentedV2RequestInJson,
        }));

INSTANTIATE_TEST_SUITE_P(
    GetValuesHandlerMultiplePartitionsTest,
    GetValuesHandlerMultiplePartitionsTest,
    testing::Values(
        TestingParameters{
            .protocol_type = ProtocolType::kPlain,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body = kv_server::kV2RequestMultiplePartitionsInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kPlain,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body =
                kv_server::kConsentedV2RequestMultiplePartitionsInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kPlain,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body = kv_server::
                kConsentedV2RequestMultiplePartitionsWithLogContextInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body = kv_server::kV2RequestMultiplePartitionsInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body =
                kv_server::kConsentedV2RequestMultiplePartitionsInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingJsonHeaderValue,
            .core_request_body = kv_server::
                kConsentedV2RequestMultiplePartitionsWithLogContextInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingCborHeaderValue,
            .core_request_body = kv_server::kV2RequestMultiplePartitionsInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingCborHeaderValue,
            .core_request_body =
                kv_server::kConsentedV2RequestMultiplePartitionsInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingCborHeaderValue,
            .core_request_body = kv_server::
                kConsentedV2RequestMultiplePartitionsWithLogContextInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingCborHeaderValue,
            .core_request_body = kv_server::
                kConsentedV2RequestMultiPartWithDebugInfoResponseInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingProtoHeaderValue,
            .core_request_body = kv_server::kV2RequestMultiplePartitionsInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingProtoHeaderValue,
            .core_request_body =
                kv_server::kConsentedV2RequestMultiplePartitionsInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingProtoHeaderValue,
            .core_request_body = kv_server::
                kConsentedV2RequestMultiplePartitionsWithLogContextInJson,
        },
        TestingParameters{
            .protocol_type = ProtocolType::kObliviousHttp,
            .content_type = kContentEncodingProtoHeaderValue,
            .core_request_body = kv_server::
                kConsentedV2RequestMultiPartWithDebugInfoResponseInJson,
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
  fields {
    key: "is_pas"
    value {
      string_value: "true"
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
  ParseResponseByContentType(response, actual_response,
                             /*multi_partition=*/false);
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

  auto request_context_factory = std::make_unique<RequestContextFactory>();
  const auto result =
      GetValuesBasedOnProtocol(*request_context_factory, core_request_body,
                               &response, &http_response_code, &handler);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), grpc::StatusCode::INTERNAL);
}

TEST_P(GetValuesHandlerTest, DuplicatePartitionIdsFails) {
  std::string core_request_body = R"(
{
    "metadata": {
      "hostname": "example.com"
    },
    "partitions": [
      {
        "id": 1,
        "compressionGroupId": 1,
        "arguments": [
          {
            "tags": [
              "custom",
              "keys"
            ],
            "data": [
              "key1"
            ]
          }
        ]

      },
      {
        "id": 1,
        "compressionGroupId": 1,
        "arguments": [
          {
            "tags": [
              "structured",
              "groupNames"
            ],
            "data": [
              "hello"
            ]
          }
        ]
      }
    ]
})";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, fake_key_fetcher_manager_);
  int16_t http_response_code = 0;

  auto request_context_factory = std::make_unique<RequestContextFactory>();
  const auto result =
      GetValuesBasedOnProtocol(*request_context_factory, core_request_body,
                               &response, &http_response_code, &handler);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
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
    ],
    "metadata": {
      "is_pas": "true"
    }
}
  )";

  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, fake_key_fetcher_manager_);
  int16_t http_response_code = 0;
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

  ParseResponseByContentType(response, actual_response,
                             /*multi_partition=*/false);
  EXPECT_THAT(actual_response, EqualsProto(expected_response));
}

TEST_P(GetValuesHandlerMultiplePartitionsTest, Success) {
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
  UDFArgument arg1, arg2, arg3;
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
      string_value: "key2"
    }
  }
})",
                              &arg3);
  nlohmann::json output1 = nlohmann::json::parse(R"(
{
  "keyGroupOutputs": [
      {
          "keyValues": {
              "hello": {
                "value": "world"
              }
          },
          "tags": [
              "structured",
              "groupNames"
          ]
      }
  ]
}
  )");
  nlohmann::json output2 = nlohmann::json::parse(R"(
{
  "keyGroupOutputs": [
      {
          "keyValues": {
              "key1": {
                "value": "value1"
              }
          },
          "tags": [
              "custom",
              "keys"
          ]
      }
  ]
}
  )");
  nlohmann::json output3 = nlohmann::json::parse(R"(
{
  "keyGroupOutputs": [
      {
           "keyValues": {
              "key2": {
                "value": "value2"
              }
          },
          "tags": [
              "custom",
              "keys"
          ]
      }
  ]
}
  )");
  absl::flat_hash_map<int32_t, std::string> batch_execute_output = {
      {0, output1.dump()}, {1, output2.dump()}, {2, output3.dump()}};
  EXPECT_CALL(
      mock_udf_client_,
      BatchExecuteCode(
          _,
          testing::UnorderedElementsAre(
              testing::Pair(0, testing::FieldsAre(
                                   EqualsProto(udf_metadata),
                                   testing::ElementsAre(EqualsProto(arg1)))),
              testing::Pair(1, testing::FieldsAre(
                                   EqualsProto(udf_metadata),
                                   testing::ElementsAre(EqualsProto(arg2)))),
              testing::Pair(2, testing::FieldsAre(
                                   EqualsProto(udf_metadata),
                                   testing::ElementsAre(EqualsProto(arg3))))),
          _))
      .WillOnce(Return(batch_execute_output));

  std::string core_request_body = GetTestRequestBody();
  google::api::HttpBody http_response;
  GetValuesV2Handler handler(mock_udf_client_, fake_key_fetcher_manager_);
  int16_t http_response_code = 0;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  const auto result =
      GetValuesBasedOnProtocol(*request_context_factory, core_request_body,
                               &http_response, &http_response_code, &handler);
  ASSERT_EQ(http_response_code, 200);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();

  nlohmann::json partition_output1 = {{"id", 0}};
  partition_output1.update(output1);
  nlohmann::json partition_output2 = {{"id", 1}};
  partition_output2.update(output2);
  nlohmann::json partition_output3 = {{"id", 2}};
  partition_output3.update(output3);
  nlohmann::json compressed_partition_group0 = {partition_output1,
                                                partition_output3};
  nlohmann::json compressed_partition_group1 =
      nlohmann::json::array({partition_output2});
  v2::GetValuesResponse actual_response_proto;
  // Parse the response to proto for easier comparison
  ParseResponseByContentType(http_response, actual_response_proto,
                             /*multi_partition=*/true);
  EXPECT_EQ(actual_response_proto.compression_groups().size(), 2);
  std::vector<std::string> contents;
  for (auto&& compression_group : actual_response_proto.compression_groups()) {
    contents.emplace_back(compression_group.content());
  }
  EXPECT_THAT(contents, testing::UnorderedElementsAre(
                            compressed_partition_group0.dump(),
                            compressed_partition_group1.dump()));
}

TEST_P(GetValuesHandlerMultiplePartitionsTest,
       SinglePartitionUDFFails_IgnorePartition) {
  nlohmann::json output1 = nlohmann::json::parse(R"(
{
  "keyGroupOutputs": [
      {
          "keyValues": {
              "hello": {
                "value": "world"
              }
          },
          "tags": [
              "structured",
              "groupNames"
          ]
      }
  ]
}
  )");
  nlohmann::json output2 = nlohmann::json::parse(R"(
{
  "keyGroupOutputs": [
      {
          "keyValues": {
              "key1": {
                "value": "value1"
              }
          },
          "tags": [
              "custom",
              "keys"
          ]
      }
  ]
}
  )");
  absl::flat_hash_map<int32_t, std::string> batch_execute_output = {
      {0, output1.dump()}, {1, output2.dump()}};
  EXPECT_CALL(mock_udf_client_, BatchExecuteCode(_, _, _))
      .WillOnce(Return(batch_execute_output));

  std::string core_request_body = GetTestRequestBody();
  google::api::HttpBody http_response;
  GetValuesV2Handler handler(mock_udf_client_, fake_key_fetcher_manager_);
  int16_t http_response_code = 0;
  auto request_context_factory = std::make_unique<RequestContextFactory>();

  const auto result =
      GetValuesBasedOnProtocol(*request_context_factory, core_request_body,
                               &http_response, &http_response_code, &handler);
  ASSERT_EQ(http_response_code, 200);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();

  nlohmann::json partition_output1 = {{"id", 0}};
  partition_output1.update(output1);
  nlohmann::json partition_output2 = {{"id", 1}};
  partition_output2.update(output2);
  nlohmann::json compressed_partition_group0 =
      nlohmann::json::array({partition_output1});
  nlohmann::json compressed_partition_group1 =
      nlohmann::json::array({partition_output2});
  v2::GetValuesResponse actual_response_proto;
  ParseResponseByContentType(http_response, actual_response_proto,
                             /*multi_partition=*/true);
  EXPECT_EQ(actual_response_proto.compression_groups().size(), 2);
  std::vector<std::string> contents;
  for (auto&& compression_group : actual_response_proto.compression_groups()) {
    contents.emplace_back(compression_group.content());
  }
  EXPECT_THAT(contents, testing::UnorderedElementsAre(
                            compressed_partition_group0.dump(),
                            compressed_partition_group1.dump()));
}

TEST_P(GetValuesHandlerMultiplePartitionsTest,
       AllPartitionsInSingleCompressionGroupUDFFails_IgnoreCompressionGroup) {
  nlohmann::json output = nlohmann::json::parse(R"(
{
  "keyGroupOutputs": [
      {
          "keyValues": {
              "key1": {
                "value": "value1"
              }
          },
          "tags": [
              "custom",
              "keys"
          ]
      }
  ]
}
  )");
  absl::flat_hash_map<int32_t, std::string> batch_execute_output = {
      {1, output.dump()}};
  EXPECT_CALL(mock_udf_client_, BatchExecuteCode(_, _, _))
      .WillOnce(Return(batch_execute_output));

  std::string core_request_body = GetTestRequestBody();
  google::api::HttpBody http_response;
  GetValuesV2Handler handler(mock_udf_client_, fake_key_fetcher_manager_);
  int16_t http_response_code = 0;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  const auto result =
      GetValuesBasedOnProtocol(*request_context_factory, core_request_body,
                               &http_response, &http_response_code, &handler);
  ASSERT_EQ(http_response_code, 200);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();

  nlohmann::json expected_partition_output = {{"id", 1}};
  expected_partition_output.update(output);
  nlohmann::json compressed_partition_group =
      nlohmann::json::array({expected_partition_output});
  v2::GetValuesResponse actual_response, expected_response;
  ParseResponseByContentType(http_response, actual_response,
                             /*multi_partition=*/true);

  auto* compression_group = expected_response.add_compression_groups();
  compression_group->set_content(compressed_partition_group.dump());
  compression_group->set_compression_group_id(1);
  EXPECT_THAT(actual_response, EqualsProto(expected_response));
}

TEST_P(GetValuesHandlerMultiplePartitionsTest,
       AllPartitionsFail_ReturnSuccess) {
  EXPECT_CALL(mock_udf_client_, BatchExecuteCode(_, _, _))
      .WillOnce(Return(absl::InternalError("Batch UDF execution error")));

  std::string core_request_body = GetTestRequestBody();
  google::api::HttpBody http_response;
  GetValuesV2Handler handler(mock_udf_client_, fake_key_fetcher_manager_);
  int16_t http_response_code = 0;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  const auto result =
      GetValuesBasedOnProtocol(*request_context_factory, core_request_body,
                               &http_response, &http_response_code, &handler);
  ASSERT_EQ(http_response_code, 200);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();
  v2::GetValuesResponse actual_response, expected_response;
  ParseResponseByContentType(http_response, actual_response,
                             /*multi_partition=*/true);
  EXPECT_THAT(actual_response, EqualsProto(expected_response));
}

TEST_F(GetValuesHandlerTest, PureGRPCTest_Success) {
  v2::GetValuesRequest req;
  ExecutionMetadata execution_metadata;
  TextFormat::ParseFromString(
      R"pb(partitions {
             id: 9
             arguments { data { string_value: "ECHO" } }
           }
           metadata {
             fields {
               key: "is_pas"
               value { string_value: "true" }
             }
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
  JsonV2EncoderDecoder v2_codec;
  const auto result = handler.GetValues(
      *request_context_factory, req, &resp, execution_metadata,
      /*single_partition_use_case=*/true, v2_codec);
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
           }
           metadata {
             fields {
               key: "is_pas"
               value { string_value: "true" }
             }
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
  JsonV2EncoderDecoder v2_codec;
  const auto result = handler.GetValues(
      *request_context_factory, req, &resp, execution_metadata,
      /*single_partition_use_case=*/true, v2_codec);
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

TEST_F(GetValuesHandlerTest,
       PureGRPCTest_SinglePartitionUseCase_PassesPartitionMetadata) {
  v2::GetValuesRequest req;
  ExecutionMetadata execution_metadata;
  TextFormat::ParseFromString(
      R"pb(partitions {
             id: 9
             arguments { data { string_value: "ECHO" } }
             metadata {
               fields {
                 key: "partition_metadata_key"
                 value: { string_value: "my_value" }
               }
             }
           }
           metadata {
             fields {
               key: "is_pas"
               value { string_value: "true" }
             }
           })pb",
      &req);
  UDFExecutionMetadata udf_metadata;
  TextFormat::ParseFromString(R"(
  request_metadata {
    fields {
      key: "is_pas"
      value {
        string_value: "true"
      }
    }
  }
  partition_metadata {
    fields {
      key: "partition_metadata_key"
      value {
        string_value: "my_value"
      }
    }
  }
  )",
                              &udf_metadata);

  GetValuesV2Handler handler(mock_udf_client_, fake_key_fetcher_manager_);
  EXPECT_CALL(
      mock_udf_client_,
      ExecuteCode(
          _, EqualsProto(udf_metadata),
          testing::ElementsAre(EqualsProto(req.partitions(0).arguments(0))), _))
      .WillOnce(Return("ECHO"));
  v2::GetValuesResponse resp;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  JsonV2EncoderDecoder v2_codec;
  const auto result = handler.GetValues(
      *request_context_factory, req, &resp, execution_metadata,
      /*single_partition_use_case=*/true, v2_codec);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();

  v2::GetValuesResponse res;
  TextFormat::ParseFromString(
      R"pb(single_partition { id: 9 string_output: "ECHO" })pb", &res);
  EXPECT_THAT(resp, EqualsProto(res));
}

TEST_F(GetValuesHandlerTest,
       PureGRPCTestPassesConsentedDebugConfigAndLogContext) {
  v2::GetValuesRequest req;
  ExecutionMetadata execution_metadata;
  TextFormat::ParseFromString(
      R"pb(partitions {
             id: 9
             arguments { data { string_value: "ECHO" } }
             metadata {
               fields {
                 key: "partition_metadata_key"
                 value: { string_value: "my_value" }
               }
             }
           }
           consented_debug_config { is_consented: true token: "debug_token" }
      )pb",
      &req);
  EXPECT_CALL(
      mock_udf_client_,
      ExecuteCode(
          _, _,
          testing::ElementsAre(EqualsProto(req.partitions(0).arguments(0))), _))
      .WillOnce(Return("ECHO"));

  GetValuesV2Handler handler(mock_udf_client_, fake_key_fetcher_manager_);
  v2::GetValuesResponse resp;
  auto request_context_factory = std::make_unique<RequestContextFactory>();
  JsonV2EncoderDecoder v2_codec;
  const auto result = handler.GetValues(
      *request_context_factory, req, &resp, execution_metadata,
      /*single_partition_use_case=*/true, v2_codec);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();
  EXPECT_THAT(req.consented_debug_config(),
              EqualsProto(request_context_factory->Get()
                              .GetRequestLogContext()
                              .GetConsentedDebugConfiguration()));
  EXPECT_THAT(req.log_context(), EqualsProto(request_context_factory->Get()
                                                 .GetRequestLogContext()
                                                 .GetLogContext()));
}

TEST_F(GetValuesHandlerTest, PureGRPCTest_CBOR_Success) {
  v2::GetValuesRequest req;
  ExecutionMetadata execution_metadata;
  TextFormat::ParseFromString(
      R"pb(partitions {
             id: 9
             arguments { data { string_value: "ECHO" } }
           }
           metadata {
             fields {
               key: "is_pas"
               value { string_value: "true" }
             }
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
  JsonV2EncoderDecoder v2_codec;
  const auto result = handler.GetValues(
      *request_context_factory, req, &resp, execution_metadata,
      /*single_partition_use_case=*/true, v2_codec);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();

  v2::GetValuesResponse res;
  TextFormat::ParseFromString(
      R"pb(single_partition { id: 9 string_output: "ECHO" })pb", &res);
  EXPECT_THAT(resp, EqualsProto(res));
}

TEST_F(GetValuesHandlerTest, IsSinglePartitionUseCaseIsPasReturnsTrue) {
  v2::GetValuesRequest req;
  TextFormat::ParseFromString(
      R"pb(
        metadata {
          fields {
            key: "is_pas"
            value { string_value: "true" }
          }
        })pb",
      &req);
  EXPECT_TRUE(IsSinglePartitionUseCase(req));
}

TEST_F(GetValuesHandlerTest, IsSinglePartitonUseCaseNotIsPasReturnsFalse) {
  v2::GetValuesRequest req;
  TextFormat::ParseFromString(
      R"pb(
        metadata {
          fields {
            key: "some"
            value { string_value: "other data" }
          }
        })pb",
      &req);
  EXPECT_FALSE(IsSinglePartitionUseCase(req));
}

}  // namespace
}  // namespace kv_server
