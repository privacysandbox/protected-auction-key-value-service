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

#include <filesystem>
#include <fstream>
#include <queue>
#include <thread>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/flags.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/substitute.h"
#include "components/data/common/msg_svc.h"
#include "components/tools/concurrent_publishing_engine.h"
#include "components/tools/publisher_service.h"
#include "components/tools/util/configure_telemetry_tools.h"
#include "components/util/platform_initializer.h"

ABSL_FLAG(std::string, deltas_folder_path, "",
          "Path to the folder with delta files");
ABSL_FLAG(int, insertion_num_threads, 1,
          "The amount of threads writing to SNS in parallel");

namespace kv_server {
namespace {

using RecursiveDirectoryIterator =
    std::filesystem::recursive_directory_iterator;
using namespace std::chrono_literals;  // NOLINT

absl::Mutex mutex;
std::queue<RealtimeMessage> updates_queue;

void PopulateQueue(const std::string& deltas_folder_path) {
  for (const auto& delta_file_name :
       RecursiveDirectoryIterator(deltas_folder_path)) {
    std::ifstream delta_file_stream(delta_file_name.path().string());
    std::stringstream buffer;
    buffer << delta_file_stream.rdbuf();
    RealtimeMessage rm;
    rm.message = absl::Base64Escape(buffer.str());
    updates_queue.push(std::move(rm));
  }
}

absl::Status Run() {
  PlatformInitializer initializer;
  kv_server::ConfigureTelemetryForTools();
  auto maybe_notifier_metadata = PublisherService::GetNotifierMetadata();
  if (!maybe_notifier_metadata.ok()) {
    return maybe_notifier_metadata.status();
  }
  const std::string deltas_folder_path =
      absl::GetFlag(FLAGS_deltas_folder_path);
  if (deltas_folder_path.empty()) {
    return absl::InvalidArgumentError("Must specify deltas_folder_path.");
  }
  PopulateQueue(deltas_folder_path);
  const int insertion_num_threads = absl::GetFlag(FLAGS_insertion_num_threads);
  int files_insertion_rate = 15;
  auto publishing_engine = ConcurrentPublishingEngine(
      insertion_num_threads, std::move(*maybe_notifier_metadata),
      files_insertion_rate, mutex, updates_queue);
  publishing_engine.Start();
  while (!updates_queue.empty()) {
    LOG(INFO) << "Waiting on consumers";
    std::this_thread::sleep_for(1000ms);
  }
  publishing_engine.Stop();
  return absl::OkStatus();
}

}  // namespace
}  // namespace kv_server

// This tool will insert all delta files from `deltas_folder_path` into
// the specified SNS. It will insert 15 delta files per second from 2 threads.
// (The amount of threads can be updated through `insertion_num_threads`). 15 is
// amount of insertion you can do from a single thread per second that was
// empirically measured. Sample AWS command: bazel run --config=aws_platform
// //components/tools:realtime_updates_publisher
// --deltas_folder_path='pathtoyourdeltas'
// --aws_sns_arn='yourrealtimeSNSARN' To generate test delta files you can run
// `tools/serving_data_generator/generate_load_test_data`.
int main(int argc, char** argv) {
  const std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  const absl::Status status = kv_server::Run();
  if (!status.ok()) {
    LOG(FATAL) << "Failed to run: " << status;
  }
  return 0;
}
