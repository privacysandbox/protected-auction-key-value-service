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
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "aws/core/Aws.h"
#include "aws/core/http/HttpClientFactory.h"
#include "aws/core/http/HttpRequest.h"
#include "aws/core/http/HttpResponse.h"
#include "aws/core/internal/AWSHttpResourceClient.h"
#include "aws/core/utils/Outcome.h"
#include "aws/ec2/EC2Client.h"
#include "aws/ec2/model/DescribeTagsRequest.h"
#include "aws/ec2/model/DescribeTagsResponse.h"
#include "aws/ec2/model/Filter.h"
#include "components/cloud_config/environment_client.h"
#include "components/errors/aws_error_util.h"
#include "glog/logging.h"

namespace fledge::kv_server {
namespace {

constexpr char kEnvironmentTag[] = "environment";
constexpr char kResourceIdFilter[] = "resource-id";
constexpr char kKeyFilter[] = "key";
constexpr char kImdsTokenHeader[] = "x-aws-ec2-metadata-token";
constexpr char kImdsTokenTtlSeconds[] = "21600";
constexpr char kImdsTokenTtlHeader[] = "x-aws-ec2-metadata-token-ttl-seconds";
constexpr char kImdsTokenResourcePath[] = "/latest/api/token";
constexpr char kImdsEndpoint[] = "http://169.254.169.254";
constexpr char kInstanceIdResourcePath[] = "/latest/meta-data/instance-id";

absl::StatusOr<std::string> GetAwsHttpResource(
    const Aws::Internal::AWSHttpResourceClient& http_client,
    std::shared_ptr<Aws::Http::HttpRequest> request) {
  Aws::AmazonWebServiceResult<Aws::String> result =
      http_client.GetResourceWithAWSWebServiceResult(request);
  if (result.GetResponseCode() == Aws::Http::HttpResponseCode::OK) {
    return Aws::Utils::StringUtils::Trim(result.GetPayload().c_str());
  }
  return absl::Status(HttpResponseCodeToStatusCode(result.GetResponseCode()),
                      "Failed to get AWS Http resource.");
}

absl::StatusOr<std::string> GetImdsToken(
    const Aws::Internal::AWSHttpResourceClient& http_client) {
  std::shared_ptr<Aws::Http::HttpRequest> token_request(
      Aws::Http::CreateHttpRequest(
          absl::StrCat(kImdsEndpoint, kImdsTokenResourcePath),
          Aws::Http::HttpMethod::HTTP_PUT,
          Aws::Utils::Stream::DefaultResponseStreamFactoryMethod));
  token_request->SetHeaderValue(kImdsTokenTtlHeader, kImdsTokenTtlSeconds);
  Aws::AmazonWebServiceResult<Aws::String> token_result =
      http_client.GetResourceWithAWSWebServiceResult(token_request);
  return GetAwsHttpResource(http_client, token_request);
}

// Returns instance id using IMDSv2. Implementation is based on the following:
// https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/configuring-instance-metadata-service.html
absl::StatusOr<std::string> GetInstanceId(
    const Aws::Internal::AWSHttpResourceClient& http_client) {
  absl::StatusOr<std::string> imds_token = GetImdsToken(http_client);
  if (!imds_token.ok()) {
    return imds_token;
  }
  std::shared_ptr<Aws::Http::HttpRequest> id_request(
      Aws::Http::CreateHttpRequest(
          absl::StrCat(kImdsEndpoint, kInstanceIdResourcePath),
          Aws::Http::HttpMethod::HTTP_GET,
          Aws::Utils::Stream::DefaultResponseStreamFactoryMethod));
  id_request->SetHeaderValue(kImdsTokenHeader, *imds_token);
  return GetAwsHttpResource(http_client, id_request);
}

class AwsEnvironmentClient : public EnvironmentClient {
 public:
  absl::StatusOr<std::string> GetEnvironmentTag() const override {
    absl::StatusOr<std::string> instance_id =
        GetInstanceId(*ec2_metadata_client_);
    if (!instance_id.ok()) {
      return instance_id;
    }

    Aws::EC2::Model::Filter resource_id_filter;
    resource_id_filter.SetName(kResourceIdFilter);
    resource_id_filter.AddValues(*instance_id);
    Aws::EC2::Model::Filter key_filter;
    key_filter.SetName(kKeyFilter);
    key_filter.AddValues(kEnvironmentTag);

    Aws::EC2::Model::DescribeTagsRequest request;
    request.SetFilters({resource_id_filter, key_filter});

    const auto outcome = ec2_client_->DescribeTags(request);
    if (!outcome.IsSuccess()) {
      return AwsErrorToStatus(outcome.GetError());
    }
    if (outcome.GetResult().GetTags().size() != 1) {
      const std::string error_msg =
          absl::StrCat("Could not get tag ", kEnvironmentTag, " for instance ",
                       *instance_id);
      LOG(ERROR) << error_msg;
      return absl::NotFoundError(error_msg);
    }
    return outcome.GetResult().GetTags()[0].GetValue();
  }

  AwsEnvironmentClient()
      : ec2_client_(std::make_unique<Aws::EC2::EC2Client>()),
        ec2_metadata_client_(
            std::make_unique<Aws::Internal::EC2MetadataClient>()) {}

 private:
  std::unique_ptr<Aws::EC2::EC2Client> ec2_client_;
  std::unique_ptr<Aws::Internal::EC2MetadataClient> ec2_metadata_client_;
};

}  // namespace

std::unique_ptr<EnvironmentClient> EnvironmentClient::Create() {
  return std::make_unique<AwsEnvironmentClient>();
}

}  // namespace fledge::kv_server
