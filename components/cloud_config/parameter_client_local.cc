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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/cloud_config/parameter_client.h"

namespace kv_server {
namespace {

class LocalParameterClient : public ParameterClient {
 public:
  absl::StatusOr<std::string> GetParameter(
      std::string_view parameter_name) const override {
    return absl::UnimplementedError("TODO(b/237669491)");
  };

  absl::StatusOr<int32_t> GetInt32Parameter(
      std::string_view parameter_name) const override {
    return absl::UnimplementedError("TODO(b/237669491)");
  };
};

}  // namespace

std::unique_ptr<ParameterClient> ParameterClient::Create() {
  return std::make_unique<LocalParameterClient>();
}

}  // namespace kv_server
