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

#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/flags.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "public/applications/pa/response_utils.h"
#include "public/query/cpp/grpc_client.h"

ABSL_FLAG(std::string, kv_endpoint, "<ip>:50051", "KV grpc endpoint");
ABSL_FLAG(int, inclusive_upper_bound, 999999999, "Inclusive upper bound");
ABSL_FLAG(int, qps, 5, "qps");
ABSL_FLAG(int, number_of_requests_to_make, 1, "Number of requests to make");
ABSL_FLAG(int, value_size, 10000, "Specify the size of value for the key");
ABSL_FLAG(int, batch_size, 10, "Batch size");
ABSL_FLAG(std::string, key_prefix, "foo", "Key prefix");
ABSL_FLAG(bool, use_tls, false, "Whether to use TLS for grpc calls.");

namespace kv_server {
namespace {
absl::BitGen bitgen;
int total_failures = 0;
int total_mismatches = 0;

int64_t Get(int64_t upper_bound) {
  return absl::Uniform(bitgen, 0, upper_bound);
}

v2::GetValuesRequest GetRequest(const std::vector<std::string>& input_values) {
  v2::GetValuesRequest req;
  v2::RequestPartition* partition = req.add_partitions();
  auto* udf_argument = partition->add_arguments();
  auto* values = udf_argument->mutable_data()->mutable_list_value();
  for (const auto& v : input_values) {
    values->add_values()->set_string_value(v);
  }
  udf_argument->mutable_tags()->add_values()->set_string_value("keys");
  udf_argument->mutable_tags()->add_values()->set_string_value("custom");
  return req;
}

absl::StatusOr<std::string> GetValueFromResponse(
    absl::StatusOr<v2::GetValuesResponse>& maybe_response,
    const std::string& key) {
  if (!maybe_response.ok()) {
    return maybe_response.status();
  }
  auto output = maybe_response->single_partition().string_output();
  auto maybe_proto = application_pa::KeyGroupOutputsFromJson(output);
  if (!maybe_proto.ok()) {
    return maybe_proto.status();
  }
  auto key_group_outputs = maybe_proto->key_group_outputs();
  if (key_group_outputs.empty()) {
    return absl::InvalidArgumentError("key_group_outputs empty");
  }
  auto kv = key_group_outputs[0].key_values();
  if (auto it = kv.find(key); it == kv.end()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Cannot find the key  %s.", key));
  } else {
    auto value = it->second;
    return value.value().string_value();
  }
}

std::vector<std::string> GetKeys(int start_index, int batch_size) {
  const std::string key_prefix = absl::GetFlag(FLAGS_key_prefix);
  std::vector<std::string> res;
  auto idx = start_index;
  while (idx < start_index + batch_size) {
    const std::string key = absl::StrCat(key_prefix, idx);
    LOG(INFO) << "Reading key: " << key;
    res.emplace_back(key);
    idx++;
  }
  return res;
}

void ValidateResponse(absl::StatusOr<v2::GetValuesResponse> maybe_response,
                      std::vector<std::string>& keys) {
  const int value_size = absl::GetFlag(FLAGS_value_size);
  for (const auto& key : keys) {
    auto maybe_response_value = GetValueFromResponse(maybe_response, key);
    if (!maybe_response_value.ok()) {
      total_failures++;
      LOG(ERROR) << maybe_response_value.status().message();
      continue;
    }
    auto response_value = *maybe_response_value;
    const std::string expected_value(value_size, key[key.size() - 1]);
    if (response_value != expected_value) {
      total_mismatches++;
      LOG(ERROR) << "Expected value: " << expected_value
                 << " Actual value: " << response_value;
    } else {
      LOG(INFO) << "matches";
    }
  }
}

void Validate() {
  const std::string kv_endpoint = absl::GetFlag(FLAGS_kv_endpoint);
  const int inclusive_upper_bound = absl::GetFlag(FLAGS_inclusive_upper_bound);
  const int batch_size = absl::GetFlag(FLAGS_batch_size);
  const int qps = absl::GetFlag(FLAGS_qps);
  const int number_of_requests_to_make =
      absl::GetFlag(FLAGS_number_of_requests_to_make);
  const bool use_tls = absl::GetFlag(FLAGS_use_tls);
  int requests_made_this_second = 0;
  int total_requests_made = 0;
  auto batch_end = absl::Now() + absl::Seconds(1);
  std::unique_ptr<v2::KeyValueService::Stub> stub;
  if (use_tls) {
    stub = GrpcClient::CreateStub(
        kv_endpoint, grpc::SslCredentials(grpc::SslCredentialsOptions()));
  } else {
    stub =
        GrpcClient::CreateStub(kv_endpoint, grpc::InsecureChannelCredentials());
  }
  GrpcClient client(*stub);
  while (total_requests_made < number_of_requests_to_make) {
    auto random_index = Get(inclusive_upper_bound / batch_size);
    random_index *= batch_size;
    std::vector<std::string> keys = GetKeys(random_index, batch_size);
    auto req = GetRequest(keys);
    ValidateResponse(client.GetValues(req), keys);
    requests_made_this_second++;
    // rate limit to N files per second
    if (requests_made_this_second % qps == 0) {
      if (batch_end > absl::Now()) {
        absl::SleepFor(batch_end - absl::Now());
        LOG(INFO) << ": sleeping ";
      }
      batch_end += absl::Seconds(1);
    }
    total_requests_made++;
  }

  LOG(INFO) << "Validated " << batch_size * total_requests_made
            << " key-value pairs \n";
}

}  // namespace
}  // namespace kv_server

// This tool will query the specified _kv_endpoint_ endpoint _qps_ number of
// times per second. The total amount of request is
// _number_of_requests_to_make_. Each request has _batch_size_ number of keys to
// lookup. The assumptions for these tests are following. The keys loaded to the
// kv server are of format _key_prefix_{0....inclusive_upper_bound} Each value
// is deterministiclly mapped from the key -- const std::string
// expected_value(value_size, key[key.size() - 1]); For each request a random
// key from the key space is selected. And the request look up that key and
// _batch_size_ of the sequential keys.
// Sample command:
// bazel run //components/tools/sharding_correctness_validator:validator --
// --qps=5 --number_of_requests_to_make=300 --batch_size=5
// --kv_endpoint=<your_ip>:50051

int main(int argc, char** argv) {
  const std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  kv_server::Validate();

  if (kv_server::total_failures > 0 || kv_server::total_mismatches > 0) {
    LOG(ERROR) << "Validation failed with total_failures: "
               << kv_server::total_failures
               << ", total_mismatches: " << kv_server::total_mismatches;
    return 1;
  }

  return 0;
}
