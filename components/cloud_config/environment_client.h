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
#ifndef COMPONENTS_CLOUD_CONFIG_ENVIRONMENT_CLIENT_H_
#define COMPONENTS_CLOUD_CONFIG_ENVIRONMENT_CLIENT_H_
#include <string>

#include "absl/status/statusor.h"

// TODO: Replace config cpio client once ready
namespace fledge::kv_server {

// Client to retrieve instance environment.
class EnvironmentClient {
 public:
  static std::unique_ptr<EnvironmentClient> Create();
  virtual ~EnvironmentClient() = default;

  // Retrieves all tags for the current instance and returns the tag with the
  // key "environment".
  virtual absl::StatusOr<std::string> GetEnvironmentTag() const = 0;
};
}  // namespace fledge::kv_server

#endif  // COMPONENTS_CLOUD_CONFIG_ENVIRONMENT_CLIENT_H_
