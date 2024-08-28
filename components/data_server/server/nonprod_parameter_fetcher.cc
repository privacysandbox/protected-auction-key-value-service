// Copyright 2024 Google LLC
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

#include <string>

#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "components/data_server/server/parameter_fetcher.h"

namespace kv_server {
constexpr std::string_view kAddChaffShardingClustersParameterSuffix =
    "add-chaff-sharding-clusters";

bool ParameterFetcher::ShouldAddChaffCalloutsToShardCluster() const {
  auto add_chaff_sharding_clusters =
      GetBoolParameter(kAddChaffShardingClustersParameterSuffix);

  PS_LOG(INFO, log_context_)
      << "Retrieved " << kAddChaffShardingClustersParameterSuffix
      << " parameter: " << add_chaff_sharding_clusters;

  return add_chaff_sharding_clusters;
}

}  // namespace kv_server
