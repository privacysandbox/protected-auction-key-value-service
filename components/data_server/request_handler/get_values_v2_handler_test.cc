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

#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/mocks.h"
#include "components/udf/mocks.h"
#include "glog/logging.h"
#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "public/constants.h"
#include "public/test_util/proto_matcher.h"
#include "quiche/binary_http/binary_http_message.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "quiche/oblivious_http/oblivious_http_client.h"
#include "src/cpp/encryption/key_fetcher/src/fake_key_fetcher_manager.h"
#include "src/cpp/telemetry/metrics_recorder.h"
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {
namespace {

using grpc::StatusCode;
using privacy_sandbox::server_common::MockMetricsRecorder;
using testing::_;
using testing::Return;
using testing::ReturnRef;
using testing::UnorderedElementsAre;
using v2::BinaryHttpGetValuesRequest;
using v2::GetValuesHttpRequest;
using v2::ObliviousGetValuesRequest;

enum class ProtocolType {
  kPlain = 0,
  kBinaryHttp,
  kObliviousHttp,
};

class GetValuesHandlerTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<ProtocolType> {
 protected:
  template <ProtocolType protocol_type>
  bool IsUsing() {
    return GetParam() == protocol_type;
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

  class BHTTPRequest {
   public:
    explicit BHTTPRequest(PlainRequest plain_request) {
      quiche::BinaryHttpRequest req_bhttp_layer({});
      req_bhttp_layer.set_body(plain_request.RequestBody());
      auto maybe_serialized = req_bhttp_layer.Serialize();
      EXPECT_TRUE(maybe_serialized.ok());
      serialized_bhttp_request_ = *maybe_serialized;
    }

    BinaryHttpGetValuesRequest Build() const {
      BinaryHttpGetValuesRequest brequest;
      brequest.mutable_raw_body()->set_data(serialized_bhttp_request_);
      return brequest;
    }

    const std::string& SerializedBHTTPRequest() const {
      return serialized_bhttp_request_;
    }

   private:
    std::string serialized_bhttp_request_;
  };

  class CompressedResponse {
   public:
    explicit CompressedResponse(std::string compressed_blob)
        : compressed_blob_(std::move(compressed_blob)) {}

    void Unwrap(std::string& uncompressed_data) const {
      auto compress_reader = CompressedBlobReader::Create(
          CompressionGroupConcatenator::CompressionType::kUncompressed,
          compressed_blob_);

      std::vector<nlohmann::json> compression_groups;
      while (!compress_reader->IsDoneReading()) {
        auto maybe_one_group = compress_reader->ExtractOneCompressionGroup();
        CHECK(maybe_one_group.ok()) << maybe_one_group.status();
        LOG(INFO) << "one group: " << *maybe_one_group;
        nlohmann::json group_json =
            nlohmann::json::parse(*maybe_one_group, nullptr,
                                  /*allow_exceptions=*/false,
                                  /*ignore_comments=*/true);
        ASSERT_FALSE(group_json.is_discarded())
            << "Failed to parse the compression group json";

        compression_groups.push_back(std::move(group_json));
      }
      nlohmann::json response_json =
          GetValuesV2Handler::BuildCompressionGroupsForDebugging(
              std::move(compression_groups));
      VLOG(5) << "Uncompressed response: " << response_json.dump(1);
      uncompressed_data = response_json.dump();
    }

   private:
    std::string compressed_blob_;
  };

  class BHTTPResponse {
   public:
    google::api::HttpBody& RawResponse() { return response_; }
    int16_t ResponseCode() const {
      const absl::StatusOr<quiche::BinaryHttpResponse> maybe_res_bhttp_layer =
          quiche::BinaryHttpResponse::Create(response_.data());
      EXPECT_TRUE(maybe_res_bhttp_layer.ok())
          << "quiche::BinaryHttpResponse::Create failed: "
          << maybe_res_bhttp_layer.status();
      return maybe_res_bhttp_layer->status_code();
    }

    CompressedResponse Unwrap() const {
      const absl::StatusOr<quiche::BinaryHttpResponse> maybe_res_bhttp_layer =
          quiche::BinaryHttpResponse::Create(response_.data());
      EXPECT_TRUE(maybe_res_bhttp_layer.ok())
          << "quiche::BinaryHttpResponse::Create failed: "
          << maybe_res_bhttp_layer.status();
      return CompressedResponse(std::string(maybe_res_bhttp_layer->body()));
    }

   private:
    google::api::HttpBody response_;
  };

  class OHTTPRequest;
  class OHTTPResponseUnwrapper {
   public:
    google::api::HttpBody& RawResponse() { return response_; }

    BHTTPResponse Unwrap() {
      uint8_t key_id = 64;
      auto maybe_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
          key_id, kKEMParameter, kKDFParameter, kAEADParameter);
      EXPECT_TRUE(maybe_config.ok());

      auto client =
          quiche::ObliviousHttpClient::Create(public_key_, *maybe_config);
      EXPECT_TRUE(client.ok());
      auto decrypted_response =
          client->DecryptObliviousHttpResponse(response_.data(), context_);
      BHTTPResponse bhttp_response;
      bhttp_response.RawResponse().set_data(
          decrypted_response->GetPlaintextData());
      return bhttp_response;
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
    explicit OHTTPRequest(BHTTPRequest bhttp_request)
        : bhttp_request_(std::move(bhttp_request)) {}

    std::pair<ObliviousGetValuesRequest, OHTTPResponseUnwrapper> Build() const {
      // matches the test key pair, see common repo:
      // ../encryption/key_fetcher/src/fake_key_fetcher_manager.h
      uint8_t key_id = 64;
      auto maybe_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
          key_id, 0x0020, 0x0001, 0x0001);
      EXPECT_TRUE(maybe_config.ok());

      auto client =
          quiche::ObliviousHttpClient::Create(public_key_, *maybe_config);
      EXPECT_TRUE(client.ok());
      auto encrypted_req = client->CreateObliviousHttpRequest(
          bhttp_request_.SerializedBHTTPRequest());
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
    BHTTPRequest bhttp_request_;
  };

  // For Non-plain protocols, test request and response data are converted
  // to/from the corresponding request/responses.
  grpc::Status GetValuesBasedOnProtocol(std::string request_body,
                                        google::api::HttpBody* response,
                                        int16_t* bhttp_response_code,
                                        GetValuesV2Handler* handler) {
    PlainRequest plain_request(std::move(request_body));

    if (IsUsing<ProtocolType::kPlain>()) {
      *bhttp_response_code = 200;
      return handler->GetValuesHttp(plain_request.Build(), response);
    }

    BHTTPRequest bhttp_request(std::move(plain_request));
    BHTTPResponse bresponse;

    if (IsUsing<ProtocolType::kBinaryHttp>()) {
      if (const auto s = handler->BinaryHttpGetValues(bhttp_request.Build(),
                                                      &bresponse.RawResponse());
          !s.ok()) {
        LOG(ERROR) << "BinaryHttpGetValues failed: " << s.error_message();
        return s;
      }
      *bhttp_response_code = bresponse.ResponseCode();
    } else if (IsUsing<ProtocolType::kObliviousHttp>()) {
      OHTTPRequest ohttp_request(std::move(bhttp_request));
      // get ObliviousGetValuesRequest, OHTTPResponseUnwrapper
      auto [request, response_unwrapper] = ohttp_request.Build();
      if (const auto s = handler->ObliviousGetValues(
              request, &response_unwrapper.RawResponse());
          !s.ok()) {
        LOG(ERROR) << "ObliviousGetValues failed: " << s.error_message();
        return s;
      }
      bresponse = response_unwrapper.Unwrap();
      *bhttp_response_code = bresponse.ResponseCode();
    }

    CompressedResponse compressed = bresponse.Unwrap();
    compressed.Unwrap(*response->mutable_data());

    return grpc::Status::OK;
  }

  MockUdfClient mock_udf_client_;
  MockMetricsRecorder mock_metrics_recorder_;
  privacy_sandbox::server_common::FakeKeyFetcherManager
      fake_key_fetcher_manager_;
};

INSTANTIATE_TEST_SUITE_P(GetValuesHandlerTest, GetValuesHandlerTest,
                         testing::Values(ProtocolType::kPlain,
                                         ProtocolType::kBinaryHttp,
                                         ProtocolType::kObliviousHttp));

TEST_P(GetValuesHandlerTest, Success) {
  nlohmann::json udf_input1 = R"({
    "context": {"subkey": "example.com"},
    "keyGroups": [
      {
        "tags": [
          "structured",
          "groupNames"
        ],
        "keyList": [
          "hello"
        ]
      },
      {
        "tags": [
          "custom",
          "keys"
        ],
        "keyList": [
          "hello"
        ]
      }
    ],
    "udfInputApiVersion": 1
  })"_json;
  nlohmann::json udf_output1 = R"({"keyGroupOutputs": [
          {
            "keyValues": {
              "hello": {
                "value": "world"
              }
            },
            "tags": [
              "custom",
              "keys"
            ]
          },
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
        ],
      "udfOutputApiVersion": 1
      })"_json;
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(std::vector<std::string>({udf_input1.dump()})))
      .WillOnce(Return(udf_output1.dump()));

  nlohmann::json udf_input2 = R"({
      "context": {"subkey": "example.com"},
      "keyGroups": [
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello2"
          ]
        }
      ],
      "udfInputApiVersion": 1
    })"_json;
  nlohmann::json udf_output2 = R"({"keyGroupOutputs": [
        {
            "keyValues": {
              "hello2": {
                "value": "world2"
              }
            },
            "tags": [
              "custom",
              "keys"
            ]
        }
      ],
      "udfOutputApiVersion": 1
      })"_json;
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(std::vector<std::string>({udf_input2.dump()})))
      .WillOnce(Return(udf_output2.dump()));

  const std::string core_request_body = R"(
{
  "context": {
    "subkey": "example.com"
  },
  "partitions": [
    {
      "id": 0,
      "compressionGroup": 0,
      "keyGroups": [
        {
          "tags": [
            "structured",
            "groupNames"
          ],
          "keyList": [
            "hello"
          ]
        },
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello"
          ]
        }
      ]
    },
    {
      "id": 1,
      "compressionGroup": 0,
      "keyGroups": [
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello2"
          ]
        }
      ]
    }
  ]
}
  )";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, mock_metrics_recorder_,
                             fake_key_fetcher_manager_);
  int16_t bhttp_response_code = 0;
  const auto result = GetValuesBasedOnProtocol(core_request_body, &response,
                                               &bhttp_response_code, &handler);
  ASSERT_EQ(bhttp_response_code, 200);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();

  nlohmann::json expected = nlohmann::json::parse(R"(
[
  {
    "partitions": [
      {
        "id": 0,
        "keyGroupOutputs": [
          {
            "keyValues": {
              "hello": {
                "value": "world"
              }
            },
            "tags": [
              "custom",
              "keys"
            ]
          },
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
      },
      {
        "id": 1,
        "keyGroupOutputs": [
          {
            "keyValues": {
              "hello2": {
                "value": "world2"
              }
            },
            "tags": [
              "custom",
              "keys"
            ]
          }
        ]
      }
    ]
  }
])");
  EXPECT_EQ(response.data(), expected.dump());
}

TEST_P(GetValuesHandlerTest, InvalidFormat) {
  const std::string core_request_body = R"(
{
  "context": {
    "subkey": "example.com"
  },
}
  )";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, mock_metrics_recorder_,
                             fake_key_fetcher_manager_);
  int16_t bhttp_response_code = 0;
  const auto result = GetValuesBasedOnProtocol(core_request_body, &response,
                                               &bhttp_response_code, &handler);
  if (IsUsing<ProtocolType::kPlain>()) {
    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error_code(), grpc::StatusCode::INTERNAL);
  } else {
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(bhttp_response_code, 500);
  }
}

TEST_P(GetValuesHandlerTest, NotADictionary) {
  const std::string core_request_body = R"(
[]
  )";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, mock_metrics_recorder_,
                             fake_key_fetcher_manager_);
  int16_t bhttp_response_code = 0;
  const auto result = GetValuesBasedOnProtocol(core_request_body, &response,
                                               &bhttp_response_code, &handler);
  if (IsUsing<ProtocolType::kPlain>()) {
    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error_code(), grpc::StatusCode::INTERNAL);
  } else {
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(bhttp_response_code, 500);
  }
}

TEST_P(GetValuesHandlerTest, NoPartition) {
  const std::string core_request_body = R"(
{
  "context": {
    "subkey": "example.com"
  }
}
  )";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, mock_metrics_recorder_,
                             fake_key_fetcher_manager_);
  int16_t bhttp_response_code = 0;
  const auto result = GetValuesBasedOnProtocol(core_request_body, &response,
                                               &bhttp_response_code, &handler);
  if (IsUsing<ProtocolType::kPlain>()) {
    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error_code(), grpc::StatusCode::INTERNAL);
  } else {
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(bhttp_response_code, 500);
  }
}

TEST_P(GetValuesHandlerTest, NoContext) {
  const std::string core_request_body = R"(
{
  "partitions": [
    {
      "id": 0,
      "compressionGroup": 0,
      "keyGroups": [
        {
          "tags": [
            "structured",
            "groupNames"
          ],
          "keyList": [
            "hello"
          ]
        },
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello"
          ]
        }
      ]
    }
  ]

}
  )";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, mock_metrics_recorder_,
                             fake_key_fetcher_manager_);
  int16_t bhttp_response_code = 0;
  const auto result = GetValuesBasedOnProtocol(core_request_body, &response,
                                               &bhttp_response_code, &handler);
  if (IsUsing<ProtocolType::kPlain>()) {
    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error_code(), grpc::StatusCode::INTERNAL);
  } else {
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(bhttp_response_code, 500);
  }
}

TEST_P(GetValuesHandlerTest, NoCompressionGroup) {
  const std::string core_request_body = R"(
{
  "context": {
    "subkey": "example.com"
  },
  "partitions": [
    {
      "id": 0,
      "keyGroups": [
        {
          "tags": [
            "structured",
            "groupNames"
          ],
          "keyList": [
            "hello"
          ]
        },
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello"
          ]
        }
      ]
    }
  ]

}
  )";
  google::api::HttpBody response;
  privacy_sandbox::server_common::FakeKeyFetcherManager
      fake_key_fetcher_manager;
  GetValuesV2Handler handler(mock_udf_client_, mock_metrics_recorder_,
                             fake_key_fetcher_manager);
  int16_t bhttp_response_code = 0;
  const auto result = GetValuesBasedOnProtocol(core_request_body, &response,
                                               &bhttp_response_code, &handler);
  if (IsUsing<ProtocolType::kPlain>()) {
    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error_code(), grpc::StatusCode::INTERNAL);
  } else {
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(bhttp_response_code, 500);
  }
}

TEST_P(GetValuesHandlerTest, CompressionGroupNotANumber) {
  const std::string core_request_body = R"(
{
  "context": {
    "subkey": "example.com"
  },
  "partitions": [
    {
      "id": 0,
      "compressionGroup": "0",
      "keyGroups": [
        {
          "tags": [
            "structured",
            "groupNames"
          ],
          "keyList": [
            "hello"
          ]
        },
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello"
          ]
        }
      ]
    }
  ]

}
  )";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, mock_metrics_recorder_,
                             fake_key_fetcher_manager_);
  int16_t bhttp_response_code = 0;
  const auto result = GetValuesBasedOnProtocol(core_request_body, &response,
                                               &bhttp_response_code, &handler);
  if (IsUsing<ProtocolType::kPlain>()) {
    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error_code(), grpc::StatusCode::INTERNAL);
  } else {
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(bhttp_response_code, 500);
  }
}

TEST_P(GetValuesHandlerTest, UdfFailureForOnePartition) {
  nlohmann::json udf_input1 = R"({
    "context": {"subkey": "example.com"},
    "keyGroups": [
      {
        "tags": [
          "custom",
          "keys"
        ],
        "keyList": [
          "hello"
        ]
      }
    ],
    "udfInputApiVersion": 1
  })"_json;
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(std::vector<std::string>({udf_input1.dump()})))
      .WillOnce(Return(absl::InternalError("UDF execution error")));

  nlohmann::json udf_input2 = R"({
      "context": {"subkey": "example.com"},
      "keyGroups": [
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello2"
          ]
        }
      ],
    "udfInputApiVersion": 1
    })"_json;
  nlohmann::json udf_output2 = R"({"keyGroupOutputs": [
        {
            "keyValues": {
              "hello2": {
                "value": "world2"
              }
            },
            "tags": [
              "custom",
              "keys"
            ]
        }
      ],
      "udfOutputApiVersion": 1
      })"_json;
  EXPECT_CALL(mock_udf_client_,
              ExecuteCode(std::vector<std::string>({udf_input2.dump()})))
      .WillOnce(Return(udf_output2.dump()));

  const std::string core_request_body = R"(
{
  "context": {
    "subkey": "example.com"
  },
  "partitions": [
    {
      "id": 0,
      "compressionGroup": 0,
      "keyGroups": [
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello"
          ]
        }
      ]
    },
    {
      "id": 1,
      "compressionGroup": 0,
      "keyGroups": [
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello2"
          ]
        }
      ]
    }
  ]
}
  )";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, mock_metrics_recorder_,
                             fake_key_fetcher_manager_);
  int16_t bhttp_response_code = 0;
  const auto result = GetValuesBasedOnProtocol(core_request_body, &response,
                                               &bhttp_response_code, &handler);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();

  nlohmann::json expected = nlohmann::json::parse(R"(
[
  {
    "partitions": [
      {
        "id": 1,
        "keyGroupOutputs": [
          {
            "keyValues": {
              "hello2": {
                "value": "world2"
              }
            },
            "tags": [
              "custom",
              "keys"
            ]
          }
        ]
      }
    ]
  }
])");
  EXPECT_EQ(response.data(), expected.dump());
}

TEST_P(GetValuesHandlerTest, UdfInvalidJsonOutput) {
  std::string udf_output = R"({"keyGroupOutputs": [
        {
            "keyValues": {
              "hello": {
                "value": "world"
              }
            },
            "tags": [
              "custom",
              "keys"
            ]
        },
      ],
      "udfOutputApiVersion": 1
      })";
  EXPECT_CALL(mock_udf_client_, ExecuteCode(_)).WillOnce(Return(udf_output));

  const std::string core_request_body = R"(
{
  "context": {
    "subkey": "example.com"
  },
  "partitions": [
    {
      "id": 0,
      "compressionGroup": 0,
      "keyGroups": [
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello"
          ]
        }
      ]
    }
  ]
}
  )";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, mock_metrics_recorder_,
                             fake_key_fetcher_manager_);
  int16_t bhttp_response_code = 0;
  const auto result = GetValuesBasedOnProtocol(core_request_body, &response,
                                               &bhttp_response_code, &handler);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();
  EXPECT_EQ(response.data(), "null");
}

TEST_P(GetValuesHandlerTest, UdfNoKeyGroupOutputs) {
  EXPECT_CALL(mock_udf_client_, ExecuteCode(_)).WillOnce(Return(R"({})"));

  const std::string core_request_body = R"(
{
  "context": {
    "subkey": "example.com"
  },
  "partitions": [
    {
      "id": 0,
      "compressionGroup": 0,
      "keyGroups": [
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello"
          ]
        }
      ]
    }
  ]
}
  )";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, mock_metrics_recorder_,
                             fake_key_fetcher_manager_);
  int16_t bhttp_response_code = 0;
  const auto result = GetValuesBasedOnProtocol(core_request_body, &response,
                                               &bhttp_response_code, &handler);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();
  EXPECT_EQ(response.data(), "null");
}

TEST_P(GetValuesHandlerTest, UdfKeyGroupOutputsNotArray) {
  std::string udf_output = R"({"keyGroupOutputs":
        {
            "keyValues": {
              "hello": {
                "value": "world"
              }
            },
            "tags": [
              "custom",
              "keys"
            ]
        },
      ,
      "udfOutputApiVersion": 1
      })";
  EXPECT_CALL(mock_udf_client_, ExecuteCode(_)).WillOnce(Return(udf_output));

  const std::string core_request_body = R"(
{
  "context": {
    "subkey": "example.com"
  },
  "partitions": [
    {
      "id": 0,
      "compressionGroup": 0,
      "keyGroups": [
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello"
          ]
        }
      ]
    }
  ]
}
  )";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, mock_metrics_recorder_,
                             fake_key_fetcher_manager_);
  int16_t bhttp_response_code = 0;
  const auto result = GetValuesBasedOnProtocol(core_request_body, &response,
                                               &bhttp_response_code, &handler);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();
  EXPECT_EQ(response.data(), "null");
}

TEST_P(GetValuesHandlerTest, UdfKeyGroupOutputsArrayEmpty) {
  nlohmann::json udf_output = R"({
    "keyGroupOutputs": [],
    "udfOutputApiVersion": 1
  })"_json;
  EXPECT_CALL(mock_udf_client_, ExecuteCode(_))
      .WillOnce(Return(udf_output.dump()));

  const std::string core_request_body = R"(
{
  "context": {
    "subkey": "example.com"
  },
  "partitions": [
    {
      "id": 0,
      "compressionGroup": 0,
      "keyGroups": [
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello"
          ]
        }
      ]
    }
  ]
}
  )";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, mock_metrics_recorder_,
                             fake_key_fetcher_manager_);
  int16_t bhttp_response_code = 0;
  const auto result = GetValuesBasedOnProtocol(core_request_body, &response,
                                               &bhttp_response_code, &handler);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();
  nlohmann::json expected =
      R"([{"partitions":[{"id":0,"keyGroupOutputs":null}]}])"_json;
  EXPECT_EQ(response.data(), expected.dump());
}

TEST_P(GetValuesHandlerTest, NoKeyGroupsInPartition) {
  const std::string core_request_body = R"({
      "context": {
        "subkey": "example.com"
      },
      "partitions": [{
        "id": 0,
        "compressionGroup": 0
      }]
    })";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, mock_metrics_recorder_,
                             fake_key_fetcher_manager_);
  int16_t bhttp_response_code = 0;
  const auto result = GetValuesBasedOnProtocol(core_request_body, &response,
                                               &bhttp_response_code, &handler);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();
  EXPECT_EQ(response.data(), "null");
}

TEST_P(GetValuesHandlerTest, UdfNoOutputApiVersion) {
  EXPECT_CALL(mock_udf_client_, ExecuteCode(_)).WillOnce(Return(R"({
    "keyGroupOutputs": [{
            "keyValues": {
              "hello": {
                "value": "world"
              }
            },
            "tags": [
              "custom",
              "keys"
            ]
        }]
  })"));
  const std::string core_request_body = R"(
{
  "context": {
    "subkey": "example.com"
  },
  "partitions": [
    {
      "id": 0,
      "compressionGroup": 0,
      "keyGroups": [
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello"
          ]
        }
      ]
    }
  ]
}
  )";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, mock_metrics_recorder_,
                             fake_key_fetcher_manager_);
  int16_t bhttp_response_code = 0;
  const auto result = GetValuesBasedOnProtocol(core_request_body, &response,
                                               &bhttp_response_code, &handler);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();
  EXPECT_EQ(response.data(), "null");
}

TEST_P(GetValuesHandlerTest, UdfWrongOutputApiVersion) {
  EXPECT_CALL(mock_udf_client_, ExecuteCode(_)).WillOnce(Return(R"({
    "keyGroupOutputs": [{
            "keyValues": {
              "hello": {
                "value": "world"
              }
            },
            "tags": [
              "custom",
              "keys"
            ]
        }],
    "udfOutputApiVersion": 2
  })"));
  const std::string core_request_body = R"(
{
  "context": {
    "subkey": "example.com"
  },
  "partitions": [
    {
      "id": 0,
      "compressionGroup": 0,
      "keyGroups": [
        {
          "tags": [
            "custom",
            "keys"
          ],
          "keyList": [
            "hello"
          ]
        }
      ]
    }
  ]
}
  )";
  google::api::HttpBody response;
  GetValuesV2Handler handler(mock_udf_client_, mock_metrics_recorder_,
                             fake_key_fetcher_manager_);
  int16_t bhttp_response_code = 0;
  const auto result = GetValuesBasedOnProtocol(core_request_body, &response,
                                               &bhttp_response_code, &handler);
  ASSERT_TRUE(result.ok()) << "code: " << result.error_code()
                           << ", msg: " << result.error_message();
  EXPECT_EQ(response.data(), "null");
}

}  // namespace
}  // namespace kv_server
