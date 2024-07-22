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

#include <sys/wait.h>
#include <unistd.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "components/cloud_config/instance_client.h"
#include "components/cloud_config/parameter_client.h"
#include "components/data_server/server/parameter_fetcher.h"
#include "components/errors/retry.h"
#include "components/telemetry/server_definition.h"
#include "components/util/platform_initializer.h"

ABSL_DECLARE_FLAG(std::string, gcp_project_id);

#if defined(INSTANCE_LOCAL)
ABSL_DECLARE_FLAG(std::string, environment);
#else
ABSL_FLAG(std::string, environment, "NOT_SPECIFIED", "Environment name.");
#endif
// The environment flag is defined for local instance (in instance_client_local)
// but not for GCP instance, hence the difference here.

bool PrepareTlsKeyCertForEnvoy() {
  // Initializes GCP platform and its parameter client.
  kv_server::PlatformInitializer platform_initializer;
  std::unique_ptr<kv_server::ParameterClient> parameter_client =
      kv_server::ParameterClient::Create();

  // Gets environment name if not specified
  std::string environment = absl::GetFlag(FLAGS_environment);
  if (environment == "NOT_SPECIFIED") {
    std::unique_ptr<kv_server::InstanceClient> instance_client =
        kv_server::InstanceClient::Create();
    environment = kv_server::TraceRetryUntilOk(
        [&instance_client]() { return instance_client->GetEnvironmentTag(); },
        "GetEnvironment", kv_server::LogMetricsNoOpCallback());
  }

  kv_server::ParameterFetcher parameter_fetcher(environment, *parameter_client);
  if (bool enable_external_traffic =
          parameter_fetcher.GetBoolParameter("enable-external-traffic");
      !enable_external_traffic) {
    return false;  // Envoy is not needed if we don't serve external traffic.
  }

  // Prepares TLS cert and key for the connection between XLB and envoy. Per
  // GCP's protocol, the certificate here is not verified and it's ok to use a
  // self-signed cert.
  // (https://cloud.google.com/load-balancing/docs/ssl-certificates/encryption-to-the-backends#secure-protocol-considerations)
  std::string tls_key_str = parameter_fetcher.GetParameter("tls-key");
  std::string tls_cert_str = parameter_fetcher.GetParameter("tls-cert");
  if (tls_key_str == "NOT_PROVIDED" || tls_cert_str == "NOT_PROVIDED") {
    LOG(ERROR) << "TLS key/cert are not provided!";
    exit(1);
  }
  std::ofstream key_file("/etc/envoy/key.pem");
  std::ofstream cert_file("/etc/envoy/cert.pem");
  key_file << tls_key_str;
  cert_file << tls_cert_str;
  key_file.close();
  cert_file.close();
  return true;
}

void StartKvServer(int argc, char* argv[]) {
  std::vector<char*> server_exec_args = {"/server"};
  for (int i = 1; i < argc; ++i) {
    server_exec_args.push_back(argv[i]);
  }
  server_exec_args.push_back(nullptr);
  LOG(INFO) << "Starting KV-Server";
  execv(server_exec_args[0], &server_exec_args[0]);
  LOG(ERROR) << "Server failure:" << std::strerror(errno);
}

void StartEnvoy() {
  LOG(INFO) << "Starting Envoy";
  std::vector<char*> envoy_exec_args = {"/usr/local/bin/envoy", "--config-path",
                                        "/etc/envoy/envoy.yaml", "-l", "warn"};
  envoy_exec_args.push_back(nullptr);
  execv(envoy_exec_args[0], &envoy_exec_args[0]);
  LOG(ERROR) << "Envoy failure:" << std::strerror(errno);
}

int main(int argc, char* argv[]) {
  std::vector<char*> positional_args;
  std::vector<absl::UnrecognizedFlag> unrecognized_flags;
  // This function is required to ignore unrecognized flags without throwing out
  // errors. The unrecognized flags will later be used by "/server".
  absl::ParseAbseilFlagsOnly(argc, argv, positional_args, unrecognized_flags);

  if (PrepareTlsKeyCertForEnvoy()) {
    // Starts Envoy and server in separate processes
    if (const pid_t pid = fork(); pid == -1) {
      PLOG(ERROR) << "Fork failure!";
      return errno;
    } else if (pid == 0) {
      StartEnvoy();
    } else {
      sleep(5);
      StartKvServer(argc, argv);
    }
  } else {
    // Only starts server if envoy is not needed.
    StartKvServer(argc, argv);
  }
  return errno;
}
