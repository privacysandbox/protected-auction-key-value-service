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
#include "aws/autoscaling/AutoScalingClient.h"
#include "aws/autoscaling/model/CompleteLifecycleActionRequest.h"
#include "aws/autoscaling/model/DescribeAutoScalingGroupsRequest.h"
#include "aws/autoscaling/model/DescribeAutoScalingGroupsResult.h"
#include "aws/autoscaling/model/DescribeAutoScalingInstancesRequest.h"
#include "aws/autoscaling/model/RecordLifecycleActionHeartbeatRequest.h"
#include "aws/core/Aws.h"
#include "aws/core/http/HttpClientFactory.h"
#include "aws/core/http/HttpRequest.h"
#include "aws/core/http/HttpResponse.h"
#include "aws/core/internal/AWSHttpResourceClient.h"
#include "aws/core/utils/Outcome.h"
#include "aws/ec2/EC2Client.h"
#include "aws/ec2/model/DescribeInstancesRequest.h"
#include "aws/ec2/model/DescribeTagsRequest.h"
#include "aws/ec2/model/DescribeTagsResponse.h"
#include "aws/ec2/model/Filter.h"
#include "components/cloud_config/instance_client.h"
#include "components/errors/error_util_aws.h"
#include "glog/logging.h"

namespace kv_server {
namespace {

using Aws::AutoScaling::Model::DescribeAutoScalingGroupsRequest;
using Aws::AutoScaling::Model::Instance;
using Aws::AutoScaling::Model::LifecycleState;
using Aws::EC2::Model::DescribeInstancesRequest;
using privacy_sandbox::server_common::MetricsRecorder;
using privacy_sandbox::server_common::ScopeLatencyRecorder;

constexpr char kEnvironmentTag[] = "environment";
constexpr char kShardNumTag[] = "shard-num";
constexpr char kResourceIdFilter[] = "resource-id";
constexpr char kKeyFilter[] = "key";
constexpr char kImdsTokenHeader[] = "x-aws-ec2-metadata-token";
constexpr char kImdsTokenTtlSeconds[] = "21600";
constexpr char kImdsTokenTtlHeader[] = "x-aws-ec2-metadata-token-ttl-seconds";
constexpr char kImdsTokenResourcePath[] = "/latest/api/token";
constexpr char kImdsEndpoint[] = "http://169.254.169.254";
constexpr char kInstanceIdResourcePath[] = "/latest/meta-data/instance-id";
constexpr char kContinueAction[] = "CONTINUE";
constexpr char kDescribeInstanceGroupInstancesEvent[] =
    "DescribeInstanceGroupInstances";
constexpr char kDescribeInstancesEvent[] = "DescribeInstances";

const absl::flat_hash_set<LifecycleState> kInstancePreServiceStatuses = {
    LifecycleState::Pending,
    LifecycleState::Pending_Wait,
    LifecycleState::Pending_Proceed,
    LifecycleState::Warmed_Pending,
    LifecycleState::Warmed_Pending_Wait,
    LifecycleState::Warmed_Pending_Proceed,
    LifecycleState::Warmed_Running,
    LifecycleState::EnteringStandby,
    LifecycleState::Standby,
};
const absl::flat_hash_set<LifecycleState> kInstancePostServiceStatuses = {
    LifecycleState::Terminated,
    LifecycleState::Terminating,
    LifecycleState::Terminating_Proceed,
    LifecycleState::Terminating_Wait,
    LifecycleState::Warmed_Terminated,
    LifecycleState::Warmed_Terminating,
    LifecycleState::Warmed_Terminating_Proceed,
    LifecycleState::Warmed_Terminating_Wait,
    LifecycleState::Quarantined,
    LifecycleState::Detached,
    LifecycleState::Detaching,
};

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
absl::StatusOr<std::string> GetInstanceIdInternal(
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

absl::StatusOr<std::string> GetAutoScalingGroupName(
    const Aws::AutoScaling::AutoScalingClient& client,
    std::string_view instance_id) {
  Aws::AutoScaling::Model::DescribeAutoScalingInstancesRequest request;
  request.AddInstanceIds(std::string(instance_id));

  const auto outcome = client.DescribeAutoScalingInstances(request);
  if (!outcome.IsSuccess()) {
    return AwsErrorToStatus(outcome.GetError());
  }
  if (outcome.GetResult().GetAutoScalingInstances().size() != 1) {
    const std::string error_msg = absl::StrCat(
        "Could not get auto scaling instances for instance ", instance_id,
        ". Retrieved ", outcome.GetResult().GetAutoScalingInstances().size(),
        " auto scaling groups.");
    return absl::NotFoundError(error_msg);
  }
  return outcome.GetResult()
      .GetAutoScalingInstances()[0]
      .GetAutoScalingGroupName();
}

InstanceServiceStatus GetInstanceServiceStatus(const Instance& instance) {
  if (instance.GetLifecycleState() == LifecycleState::InService) {
    return InstanceServiceStatus::kInService;
  }
  if (kInstancePreServiceStatuses.contains(instance.GetLifecycleState())) {
    return InstanceServiceStatus::kPreService;
  }
  if (kInstancePostServiceStatuses.contains(instance.GetLifecycleState())) {
    return InstanceServiceStatus::kPostService;
  }
  return InstanceServiceStatus::kUnknown;
}

class AwsInstanceClient : public InstanceClient {
 public:
  absl::StatusOr<std::string> GetEnvironmentTag() override {
    return GetTag(kEnvironmentTag);
  }

  absl::StatusOr<std::string> GetShardNumTag() override {
    return GetTag(kShardNumTag);
  }

  absl::Status RecordLifecycleHeartbeat(
      std::string_view lifecycle_hook_name) override {
    const absl::StatusOr<std::string> instance_id = GetInstanceId();
    if (!instance_id.ok()) {
      LOG(ERROR) << "Failed to get instance_id: " << instance_id.status();
      return instance_id.status();
    }
    LOG(INFO) << "Retrieved instance id: " << *instance_id;

    const absl::StatusOr<std::string> auto_scaling_group_name =
        GetAutoScalingGroupName(*auto_scaling_client_, *instance_id);
    if (!auto_scaling_group_name.ok()) {
      return auto_scaling_group_name.status();
    }
    LOG(INFO) << "Retrieved auto scaling group name "
              << *auto_scaling_group_name;

    Aws::AutoScaling::Model::RecordLifecycleActionHeartbeatRequest request;
    request.SetAutoScalingGroupName(*auto_scaling_group_name);
    request.SetLifecycleHookName(std::string(lifecycle_hook_name));
    request.SetInstanceId(*instance_id);

    const auto outcome =
        auto_scaling_client_->RecordLifecycleActionHeartbeat(request);
    if (!outcome.IsSuccess()) {
      return AwsErrorToStatus(outcome.GetError());
    }
    return absl::OkStatus();
  }

  absl::Status CompleteLifecycle(
      std::string_view lifecycle_hook_name) override {
    const absl::StatusOr<std::string> instance_id = GetInstanceId();
    if (!instance_id.ok()) {
      LOG(ERROR) << "Failed to get instance_id: " << instance_id.status();
      return instance_id.status();
    }
    LOG(INFO) << "Retrieved instance id: " << *instance_id;

    const absl::StatusOr<std::string> auto_scaling_group_name =
        GetAutoScalingGroupName(*auto_scaling_client_, *instance_id);
    if (!auto_scaling_group_name.ok()) {
      return auto_scaling_group_name.status();
    }
    LOG(INFO) << "Retrieved auto scaling group name "
              << *auto_scaling_group_name;

    Aws::AutoScaling::Model::CompleteLifecycleActionRequest request;
    request.SetAutoScalingGroupName(*auto_scaling_group_name);
    request.SetLifecycleHookName(std::string(lifecycle_hook_name));
    request.SetInstanceId(*instance_id);
    request.SetLifecycleActionResult(kContinueAction);

    const auto outcome = auto_scaling_client_->CompleteLifecycleAction(request);
    if (!outcome.IsSuccess()) {
      const auto status = AwsErrorToStatus(outcome.GetError());
      if (!absl::IsInvalidArgument(status)) {
        return status;
      }
      // TODO: add metric
      // Invalid argument errors are returned if the lifecycle is already
      // complete. Ignore this error because it is unlikely helpful to retry it.
      // If we are already complete, it doesn't matter
      // If we aren't, we should probably restart anyway.
    }
    return absl::OkStatus();
  }
  absl::StatusOr<std::string> GetInstanceId() override {
    if (!machine_id_.empty()) {
      return machine_id_;
    }
    auto result = GetInstanceIdInternal(*ec2_metadata_client_);
    if (!result.ok()) {
      return result;
    }
    machine_id_ = std::move(result.value());
    return machine_id_;
  }

  absl::StatusOr<std::vector<InstanceInfo>> DescribeInstanceGroupInstances(
      DescribeInstanceGroupInput& describe_instance_group_input) override {
    auto input = std::get_if<AwsDescribeInstanceGroupInput>(
        &describe_instance_group_input);
    CHECK(input) << "AwsDescribeInstanceGroupInput invalid";
    const auto& instance_groups = input->instance_group_names;
    std::vector<InstanceInfo> instances;
    DescribeAutoScalingGroupsRequest request;
    request.SetAutoScalingGroupNames(
        {instance_groups.begin(), instance_groups.end()});
    std::string next_token;
    while (true) {
      if (!next_token.empty()) {
        request.SetNextToken(next_token);
      }
      auto outcome = auto_scaling_client_->DescribeAutoScalingGroups(request);
      if (!outcome.IsSuccess()) {
        return AwsErrorToStatus(outcome.GetError());
      }
      const auto& result = outcome.GetResultWithOwnership();
      for (const auto& auto_scaling_group : result.GetAutoScalingGroups()) {
        for (const auto& instance : auto_scaling_group.GetInstances()) {
          InstanceInfo instance_info;
          instance_info.instance_group =
              auto_scaling_group.GetAutoScalingGroupName();
          instance_info.id = instance.GetInstanceId();
          instance_info.service_status = GetInstanceServiceStatus(instance);
          instances.push_back(instance_info);
        }
      }
      if (next_token = result.GetNextToken(); next_token.empty()) {
        break;
      }
    }
    return instances;
  }

  absl::StatusOr<std::vector<InstanceInfo>> DescribeInstances(
      const absl::flat_hash_set<std::string>& instance_ids) override {
    std::vector<InstanceInfo> instances;
    DescribeInstancesRequest request;
    request.SetInstanceIds({instance_ids.begin(), instance_ids.end()});
    std::string next_token;
    while (true) {
      if (!next_token.empty()) {
        request.SetNextToken(next_token);
      }
      auto outcome = ec2_client_->DescribeInstances(request);
      if (!outcome.IsSuccess()) {
        return AwsErrorToStatus(outcome.GetError());
      }
      const auto& result = outcome.GetResultWithOwnership();
      for (const auto& reservation : result.GetReservations()) {
        for (const auto& instance : reservation.GetInstances()) {
          InstanceInfo instance_info;
          instance_info.id = instance.GetInstanceId();
          instance_info.private_ip_address = instance.GetPrivateIpAddress();
          instance_info.service_status = InstanceServiceStatus::kUnknown;
          instances.push_back(instance_info);
        }
      }
      if (next_token = result.GetNextToken(); next_token.empty()) {
        break;
      }
    }
    return instances;
  }

  explicit AwsInstanceClient(MetricsRecorder& metrics_recorder)
      : metrics_recorder_(metrics_recorder),
        ec2_client_(std::make_unique<Aws::EC2::EC2Client>()),
        // EC2MetadataClient does not fall back to the default client
        // configuration, needs to specify it to
        //  fall back default configuration such as connectTimeoutMs (1000ms)
        //  and requestTimeoutMs (3000ms).
        ec2_metadata_client_(std::make_unique<Aws::Internal::EC2MetadataClient>(
            Aws::Client::ClientConfiguration())),
        auto_scaling_client_(
            std::make_unique<Aws::AutoScaling::AutoScalingClient>()) {}

 private:
  MetricsRecorder& metrics_recorder_;
  std::unique_ptr<Aws::EC2::EC2Client> ec2_client_;
  std::unique_ptr<Aws::Internal::EC2MetadataClient> ec2_metadata_client_;
  std::unique_ptr<Aws::AutoScaling::AutoScalingClient> auto_scaling_client_;
  std::string machine_id_;

  absl::StatusOr<std::string> GetTag(std::string tag) {
    absl::StatusOr<std::string> instance_id = GetInstanceId();
    if (!instance_id.ok()) {
      LOG(ERROR) << "Failed to get instance_id: " << instance_id.status();
      return instance_id;
    }
    LOG(INFO) << "Retrieved instance id: " << *instance_id;
    Aws::EC2::Model::Filter resource_id_filter;
    resource_id_filter.SetName(kResourceIdFilter);
    resource_id_filter.AddValues(*instance_id);
    Aws::EC2::Model::Filter key_filter;
    key_filter.SetName(kKeyFilter);
    key_filter.AddValues(tag);

    Aws::EC2::Model::DescribeTagsRequest request;
    request.SetFilters({resource_id_filter, key_filter});

    LOG(INFO) << "Sending Aws::EC2::Model::DescribeTagsRequest to get tag: "
              << tag;
    const auto outcome = ec2_client_->DescribeTags(request);
    if (!outcome.IsSuccess()) {
      LOG(ERROR) << "Failed to get tag: " << outcome.GetError();
      return AwsErrorToStatus(outcome.GetError());
    }
    if (outcome.GetResult().GetTags().size() != 1) {
      const std::string error_msg = absl::StrCat(
          "Could not get tag ", tag, " for instance ", *instance_id);
      LOG(ERROR) << error_msg << "; Retrieved "
                 << outcome.GetResult().GetTags().size() << " tags";
      return absl::NotFoundError(error_msg);
    }
    return outcome.GetResult().GetTags()[0].GetValue();
  }
};

}  // namespace

std::unique_ptr<InstanceClient> InstanceClient::Create(
    MetricsRecorder& metrics_recorder) {
  return std::make_unique<AwsInstanceClient>(metrics_recorder);
}

}  // namespace kv_server
