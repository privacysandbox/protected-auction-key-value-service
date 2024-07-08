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

#ifndef COMPONENTS_DATA_SERVER_SERVER_LIFECYCLE_HEARTBEAT_H_
#define COMPONENTS_DATA_SERVER_SERVER_LIFECYCLE_HEARTBEAT_H_

#include <memory>

#include "absl/status/status.h"
#include "components/cloud_config/instance_client.h"
#include "components/data_server/server/parameter_fetcher.h"
#include "components/util/periodic_closure.h"
#include "src/logger/request_context_logger.h"

namespace kv_server {

class LifecycleHeartbeat {
 public:
  virtual ~LifecycleHeartbeat() = default;

  virtual absl::Status Start(const ParameterFetcher& parameter_fetcher) = 0;
  virtual void Finish() = 0;

  static std::unique_ptr<LifecycleHeartbeat> Create(
      InstanceClient& instance_client,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));

  // For testing
  static std::unique_ptr<LifecycleHeartbeat> Create(
      std::unique_ptr<PeriodicClosure> heartbeat,
      InstanceClient& instance_client,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_SERVER_LIFECYCLE_HEARTBEAT_H_
