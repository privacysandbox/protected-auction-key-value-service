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

#include <memory>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/flags.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "components/cloud_config/parameter_client.h"
#include "components/data/converters/cbor_converter.h"
#include "components/data_server/request_handler/encryption/ohttp_client_encryptor.h"
#include "components/data_server/request_handler/get_values_v2_handler.h"
#include "components/data_server/server/key_fetcher_factory.h"
#include "components/data_server/server/parameter_fetcher.h"
#include "components/tools/util/configure_telemetry_tools.h"
#include "components/util/platform_initializer.h"
#include "nlohmann/json.hpp"
#include "public/applications/pa/response_utils.h"
#include "public/constants.h"
#include "public/query/cpp/grpc_client.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"
#include "src/communication/encoding_utils.h"
#include "src/communication/framing_utils.h"

#include "cbor.h"

ABSL_DECLARE_FLAG(std::string, gcp_project_id);

ABSL_FLAG(std::string, kv_endpoint, "<ip>:50051", "KV grpc endpoint");
ABSL_FLAG(int, inclusive_upper_bound, 999999999, "Inclusive upper bound");
ABSL_FLAG(int, qps, 5, "qps");
ABSL_FLAG(int, number_of_requests_to_make, 1, "Number of requests to make");
ABSL_FLAG(int, value_size, 10000, "Specify the size of value for the key");
ABSL_FLAG(int, batch_size, 10, "Batch size");
ABSL_FLAG(std::string, key_prefix, "foo", "Key prefix");
ABSL_FLAG(bool, use_tls, false, "Whether to use TLS for grpc calls.");
ABSL_FLAG(std::string, environment, "NOT_SPECIFIED", "Environment name.");
ABSL_FLAG(bool, use_coordinator, false,
          "Whether to use coordinator for query encryption.");

namespace kv_server {
namespace {
inline constexpr std::string_view kPublicKeyEndpointParameterSuffix =
    "public-key-endpoint";
inline constexpr std::string_view kUseRealCoordinatorsParameterSuffix =
    "use-real-coordinators";

absl::BitGen bitgen;
int total_failures = 0;
int total_mismatches = 0;

privacy_sandbox::server_common::CloudPlatform GetCloudPlatform() {
#if defined(CLOUD_PLATFORM_AWS)
  return privacy_sandbox::server_common::CloudPlatform::kAws;
#elif defined(CLOUD_PLATFORM_GCP)
  return privacy_sandbox::server_common::CloudPlatform::kGcp;
#endif
  return privacy_sandbox::server_common::CloudPlatform::kLocal;
}

int64_t Get(int64_t upper_bound) {
  return absl::Uniform(bitgen, 0, upper_bound);
}

// Cannot use existing CborDecodeToNonBytesProto, since that doesn't handle
// bytes (content field). To fix this, convert the content from a
// cbor serialized list to a proto serialized list.
//
// This function is only used for testing purposes and the output might
// not accurately represent what an actual response proto content would look
// like.
absl::Status CborDecodeToGetValuesResponseProto(
    std::string_view cbor_raw, v2::GetValuesResponse& response) {
  nlohmann::json json_from_cbor = nlohmann::json::from_cbor(
      cbor_raw, /*strict=*/true, /*allow_exceptions=*/false);
  if (json_from_cbor.is_discarded()) {
    return absl::InternalError("Failed to convert raw CBOR buffer to JSON");
  }
  for (auto& json_compression_group : json_from_cbor["compressionGroups"]) {
    // Convert CBOR serialized list to a JSON list of partition outputs
    PS_ASSIGN_OR_RETURN(
        auto json_partition_outputs,
        GetPartitionOutputsInJson(json_compression_group["content"]));

    // Put the JSON list of partition outputs into a V2CompressionGroup
    // This is necessary so that we can properly serialize the proto.
    // The proto library only supports serializing a message, not
    // a vector/list of protos.
    application_pa::V2CompressionGroup v2_compression_group;
    for (const auto& json_partition_output : json_partition_outputs) {
      PS_ASSIGN_OR_RETURN(*v2_compression_group.add_partition_outputs(),
                          application_pa::PartitionOutputFromJson(
                              json_partition_output.dump()));
    }
    // Serialize the V2CompressionGroup and set it as the compression group's
    // content
    LOG(INFO) << "V2 compression group: " << v2_compression_group;
    std::string serialized_content;
    if (!v2_compression_group.SerializeToString(&serialized_content)) {
      return absl::InternalError(absl::StrCat(
          "Failed to serialize proto to string: ", v2_compression_group));
    }
    json_compression_group.erase("content");
    auto* compression_group = response.add_compression_groups();
    PS_RETURN_IF_ERROR(google::protobuf::util::JsonStringToMessage(
        json_compression_group.dump(), compression_group));
    compression_group->set_content(std::move(serialized_content));
  }
  return absl::OkStatus();
}

absl::StatusOr<google::cmrt::sdk::public_key_service::v1::PublicKey>
GetPublicKey(std::unique_ptr<kv_server::ParameterFetcher>& parameter_fetcher) {
  if (!parameter_fetcher->GetBoolParameter(
          kUseRealCoordinatorsParameterSuffix)) {
    // The key_fetcher_manager would just return hard coded public key without
    // involving private key fetching
    auto factory = kv_server::KeyFetcherFactory::Create();
    auto key_fetcher_manager =
        factory->CreateKeyFetcherManager(*parameter_fetcher);
    auto maybe_public_key =
        key_fetcher_manager->GetPublicKey(GetCloudPlatform());
    if (!maybe_public_key.ok()) {
      const std::string error =
          absl::StrCat("Could not get public key to use for HPKE encryption:",
                       maybe_public_key.status().message());
      LOG(ERROR) << error;
      return absl::InternalError(error);
    }
    return maybe_public_key.value();
  }

  auto publicKeyEndpointParameter =
      parameter_fetcher->GetParameter(kPublicKeyEndpointParameterSuffix);
  LOG(INFO) << "Retrieved public_key_endpoint parameter: "
            << publicKeyEndpointParameter;
  std::vector<std::string> endpoints = {publicKeyEndpointParameter};
  auto public_key_fetcher =
      privacy_sandbox::server_common::PublicKeyFetcherFactory::Create(
          {{GetCloudPlatform(), endpoints}});
  if (public_key_fetcher) {
    absl::Status public_key_refresh_status = public_key_fetcher->Refresh();
    if (!public_key_refresh_status.ok()) {
      const std::string error = absl::StrCat(
          "Public key refresh failed: ", public_key_refresh_status.message());
      LOG(ERROR) << error;
      return absl::InternalError(error);
    }
  }
  return public_key_fetcher->GetKey(GetCloudPlatform());
}

absl::StatusOr<v2::GetValuesResponse> GetValuesWithCoordinators(
    const v2::GetValuesRequest& proto_req,
    std::unique_ptr<v2::KeyValueService::Stub>& stub,
    std::unique_ptr<google::cmrt::sdk::public_key_service::v1::PublicKey>&
        public_key) {
  std::string serialized_req;
  PS_ASSIGN_OR_RETURN(serialized_req,
                      V2GetValuesRequestProtoToCborEncode(proto_req));
  auto encoded_data_size = privacy_sandbox::server_common::GetEncodedDataSize(
      serialized_req.size(), kMinResponsePaddingBytes);
  auto maybe_padded_request =
      privacy_sandbox::server_common::EncodeResponsePayload(
          privacy_sandbox::server_common::CompressionType::kUncompressed,
          std::move(serialized_req), encoded_data_size);
  if (!maybe_padded_request.ok()) {
    LOG(ERROR) << "Padding failed: " << maybe_padded_request.status().message();
    return maybe_padded_request.status();
  }
  if (!public_key) {
    const std::string error = "public_key==nullptr, cannot proceed.";
    LOG(ERROR) << error;
    return absl::InternalError(error);
  }
  OhttpClientEncryptor encryptor(*public_key);
  auto encrypted_serialized_request_maybe =
      encryptor.EncryptRequest(*maybe_padded_request);
  if (!encrypted_serialized_request_maybe.ok()) {
    return encrypted_serialized_request_maybe.status();
  }
  v2::ObliviousGetValuesRequest ohttp_req;
  ohttp_req.mutable_raw_body()->set_data(*encrypted_serialized_request_maybe);
  google::api::HttpBody ohttp_res;
  grpc::ClientContext context;
  context.AddMetadata(std::string(kKVContentTypeHeader),
                      std::string(kContentEncodingCborHeaderValue));
  grpc::Status status =
      stub->ObliviousGetValues(&context, ohttp_req, &ohttp_res);
  if (!status.ok()) {
    LOG(ERROR) << status.error_code() << ": " << status.error_message();
    return absl::Status((absl::StatusCode)status.error_code(),
                        status.error_message());
  }
  auto decrypted_ohttp_response_maybe =
      encryptor.DecryptResponse(std::move(ohttp_res.data()));
  if (!decrypted_ohttp_response_maybe.ok()) {
    LOG(ERROR) << "ohttp response decryption failed!";
    return decrypted_ohttp_response_maybe.status();
  }
  auto deframed_req = privacy_sandbox::server_common::DecodeRequestPayload(
      *decrypted_ohttp_response_maybe);
  if (!deframed_req.ok()) {
    LOG(ERROR) << "unpadding response failed!";
    return deframed_req.status();
  }
  v2::GetValuesResponse get_values_response;
  PS_RETURN_IF_ERROR(CborDecodeToGetValuesResponseProto(
      deframed_req->compressed_data, get_values_response));
  LOG(INFO) << "response: " << get_values_response;
  return get_values_response;
}

v2::GetValuesRequest GetRequest(const std::vector<std::string>& input_values) {
  v2::GetValuesRequest req;
  v2::RequestPartition* partition = req.add_partitions();
  auto* udf_argument = partition->add_arguments();
  auto* values = udf_argument->mutable_data()->mutable_list_value();
  for (const auto& v : input_values) {
    values->add_values()->set_string_value(v);
  }
  udf_argument->mutable_tags()->add_values()->set_string_value("keys");
  udf_argument->mutable_tags()->add_values()->set_string_value("custom");
  return req;
}

absl::StatusOr<std::string> GetValueFromResponse(
    absl::StatusOr<v2::GetValuesResponse>& maybe_response,
    const std::string& key) {
  if (!maybe_response.ok()) {
    return maybe_response.status();
  }
  // We are only sending 1 partition, so should only get 1 partition back.
  if (maybe_response->compression_groups().size() != 1) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Expected compression group size is 1, but found %s.",
        std::to_string(maybe_response->compression_groups().size())));
  }
  // TODO(b/355464083): Will need to uncompress once compression is implemented
  auto content = maybe_response->compression_groups(0).content();
  application_pa::V2CompressionGroup v2_compression_group;
  if (!v2_compression_group.ParseFromString(content)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Could not parse V2CompressionGroup from content: ", content));
  }

  // Expecting 1 partition in the key group output
  if (v2_compression_group.partition_outputs_size() != 1) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Expected key group output to be a list of size 1, but received: ",
        v2_compression_group));
  }

  auto partition_output = v2_compression_group.partition_outputs(0);
  // Expecting 1 kv in the response key group output
  if (partition_output.key_group_outputs_size() != 1) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Expected key group output to be a list of size 1, but received: ",
        partition_output));
  }
  auto key_group_outputs = partition_output.key_group_outputs();
  if (key_group_outputs.empty()) {
    return absl::InvalidArgumentError("key_group_outputs empty");
  }
  auto kv = key_group_outputs[0].key_values();
  if (auto it = kv.find(key); it == kv.end()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Cannot find the key  %s.", key));
  } else {
    auto value = it->second;
    return value.value().string_value();
  }
}

std::vector<std::string> GetKeys(int start_index, int batch_size) {
  const std::string key_prefix = absl::GetFlag(FLAGS_key_prefix);
  std::vector<std::string> res;
  auto idx = start_index;
  while (idx < start_index + batch_size) {
    const std::string key = absl::StrCat(key_prefix, idx);
    LOG(INFO) << "Reading key: " << key;
    res.emplace_back(key);
    idx++;
  }
  return res;
}

void ValidateResponse(absl::StatusOr<v2::GetValuesResponse> maybe_response,
                      std::vector<std::string>& keys) {
  const int value_size = absl::GetFlag(FLAGS_value_size);
  for (const auto& key : keys) {
    LOG(INFO) << "Validating key: " << key;
    auto maybe_response_value = GetValueFromResponse(maybe_response, key);
    if (!maybe_response_value.ok()) {
      total_failures++;
      LOG(ERROR) << maybe_response_value.status().message();
      continue;
    }
    auto response_value = *maybe_response_value;
    const std::string expected_value(value_size, key[key.size() - 1]);
    if (response_value != expected_value) {
      total_mismatches++;
      LOG(ERROR) << "Expected value: " << expected_value
                 << " Actual value: " << response_value;
    } else {
      LOG(INFO) << "matches";
    }
  }
}

void Validate(
    std::unique_ptr<google::cmrt::sdk::public_key_service::v1::PublicKey>&
        public_key) {
  const std::string kv_endpoint = absl::GetFlag(FLAGS_kv_endpoint);
  const int inclusive_upper_bound = absl::GetFlag(FLAGS_inclusive_upper_bound);
  const int batch_size = absl::GetFlag(FLAGS_batch_size);
  const int qps = absl::GetFlag(FLAGS_qps);
  const int number_of_requests_to_make =
      absl::GetFlag(FLAGS_number_of_requests_to_make);
  const bool use_tls = absl::GetFlag(FLAGS_use_tls);
  int requests_made_this_second = 0;
  int total_requests_made = 0;
  auto batch_end = absl::Now() + absl::Seconds(1);
  std::unique_ptr<v2::KeyValueService::Stub> stub;
  if (use_tls) {
    stub = GrpcClient::CreateStub(
        kv_endpoint, grpc::SslCredentials(grpc::SslCredentialsOptions()));
  } else {
    stub =
        GrpcClient::CreateStub(kv_endpoint, grpc::InsecureChannelCredentials());
  }
  GrpcClient client(*stub);
  while (total_requests_made < number_of_requests_to_make) {
    auto random_index = Get(inclusive_upper_bound / batch_size);
    random_index *= batch_size;
    std::vector<std::string> keys = GetKeys(random_index, batch_size);
    auto req = GetRequest(keys);
    absl::StatusOr<v2::GetValuesResponse> get_value_response;
    if (absl::GetFlag(FLAGS_use_coordinator)) {
      get_value_response = GetValuesWithCoordinators(req, stub, public_key);
    } else {
      get_value_response = client.GetValues(req);
    }
    ValidateResponse(get_value_response, keys);
    requests_made_this_second++;
    // rate limit to N files per second
    if (requests_made_this_second % qps == 0) {
      if (batch_end > absl::Now()) {
        absl::SleepFor(batch_end - absl::Now());
        LOG(INFO) << ": sleeping ";
      }
      batch_end += absl::Seconds(1);
    }
    total_requests_made++;
  }

  LOG(INFO) << "Validated " << batch_size * total_requests_made
            << " key-value pairs \n";
}

}  // namespace
}  // namespace kv_server

// This tool will query the specified _kv_endpoint_ endpoint _qps_ number of
// times per second. The total amount of request is
// _number_of_requests_to_make_. Each request has _batch_size_ number of keys to
// lookup. The assumptions for these tests are following. The keys loaded to the
// kv server are of format _key_prefix_{0....inclusive_upper_bound} Each value
// is deterministically mapped from the key -- const std::string
// expected_value(value_size, key[key.size() - 1]); For each request a random
// key from the key space is selected. And the request look up that key and
// _batch_size_ of the sequential keys.
// Sample command:
// bazel run //components/tools/sharding_correctness_validator:validator --
// --qps=5 --number_of_requests_to_make=300 --batch_size=5
// --kv_endpoint=<your_ip>:50051

int main(int argc, char** argv) {
  const std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  kv_server::ConfigureTelemetryForTools();

  // ptrs for validation with coordinators
  std::unique_ptr<kv_server::PlatformInitializer> platform_initializer;
  std::unique_ptr<kv_server::ParameterClient> parameter_client;
  std::unique_ptr<kv_server::ParameterFetcher> parameter_fetcher;
  std::unique_ptr<google::cmrt::sdk::public_key_service::v1::PublicKey>
      public_key;

  if (absl::GetFlag(FLAGS_use_coordinator)) {
    // Initializes GCP platform and its parameter client.
    platform_initializer = std::make_unique<kv_server::PlatformInitializer>();
    parameter_client = kv_server::ParameterClient::Create();

    // Gets environment name
    std::string environment = absl::GetFlag(FLAGS_environment);
    if (environment == "NOT_SPECIFIED") {
      LOG(ERROR) << "Flag environment is required to get key fetch parameters";
      return 1;
    }

    // Create parameter fetcher and key fetcher manager
    auto parameter_fetcher = std::make_unique<kv_server::ParameterFetcher>(
        environment, *parameter_client);
    auto maybe_public_key = kv_server::GetPublicKey(parameter_fetcher);
    if (!maybe_public_key.ok()) {
      LOG(ERROR) << "GetPublicKey failed with error: "
                 << maybe_public_key.status().message();
      return 1;
    }
    public_key =
        std::make_unique<google::cmrt::sdk::public_key_service::v1::PublicKey>(
            maybe_public_key.value());
  }
  kv_server::Validate(public_key);

  if (kv_server::total_failures > 0 || kv_server::total_mismatches > 0) {
    LOG(ERROR) << "Validation failed with total_failures: "
               << kv_server::total_failures
               << ", total_mismatches: " << kv_server::total_mismatches;
    return 1;
  }
  LOG(INFO) << "Query Validation succeed!";
  return 0;
}
