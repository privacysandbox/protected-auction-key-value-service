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

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/substitute.h"
#include "aws/sns/SNSClient.h"
#include "aws/sns/model/PublishRequest.h"
#include "components/errors/error_util_aws.h"
#include "components/util/platform_initializer.h"
#include "public/data_loading/filename_utils.h"

ABSL_FLAG(std::string, sns_arn, "", "sns_arn");
ABSL_FLAG(std::string, deltas_folder_path, "",
          "Path to the folder with delta files");
ABSL_FLAG(int, insertion_num_threads, 2,
          "The amount of threads writing to SNS in parallel");

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

absl::Status Publish(Aws::SNS::SNSClient& sns, const std::string& sns_arn,
                     const std::string& body) {
  Aws::SNS::Model::PublishRequest req;
  req.SetTopicArn(sns_arn);
  req.SetMessage(body);
  Aws::SNS::Model::MessageAttributeValue messageAttributeValue;
  messageAttributeValue.SetDataType("String");
  std::string nanos_since_epoch =
      std::to_string(absl::ToUnixNanos(absl::Now()));
  messageAttributeValue.SetStringValue(nanos_since_epoch);
  req.AddMessageAttributes("time_sent", messageAttributeValue);
  auto outcome = sns.Publish(req);
  return outcome.IsSuccess() ? absl::OkStatus()
                             : kv_server::AwsErrorToStatus(outcome.GetError());
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

void ConsumeAndPublishToSns(const std::string& sns_arn, int thread_idx) {
  auto start = absl::Now();
  Aws::SNS::SNSClient sns_client;
  int delta_file_index = 1;
  // can't insert more than that many messages per second.
  int files_insertion_rate = 15;
  auto batch_end = absl::Now() + absl::Seconds(1);
  auto message = Pop();
  while (message.has_value()) {
    std::cout << absl::Now() << ": Inserting to the SNS: " << delta_file_index
              << " Thread idx " << thread_idx << std::endl;

    auto status = Publish(sns_client, sns_arn, *message);
    if (!status.ok()) {
      std::cerr << absl::Now() << status << std::endl;
    }
    delta_file_index++;
    // rate limit to N files per second
    if (delta_file_index % files_insertion_rate == 0) {
      if (batch_end > absl::Now()) {
        absl::SleepFor(batch_end - absl::Now());
        std::cout << absl::Now() << ": sleeping " << delta_file_index
                  << std::endl;
      }
      batch_end += absl::Seconds(1);
    }
    message = Pop();
  }

  int64_t elapsed_seconds = absl::ToInt64Seconds(absl::Now() - start);
  std::cout << absl::Now() << ": Total inserted: " << delta_file_index
            << " Seconds elapsed " << elapsed_seconds << " Actual rate "
            << (delta_file_index / elapsed_seconds) << " Thread idx "
            << thread_idx << std::endl;
}

// This tool will insert all delta files from `deltas_folder_path` into
// the specified SNS. It will insert 15 delta files per second from 2 threads.
// (The amount of threads can be updated through `insertion_num_threads`). 15 is
// amount of insertion you can do from a single thread per second that was
// empirically measured. Sample command: bazel run
// //components/tools:realtime_updates_publisher
// --deltas_folder_path='pathtoyourdeltas'
// --sns_arn='yourrealtimeSNSARN' To generate test delta files you can run
// `tools/serving_data_generator/generate_load_test_data`.
int main(int argc, char** argv) {
  kv_server::PlatformInitializer initializer;
  const std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  const std::string sns_arn = absl::GetFlag(FLAGS_sns_arn);
  if (sns_arn.empty()) {
    std::cerr << "Must specify sns_arn" << std::endl;
    return -1;
  }

  const std::string deltas_folder_path =
      absl::GetFlag(FLAGS_deltas_folder_path);
  if (deltas_folder_path.empty()) {
    std::cerr << "Must specify deltas_folder_path" << std::endl;
    return -1;
  }
  PopulateQueue(deltas_folder_path);
  const int insertion_num_threads = absl::GetFlag(FLAGS_insertion_num_threads);
  std::vector<std::unique_ptr<std::thread>> publishers;
  for (int i = 0; i < insertion_num_threads; i++) {
    publishers.emplace_back(std::make_unique<std::thread>(
        [&sns_arn, i] { ConsumeAndPublishToSns(sns_arn, i); }));
  }

  while (!updates_queue.empty()) {
    std::cout << "Waiting on consumers\n";
    std::this_thread::sleep_for(1000ms);
  }
  for (auto& publisher_thread : publishers) {
    publisher_thread->join();
  }
  return 0;
}
