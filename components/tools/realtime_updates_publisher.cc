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
#include "absl/strings/substitute.h"
#include "components/data/common/msg_svc.h"
#include "components/tools/publisher_service.h"
#include "components/util/platform_initializer.h"
#include "glog/logging.h"

ABSL_FLAG(std::string, deltas_folder_path, "",
          "Path to the folder with delta files");
ABSL_FLAG(int, insertion_num_threads, 1,
          "The amount of threads writing to SNS in parallel");

namespace kv_server {
namespace {

using RecursiveDirectoryIterator =
    std::filesystem::recursive_directory_iterator;
using namespace std::chrono_literals;  // NOLINT

absl::Mutex mutex_;
std::queue<std::string> updates_queue;

void PopulateQueue(const std::string& deltas_folder_path) {
  for (const auto& delta_file_name :
       RecursiveDirectoryIterator(deltas_folder_path)) {
    std::ifstream delta_file_stream(delta_file_name.path().string());
    std::stringstream buffer;
    buffer << delta_file_stream.rdbuf();
    std::string file_encoded = absl::Base64Escape(buffer.str());
    updates_queue.push(file_encoded);
  }
}

std::optional<std::string> Pop() {
  absl::MutexLock lock(&mutex_);
  {
    if (updates_queue.empty()) {
      return std::nullopt;
    }
    auto file_encoded = updates_queue.front();
    updates_queue.pop();
    return file_encoded;
  }
}

void ConsumeAndPublish(NotifierMetadata notifier_metadata, int thread_idx) {
  auto start = absl::Now();
  int delta_file_index = 1;
  // can't insert more than that many messages per second.
  int files_insertion_rate = 15;
  auto batch_end = absl::Now() + absl::Seconds(1);
  auto message = Pop();
  auto maybe_msg_service = PublisherService::Create(notifier_metadata);
  if (!maybe_msg_service.ok()) {
    LOG(ERROR) << "Failed creating a publisher service";
    return;
  }
  auto msg_service = std::move(*maybe_msg_service);
  while (message.has_value()) {
    LOG(INFO) << absl::Now() << ": Inserting to the SNS: " << delta_file_index
              << " Thread idx " << thread_idx;
    auto status = msg_service->Publish(*message);
    if (!status.ok()) {
      LOG(ERROR) << absl::Now() << status << std::endl;
    }
    delta_file_index++;
    // rate limit to N files per second
    if (delta_file_index % files_insertion_rate == 0) {
      if (batch_end > absl::Now()) {
        absl::SleepFor(batch_end - absl::Now());
        LOG(INFO) << absl::Now() << ": sleeping " << delta_file_index;
      }
      batch_end += absl::Seconds(1);
    }
    message = Pop();
  }

  int64_t elapsed_seconds = absl::ToInt64Seconds(absl::Now() - start);
  LOG(INFO) << absl::Now() << ": Total inserted: " << delta_file_index
            << " Seconds elapsed " << elapsed_seconds << " Thread idx "
            << thread_idx;
  if (elapsed_seconds > 0) {
    LOG(INFO) << absl::Now() << " Actual rate "
              << (delta_file_index / elapsed_seconds);
  }
}

absl::Status Run() {
  PlatformInitializer initializer;
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
  std::vector<std::unique_ptr<std::thread>> publishers;
  for (int i = 0; i < insertion_num_threads; i++) {
    publishers.emplace_back(
        std::make_unique<std::thread>([maybe_notifier_metadata, i] {
          ConsumeAndPublish(*maybe_notifier_metadata, i);
        }));
  }

  while (!updates_queue.empty()) {
    LOG(INFO) << "Waiting on consumers";
    std::this_thread::sleep_for(1000ms);
  }
  for (auto& publisher_thread : publishers) {
    publisher_thread->join();
  }

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
  google::InitGoogleLogging(argv[0]);
  const absl::Status status = kv_server::Run();
  if (!status.ok()) {
    LOG(FATAL) << "Failed to run: " << status;
  }
  return 0;
}
