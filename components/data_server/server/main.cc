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

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/flags.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "components/data_server/server/server.h"
#include "components/data_server/server/server_log_init.h"
#include "components/util/build_info.h"
#include "src/util/rlimit_core_config.h"

ABSL_FLAG(bool, buildinfo, false, "Print build info.");

int main(int argc, char** argv) {
  // The first thing we do is make sure that crashes will have a stacktrace
  // printed, with demangled symbols.  This is safe for several reasons:
  // 1. The stacktrace is based on the program counter and does not contain
  //    arguments to any of the functions - no user data is present in function
  //    names.
  // 2. Stacktraces of UDF failures are not printed here, this is only K/V
  //    Server framework code, which is all Open Source.
  // 3. Production versions of the K/V Server run inside Trusted Execution
  //    Environments, which restrict where STDOUT and STDERR are visible to.
  absl::InitializeSymbolizer(argv[0]);
  privacysandbox::server_common::SetRLimits({
      .enable_core_dumps = true,
  });
  {
    absl::FailureSignalHandlerOptions options;
    absl::InstallFailureSignalHandler(options);
  }

  kv_server::InitLog();
  absl::SetProgramUsageMessage(absl::StrCat(
      "FLEDGE Key/Value Server.  Sample usage:\n", argv[0], " --port=50051"));
  absl::ParseCommandLine(argc, argv);

  kv_server::LogBuildInfo();
  if (absl::GetFlag(FLAGS_buildinfo)) {
    return 0;
  }

  kv_server::Server server;
  if (const absl::Status status = server.Init(); status != absl::OkStatus()) {
    LOG(FATAL) << "Failed to run server: " << status;
  }
  server.Wait();
  server.GracefulShutdown(absl::Seconds(1));
  server.ForceShutdown();
  return 0;
}
