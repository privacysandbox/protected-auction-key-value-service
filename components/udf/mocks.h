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

#ifndef COMPONENTS_UDF_MOCKS_H_
#define COMPONENTS_UDF_MOCKS_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "components/udf/code_config.h"
#include "components/udf/udf_client.h"
#include "gmock/gmock.h"
#include "src/roma/interface/roma.h"

namespace kv_server {

class MockUdfClient : public UdfClient {
 public:
  MOCK_METHOD((absl::StatusOr<std::string>), ExecuteCode,
              (const RequestContextFactory&, std::vector<std::string>,
               ExecutionMetadata& execution_metadata),
              (const, override));
  MOCK_METHOD((absl::StatusOr<std::string>), ExecuteCode,
              (const RequestContextFactory&, UDFExecutionMetadata&&,
               const google::protobuf::RepeatedPtrField<UDFArgument>&,
               ExecutionMetadata& execution_metadata),
              (const, override));
  MOCK_METHOD((absl::Status), Stop, (), (override));
  MOCK_METHOD((absl::Status), SetCodeObject,
              (CodeConfig, privacy_sandbox::server_common::log::PSLogContext&),
              (override));
  MOCK_METHOD((absl::Status), SetWasmCodeObject,
              (CodeConfig, privacy_sandbox::server_common::log::PSLogContext&),
              (override));
};

}  // namespace kv_server

#endif  // COMPONENTS_UDF_MOCKS_H_
