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

#include "kv_telemetry.h"

#include <string>
#include <utility>

#include "components/util/build_info.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/resource/semantic_conventions.h"
#include "public/constants.h"

using opentelemetry::sdk::resource::Resource;
using opentelemetry::sdk::resource::ResourceAttributes;
namespace semantic_conventions =
    opentelemetry::sdk::resource::SemanticConventions;

namespace kv_server {
constexpr char kShardNumber[] = "shard_number";
Resource CreateKVAttributes(std::string instance_id, std::string shard_number,
                            std::string environment) {
  std::string build_version = std::string(BuildVersion());
  std::string build_platform = std::string(BuildPlatform());
  const auto attributes = ResourceAttributes{
      {semantic_conventions::kServiceName, kServiceName.data()},
      {semantic_conventions::kServiceVersion, std::move(build_version)},
      {semantic_conventions::kHostArch, std::move(build_platform)},
      {semantic_conventions::kDeploymentEnvironment, std::move(environment)},
      {semantic_conventions::kServiceInstanceId, std::move(instance_id)},
      {kShardNumber, std::move(shard_number)}};
  return Resource::Create(attributes);
}
}  // namespace kv_server
