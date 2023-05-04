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

#include "absl/status/statusor.h"

// TODO: Replace config cpio client once ready
namespace kv_server {

// Client to perform instance-specific operations.
class InstanceClient {
 public:
  static std::unique_ptr<InstanceClient> Create();
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
};

}  // namespace kv_server

#endif  // COMPONENTS_CLOUD_CONFIG_INSTANCE_CLIENT_H_
