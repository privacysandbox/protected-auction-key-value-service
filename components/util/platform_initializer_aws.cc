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

#include <cstdlib>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "aws/core/Aws.h"
#include "components/util/platform_initializer.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/cpio.h"

namespace kv_server {
using google::scp::core::errors::GetErrorMessage;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::LogOption;

namespace {
Aws::SDKOptions options_;
google::scp::cpio::CpioOptions cpio_options_;

}  // namespace

PlatformInitializer::PlatformInitializer() {
  // Handle curl SIGPIPEs:
  // https://sdk.amazonaws.com/cpp/api/LATEST/aws-cpp-sdk-core/html/struct_aws_1_1_http_options.html#ac56ca429444ca99aeb2dce6aec35f017
  options_.httpOptions.installSigPipeHandler = true;
  Aws::InitAPI(options_);
  cpio_options_.log_option = LogOption::kConsoleLog;
  cpio_options_.cloud_init_option =
      google::scp::cpio::CloudInitOption::kNoInitInCpio;

// TODO(b/338206801): Remove this aws region logic for aws local instance once
// it's fixed on the CPIO side.
#if defined(INSTANCE_LOCAL)
  if (std::string aws_region = std::getenv("AWS_DEFAULT_REGION");
      aws_region.empty()) {
    LOG(WARNING) << "Failed to get environment variable 'AWS_DEFAULT_REGION' "
                    "for PlatformInitializer.";
  } else {
    cpio_options_.region = aws_region;
  }
#endif
  if (auto error = Cpio::InitCpio(cpio_options_); !error.Successful()) {
    LOG(ERROR) << "Failed to initialize CPIO: "
               << GetErrorMessage(error.status_code) << std::endl;
  }
}

PlatformInitializer::~PlatformInitializer() {
  if (auto error = Cpio::ShutdownCpio(cpio_options_); !error.Successful()) {
    LOG(ERROR) << "Failed to shutdown CPIO: "
               << GetErrorMessage(error.status_code) << std::endl;
  }
  Aws::ShutdownAPI(options_);
}

}  // namespace kv_server
