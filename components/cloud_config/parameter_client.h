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
#ifndef COMPONENTS_CLOUD_CONFIG_PARAMETERCLIENT_H_
#define COMPONENTS_CLOUD_CONFIG_PARAMETERCLIENT_H_
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"

// TODO: Replace config cpio client once ready
namespace kv_server {

// Client to interact with Parameter storage.
class ParameterClient {
 public:
  static std::unique_ptr<ParameterClient> Create();
  virtual ~ParameterClient() = default;

  virtual absl::StatusOr<std::string> GetParameter(
      std::string_view parameter_name) const = 0;

  virtual absl::StatusOr<int32_t> GetInt32Parameter(
      std::string_view parameter_name) const = 0;
};
}  // namespace kv_server

#endif  // COMPONENTS_CLOUD_CONFIG_PARAMETERCLIENT_H_
