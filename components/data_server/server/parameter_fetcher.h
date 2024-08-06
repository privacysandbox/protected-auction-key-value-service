/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMPONENTS_DATA_SERVER_SERVER_PARAMETER_FETCHER_H_
#define COMPONENTS_DATA_SERVER_SERVER_PARAMETER_FETCHER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/status/statusor.h"
#include "components/cloud_config/parameter_client.h"
#include "components/data/blob_storage/blob_storage_change_notifier.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/telemetry/server_definition.h"

namespace kv_server {

class ParameterFetcher {
 public:
  // `Metric callback` is optional
  ParameterFetcher(
      std::string environment, const ParameterClient& parameter_client,
      absl::AnyInvocable<void(const absl::Status& status, int count) const>
          metric_callback = LogMetricsNoOpCallback(),
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));

  virtual ~ParameterFetcher() = default;

  // This function will retry any necessary requests until it succeeds.
  virtual std::string GetParameter(
      std::string_view parameter_suffix,
      std::optional<std::string> default_value = std::nullopt) const;

  // This function will retry any necessary requests until it succeeds.
  virtual int32_t GetInt32Parameter(std::string_view parameter_suffix) const;

  // This function will retry any necessary requests until it succeeds.
  virtual bool GetBoolParameter(std::string_view parameter_suffix) const;

  virtual NotifierMetadata GetBlobStorageNotifierMetadata() const;

  virtual BlobStorageClient::ClientOptions GetBlobStorageClientOptions() const;

  virtual NotifierMetadata GetRealtimeNotifierMetadata(int32_t num_shards,
                                                       int32_t shard_num) const;
  virtual NotifierMetadata GetLoggingVerbosityParameterNotifierMetadata() const;

  std::string GetParamName(std::string_view parameter_suffix) const;

 protected:
  privacy_sandbox::server_common::log::PSLogContext& log_context_;

 private:
  const std::string environment_;
  const ParameterClient& parameter_client_;
  absl::AnyInvocable<void(const absl::Status& status, int count) const>
      metrics_callback_;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_SERVER_PARAMETER_FETCHER_H_
