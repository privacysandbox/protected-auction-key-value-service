// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/log/initialize.h"
#include "components/data_server/server/parameter_fetcher.h"
#include "components/data_server/server/server_log_init.h"

namespace kv_server {

namespace {

constexpr absl::string_view kUseExternalMetricsCollectorEndpointSuffix =
    "use-external-metrics-collector-endpoint";
constexpr absl::string_view kMetricsCollectorEndpointSuffix =
    "metrics-collector-endpoint";

}  // namespace

absl::optional<std::string> GetMetricsCollectorEndPoint(
    const ParameterClient& parameter_client, const std::string& environment) {
  absl::optional<std::string> metrics_collection_endpoint;
  ParameterFetcher parameter_fetcher(environment, parameter_client);
  auto should_connect_to_external_metrics_collector =
      parameter_fetcher.GetBoolParameter(
          kUseExternalMetricsCollectorEndpointSuffix);
  if (should_connect_to_external_metrics_collector) {
    std::string metrics_collector_endpoint_value =
        parameter_fetcher.GetParameter(kMetricsCollectorEndpointSuffix);
    LOG(INFO) << "Retrieved " << kMetricsCollectorEndpointSuffix
              << " parameter: " << metrics_collector_endpoint_value;
    metrics_collection_endpoint = std::move(metrics_collector_endpoint_value);
  }
  return metrics_collection_endpoint;
}

}  // namespace kv_server
