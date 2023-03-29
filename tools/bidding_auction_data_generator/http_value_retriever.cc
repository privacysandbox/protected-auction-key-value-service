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

#include "tools/bidding_auction_data_generator/http_value_retriever.h"

#include <algorithm>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "glog/logging.h"
#include "google/protobuf/util/json_util.h"
#include "public/query/get_values.pb.h"
#include "tools/bidding_auction_data_generator/value_fetch_util.h"

namespace kv_server {
namespace {
constexpr int64_t kTimeoutInMs = 5000;
using v1::GetValuesResponse;
}  // namespace

std::vector<std::string> HttpValueRetriever::GetBatchedUrls(
    const absl::flat_hash_set<std::string>& keys,
    const std::string& key_namespace, const std::string& base_url,
    bool need_encode, int num_keys_in_batch) {
  // break the keys into different batches and construct urls
  std::vector<std::string> urls;
  std::vector<std::string> keys_in_the_batch;
  for (const auto& k : keys) {
    if (keys_in_the_batch.size() == num_keys_in_batch) {
      std::string batch_url = kv_server::AppendQueryParametersToUrl(
          base_url, key_namespace, keys_in_the_batch, need_encode);
      urls.emplace_back(std::move(batch_url));
      keys_in_the_batch.clear();
    }
    keys_in_the_batch.emplace_back(k);
  }
  if (!keys_in_the_batch.empty()) {
    std::string batch_url = kv_server::AppendQueryParametersToUrl(
        base_url, key_namespace, keys_in_the_batch, need_encode);
    urls.emplace_back(std::move(batch_url));
  }
  LOG(INFO) << "Break " << keys.size() << " keys into " << urls.size()
            << " batch urls";
  return urls;
}

absl::StatusOr<std::vector<absl::flat_hash_map<std::string, std::string>>>
HttpValueRetriever::RetrieveValues(
    const absl::flat_hash_set<std::string>& keys, const std::string& base_url,
    const std::string& key_namespace,
    std::function<
        void(GetValuesResponse& response,
             absl::flat_hash_map<std::string, std::string>& key_value_map)>
        callback,
    bool need_encode, int num_keys_per_batch) {
  const std::vector<std::string> urls = GetBatchedUrls(
      keys, key_namespace, base_url, need_encode, num_keys_per_batch);
  std::vector<absl::flat_hash_map<std::string, std::string>> output(
      urls.size());

  const absl::StatusOr<std::vector<std::string>> responses =
      http_url_fetch_client_.FetchUrls(urls, kTimeoutInMs);
  if (!responses.ok()) {
    return responses.status();
  }

  LOG(INFO) << "Retrieved values from " << urls.size() << " urls";

  // parse responses to get key value pairs in parallel with max threads allowed
  google::protobuf::util::JsonParseOptions json_parse_options;
  json_parse_options.ignore_unknown_fields = true;

  int max_thread_count = (int)std::thread::hardware_concurrency();
  int num_responses_processed = 0;
  while (num_responses_processed < responses.value().size()) {
    int num_tasks_left = responses.value().size() - num_responses_processed;
    int num_of_tasks_in_parallel = std::min(max_thread_count, num_tasks_left);
    std::vector<std::thread> threads;
    for (int i = 0; i < num_of_tasks_in_parallel; i++) {
      int offset = i + num_responses_processed;
      const std::string& json_res =
          responses.value()[i + num_responses_processed];
      threads.push_back(std::move(std::thread([json_res, callback, offset,
                                               &json_parse_options, &output]() {
        GetValuesResponse response;
        absl::flat_hash_map<std::string, std::string> key_value_map;
        google::protobuf::util::Status status =
            google::protobuf::util::JsonStringToMessage(json_res, &response,
                                                        json_parse_options);
        if (!status.ok()) {
          LOG(ERROR) << "Unable to convert json response to GetValueResponse "
                     << status.ToString();
        } else {
          callback(response, output[offset]);
        }
      })));
    }
    for (std::thread& thread : threads) {
      thread.join();
    }
    num_responses_processed += num_of_tasks_in_parallel;
  }
  return output;
}

std::unique_ptr<HttpValueRetriever> HttpValueRetriever::Create(
    HttpUrlFetchClient& http_url_fetch_client) {
  return std::make_unique<HttpValueRetriever>(http_url_fetch_client);
}

}  // namespace kv_server
