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

#ifndef COMPONENTS_DATA_SERVER_SERVER_MOCKS_H_
#define COMPONENTS_DATA_SERVER_SERVER_MOCKS_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "components/cloud_config/instance_client.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
class MockInstanceClient : public InstanceClient {
 public:
  MOCK_METHOD(absl::StatusOr<std::string>, GetEnvironmentTag, (), (override));
  MOCK_METHOD(absl::StatusOr<std::string>, GetShardNumTag, (), (override));
  MOCK_METHOD(absl::Status, RecordLifecycleHeartbeat,
              (std::string_view lifecycle_hook_name), (override));
  MOCK_METHOD(absl::Status, CompleteLifecycle,
              (std::string_view lifecycle_hook_name), (override));
  MOCK_METHOD(absl::StatusOr<std::string>, GetInstanceId, (), (override));
  MOCK_METHOD(absl::StatusOr<std::vector<InstanceInfo>>,
              DescribeInstanceGroupInstances,
              (const absl::flat_hash_set<std::string>&), (override));
  MOCK_METHOD(absl::StatusOr<std::vector<InstanceInfo>>, DescribeInstances,
              (const absl::flat_hash_set<std::string>&), (override));
};

}  // namespace kv_server
#endif  // COMPONENTS_DATA_SERVER_SERVER_MOCKS_H_
