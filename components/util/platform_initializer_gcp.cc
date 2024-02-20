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

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "components/util/platform_initializer.h"
#include "scp/cc/public/core/interface/errors.h"
#include "scp/cc/public/core/interface/execution_result.h"
#include "scp/cc/public/cpio/interface/cpio.h"

// This flag is added to allow for a local instance to use GCP as the cloud
// platform. Ideally, this would be fetched from the parameter_client, but the
// parameter client can't be used until a project is specified, chicken and egg.
// This flag is defined during build only when //:gcp_platform is specified.
ABSL_FLAG(std::string, gcp_project_id, "",
          "Overrides the GCP Project ID to run the parameter client."
          "Required when running on a local instance."
          "When not provided, CPIO finds the gcp_project_id by calling the "
          "Google Compute Engine metadata server on the GCP instance client.");

namespace kv_server {
namespace {
using google::scp::core::GetErrorMessage;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::LogOption;
google::scp::cpio::CpioOptions cpio_options_;
}  // namespace

PlatformInitializer::PlatformInitializer() {
  cpio_options_.log_option = LogOption::kConsoleLog;
  cpio_options_.project_id = absl::GetFlag(FLAGS_gcp_project_id);
  auto execution_result = Cpio::InitCpio(cpio_options_);
  CHECK(execution_result.Successful())
      << "Failed to initialize CPIO: "
      << GetErrorMessage(execution_result.status_code);
}

PlatformInitializer::~PlatformInitializer() {
  auto execution_result = Cpio::ShutdownCpio(cpio_options_);
  if (!execution_result.Successful()) {
    LOG(ERROR) << "Failed to shutdown CPIO: "
               << GetErrorMessage(execution_result.status_code);
  }
}
}  // namespace kv_server
