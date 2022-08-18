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

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/cloud_config/environment_client.h"

ABSL_FLAG(std::string, environment, "local", "Environment name.");

namespace fledge::kv_server {
namespace {

class LocalEnvironmentClient : public EnvironmentClient {
 public:
  absl::StatusOr<std::string> GetEnvironmentTag() const override {
    return absl::GetFlag(FLAGS_environment);
  }
};

}  // namespace

std::unique_ptr<EnvironmentClient> EnvironmentClient::Create() {
  return std::make_unique<LocalEnvironmentClient>();
}

}  // namespace fledge::kv_server
