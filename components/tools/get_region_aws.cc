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

#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "absl/cleanup/cleanup.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "aws/core/Aws.h"
#include "aws/core/http/HttpClientFactory.h"
#include "aws/core/http/HttpRequest.h"
#include "aws/core/http/HttpResponse.h"
#include "aws/core/internal/AWSHttpResourceClient.h"
#include "aws/core/utils/Outcome.h"
#include "components/errors/error_util_aws.h"
#include "glog/logging.h"

ABSL_FLAG(std::string, output_file, "", "output_file");

namespace kv_server {
constexpr char kImdsTokenHeader[] = "x-aws-ec2-metadata-token";
constexpr char kImdsTokenTtlSeconds[] = "5";
constexpr char kImdsTokenTtlHeader[] = "x-aws-ec2-metadata-token-ttl-seconds";
constexpr char kImdsTokenResourcePath[] = "/latest/api/token";
constexpr char kImdsEndpoint[] = "http://169.254.169.254";
constexpr char kRegionResourcePath[] = "/latest/meta-data/placement/region";

absl::StatusOr<std::string> GetAwsHttpResource(
    const Aws::Internal::AWSHttpResourceClient& http_client,
    std::shared_ptr<Aws::Http::HttpRequest> request) {
  Aws::AmazonWebServiceResult<std::string> result =
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
  Aws::AmazonWebServiceResult<std::string> token_result =
      http_client.GetResourceWithAWSWebServiceResult(token_request);
  return GetAwsHttpResource(http_client, token_request);
}

// Returns the resource using IMDSv2. Implementation is based on the following:
// https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/configuring-instance-metadata-service.html
absl::StatusOr<std::string> GetResource(
    const Aws::Internal::AWSHttpResourceClient& http_client,
    std::string_view resource_path) {
  absl::StatusOr<std::string> imds_token = GetImdsToken(http_client);
  if (!imds_token.ok()) {
    return imds_token;
  }
  std::shared_ptr<Aws::Http::HttpRequest> id_request(
      Aws::Http::CreateHttpRequest(
          absl::StrCat(kImdsEndpoint, resource_path),
          Aws::Http::HttpMethod::HTTP_GET,
          Aws::Utils::Stream::DefaultResponseStreamFactoryMethod));
  id_request->SetHeaderValue(kImdsTokenHeader, *imds_token);
  return GetAwsHttpResource(http_client, id_request);
}

absl::Status GetRegion() {
  Aws::SDKOptions options;
  Aws::InitAPI(options);
  absl::Cleanup shutdown = [&options] { Aws::ShutdownAPI(options); };

  Aws::Internal::EC2MetadataClient ec2_metadata_client;
  absl::StatusOr<std::string> region =
      GetResource(ec2_metadata_client, kRegionResourcePath);
  if (!region.ok()) {
    LOG(ERROR) << "Failed to create instance client due to GetRegion failure: "
               << region.status();
    return region.status();
  }
  // Create and open file, if file exits, rewrite the content.
  const std::string region_output_file_path = absl::GetFlag(FLAGS_output_file);
  std::ofstream output_file(region_output_file_path);
  if (!output_file.is_open()) {
    return absl::FailedPreconditionError(
        absl::StrCat("ERROR: unable to open the region output file ",
                     region_output_file_path));
  }
  output_file << region.value();
  if (output_file.tellp() != region->size()) {
    output_file.close();
    return absl::FailedPreconditionError(
        absl::StrCat("ERROR: cannot fully write region output file ",
                     region_output_file_path));
  }
  output_file.close();
  return absl::OkStatus();
}
}  // namespace kv_server

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  const absl::Status status = kv_server::GetRegion();
  if (!status.ok()) {
    LOG(FATAL) << "Failed to run: " << status;
  }
  return 0;
}
