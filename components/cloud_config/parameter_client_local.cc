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
#include <memory>
#include <string>
#include <thread>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/flags/marshalling.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "components/cloud_config/parameter_client.h"

ABSL_FLAG(std::string, delta_directory, "",
          "Local directory to watch for files.");
ABSL_FLAG(absl::Duration, export_interval, absl::Seconds(30),
          "Frequency to export local metrics.");
ABSL_FLAG(absl::Duration, export_timeout, absl::Seconds(5),
          "Timeout for exporting local metrics.");
ABSL_FLAG(std::string, launch_hook, "", "Launch hook.");
ABSL_FLAG(absl::Duration, local_poll_frequency, absl::Seconds(5),
          "Frequency to poll for local file changes.");
ABSL_FLAG(std::string, realtime_directory, "",
          "Local directory to watch for realtime file changes.");
ABSL_FLAG(int32_t, local_realtime_updater_num_threads, 1,
          "Amount of realtime updates threads locally.");
ABSL_FLAG(int32_t, data_loading_num_threads,
          std::thread::hardware_concurrency(),
          "Number of parallel threads for reading and loading data files.");
ABSL_FLAG(int32_t, s3client_max_connections, 1,
          "S3Client max connections for reading data files.");
ABSL_FLAG(int32_t, s3client_max_range_bytes, 1,
          "S3Client max range bytes for reading data files.");
ABSL_FLAG(int32_t, num_shards, 1, "Total number of shards.");
ABSL_FLAG(int32_t, udf_num_workers, 2, "Number of workers for UDF execution.");
ABSL_FLAG(bool, route_v1_to_v2, false,
          "Whether to route V1 requests through V2.");

namespace kv_server {
namespace {

// Initialize a static map of flag values and use those to look up parameters.
//
// TODO(b/237669491) The values here are hardcoded with a server prefix of
//  "kv-server" and an environment of "local".  This is safe because these
// parameters should only work in that configuration but it's not elegant, see
// if there's a better way.
class LocalParameterClient : public ParameterClient {
 public:
  LocalParameterClient() {
    string_flag_values_.insert(
        {"kv-server-local-directory", absl::GetFlag(FLAGS_delta_directory)});
    string_flag_values_.insert({"kv-server-local-data-bucket-id",
                                absl::GetFlag(FLAGS_delta_directory)});
    string_flag_values_.insert(
        {"kv-server-local-launch-hook", absl::GetFlag(FLAGS_launch_hook)});
    string_flag_values_.insert({"kv-server-local-realtime-directory",
                                absl::GetFlag(FLAGS_realtime_directory)});
    // Insert more string flag values here.

    int32_t_flag_values_.insert(
        {"kv-server-local-metrics-export-interval-millis",
         absl::ToInt64Milliseconds(absl::GetFlag(FLAGS_export_interval))});
    int32_t_flag_values_.insert(
        {"kv-server-local-metrics-export-timeout-millis",
         absl::ToInt64Milliseconds(absl::GetFlag(FLAGS_export_timeout))});
    int32_t_flag_values_.insert(
        {"kv-server-local-backup-poll-frequency-secs",
         absl::ToInt64Seconds(absl::GetFlag(FLAGS_local_poll_frequency))});
    int32_t_flag_values_.insert(
        {"kv-server-local-realtime-updater-num-threads",
         absl::GetFlag(FLAGS_local_realtime_updater_num_threads)});
    int32_t_flag_values_.insert(
        {"kv-server-local-data-loading-num-threads",
         absl::GetFlag(FLAGS_data_loading_num_threads)});
    int32_t_flag_values_.insert(
        {"kv-server-local-s3client-max-connections",
         absl::GetFlag(FLAGS_s3client_max_connections)});
    int32_t_flag_values_.insert(
        {"kv-server-local-s3client-max-range-bytes",
         absl::GetFlag(FLAGS_s3client_max_range_bytes)});
    int32_t_flag_values_.insert(
        {"kv-server-local-num-shards", absl::GetFlag(FLAGS_num_shards)});
    int32_t_flag_values_.insert({"kv-server-local-udf-num-workers",
                                 absl::GetFlag(FLAGS_udf_num_workers)});
    // Insert more int32 flag values here.

    bool_flag_values_.insert({"kv-server-local-route-v1-to-v2",
                              absl::GetFlag(FLAGS_route_v1_to_v2)});
    // Insert more bool flag values here.
  }

  absl::StatusOr<std::string> GetParameter(
      std::string_view parameter_name) const override {
    const auto& it = string_flag_values_.find(parameter_name);
    if (it != string_flag_values_.end()) {
      return it->second;
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown local string parameter: ", parameter_name));
    }
  };

  absl::StatusOr<int32_t> GetInt32Parameter(
      std::string_view parameter_name) const override {
    const auto& it = int32_t_flag_values_.find(parameter_name);
    if (it != int32_t_flag_values_.end()) {
      return it->second;
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown local int32_t parameter: ", parameter_name));
    }
  }

  absl::StatusOr<bool> GetBoolParameter(
      std::string_view parameter_name) const override {
    const auto& it = bool_flag_values_.find(parameter_name);
    if (it != bool_flag_values_.end()) {
      return it->second;
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown local bool parameter: ", parameter_name));
    }
  }

 private:
  absl::flat_hash_map<std::string, int32_t> int32_t_flag_values_;
  absl::flat_hash_map<std::string, std::string> string_flag_values_;
  absl::flat_hash_map<std::string, bool> bool_flag_values_;
};

}  // namespace

std::unique_ptr<ParameterClient> ParameterClient::Create(
    ParameterClient::ClientOptions client_options) {
  return std::make_unique<LocalParameterClient>();
}

}  // namespace kv_server
