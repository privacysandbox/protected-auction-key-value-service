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

#ifndef COMPONENTS_UDF_UDF_CLIENT_H_
#define COMPONENTS_UDF_UDF_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/telemetry/server_definition.h"
#include "components/udf/code_config.h"
#include "components/util/request_context.h"
#include "google/protobuf/message.h"
#include "public/api_schema.pb.h"
#include "src/logger/request_context_logger.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"

namespace kv_server {

struct ExecutionMetadata {
  std::optional<int64_t> custom_code_total_execution_time_micros;
};

// Client to execute UDF
class UdfClient {
 public:
  virtual ~UdfClient() = default;

  // This interface is too liberal. We may need to change this to an internal
  // function so the public interface aligns with our public documentation on
  // UDF signature.
  ABSL_DEPRECATED("Use ExecuteCode(metadata, arguments) instead")
  virtual absl::StatusOr<std::string> ExecuteCode(
      const RequestContextFactory& request_context_factory,
      std::vector<std::string> keys,
      ExecutionMetadata& execution_metadata) const = 0;

  // Executes the UDF. Code object must be set before making
  // this call.
  virtual absl::StatusOr<std::string> ExecuteCode(
      const RequestContextFactory& request_context_factory,
      UDFExecutionMetadata&& execution_metadata,
      const google::protobuf::RepeatedPtrField<UDFArgument>& arguments,
      ExecutionMetadata& metadata) const = 0;

  virtual absl::Status Stop() = 0;

  // Sets the code object that will be used for UDF execution
  virtual absl::Status SetCodeObject(
      CodeConfig code_config,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext)) = 0;

  // Sets the WASM code object that will be used for UDF execution
  virtual absl::Status SetWasmCodeObject(
      CodeConfig code_config,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext)) = 0;

  // Creates a UDF executor. This calls Roma::Init, which forks.
  static absl::StatusOr<std::unique_ptr<UdfClient>> Create(
      google::scp::roma::Config<std::weak_ptr<RequestContext>>&& config =
          google::scp::roma::Config<std::weak_ptr<RequestContext>>(),
      absl::Duration udf_timeout = absl::Seconds(5),
      absl::Duration udf_update_timeout = absl::Seconds(30),
      int udf_min_log_level = 0);
};

}  // namespace kv_server

#endif  // COMPONENTS_UDF_UDF_CLIENT_H_
