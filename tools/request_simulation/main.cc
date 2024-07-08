// Copyright 2023 Google LLC
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

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/flags.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "src/telemetry/telemetry_provider.h"
#include "tools/request_simulation/grpc_client.h"
#include "tools/request_simulation/request_simulation_system.h"

using privacy_sandbox::server_common::TelemetryProvider;

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);

  absl::InitializeSymbolizer(argv[0]);
  {
    absl::FailureSignalHandlerOptions options;
    absl::InstallFailureSignalHandler(options);
  }
  absl::InitializeLog();
  absl::SetProgramUsageMessage(absl::StrCat(
      "Key Value Server Request Simulation System.  Sample usage:\n", argv[0]));
  kv_server::RequestSimulationSystem::InitializeTelemetry();
  kv_server::RequestSimulationSystem system(
      privacy_sandbox::server_common::SteadyClock::RealClock(),
      [](const std::string& server_address,
         const kv_server::GrpcAuthenticationMode& mode) {
        return kv_server::CreateGrpcChannel(server_address, mode);
      });
  if (const absl::Status status = system.InitAndStart();
      status != absl::OkStatus()) {
    LOG(FATAL) << "Failed to run system: " << status;
  }
  while (system.IsRunning()) {
    // Noop
  }
  LOG(INFO) << "The request simulation system stops running!!!";
  return 0;
}
