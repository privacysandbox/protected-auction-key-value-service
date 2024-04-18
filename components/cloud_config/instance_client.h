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
#ifndef COMPONENTS_CLOUD_CONFIG_INSTANCE_CLIENT_H_
#define COMPONENTS_CLOUD_CONFIG_INSTANCE_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "src/logger/request_context_logger.h"

// TODO: Replace config cpio client once ready
namespace kv_server {

// State to indicate whether an instance is servicing requests or not.
enum class InstanceServiceStatus : int8_t {
  kUnknown = 0,
  kPreService,
  kInService,
  kPostService,
};

struct InstanceInfo {
  std::string id;
  InstanceServiceStatus service_status;
  std::string instance_group;
  std::string private_ip_address;
  absl::flat_hash_map<std::string, std::string> labels;
};

struct AwsDescribeInstanceGroupInput {
  absl::flat_hash_set<std::string>& instance_group_names;
};

struct GcpDescribeInstanceGroupInput {
  std::string project_id;
};

using DescribeInstanceGroupInput =
    std::variant<AwsDescribeInstanceGroupInput, GcpDescribeInstanceGroupInput>;

// Client to perform instance-specific operations.
class InstanceClient {
 public:
  static std::unique_ptr<InstanceClient> Create(
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));
  virtual ~InstanceClient() = default;

  // Retrieves all tags for the current instance and returns the tag with the
  // key "environment".
  virtual absl::StatusOr<std::string> GetEnvironmentTag() = 0;

  // Indicates that the instance is still initializing.
  virtual absl::Status RecordLifecycleHeartbeat(
      std::string_view lifecycle_hook_name) = 0;

  // Calls to complete the instance lifecycle with CONTINUE action.
  virtual absl::Status CompleteLifecycle(
      std::string_view lifecycle_hook_name) = 0;

  // Returns machine id.  May use cached value.
  virtual absl::StatusOr<std::string> GetInstanceId() = 0;

  // Retrieves all tags for the current instance and returns the tag with the
  // key "shard_num".
  virtual absl::StatusOr<std::string> GetShardNumTag() = 0;

  // Retrieves descriptive information about all instances matching the input
  // filter.
  // On AWS, instance groups would map to auto scaling groups. The function
  // will return the ones that match `instance_group_names`.
  // On GCP, instance groups matching project_id and envrionment will be
  // returned.
  virtual absl::StatusOr<std::vector<InstanceInfo>>
  DescribeInstanceGroupInstances(DescribeInstanceGroupInput& input) = 0;

  // Retrieves descriptive information about the given instances.
  virtual absl::StatusOr<std::vector<InstanceInfo>> DescribeInstances(
      const absl::flat_hash_set<std::string>& instance_ids) = 0;

  // Updates the log context reference to enable otel logging for instance
  // client. This function should be called after telemetry is initialized with
  // retrieved parameters.
  virtual void UpdateLogContext(
      privacy_sandbox::server_common::log::PSLogContext& log_context) = 0;
};

}  // namespace kv_server

#endif  // COMPONENTS_CLOUD_CONFIG_INSTANCE_CLIENT_H_
