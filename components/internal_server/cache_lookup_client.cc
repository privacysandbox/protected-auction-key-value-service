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

#include "components/internal_server/cache_lookup_client.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "components/data_server/cache/cache.h"
#include "components/internal_server/lookup.pb.h"
#include "glog/logging.h"

namespace kv_server {
namespace {

class CacheLookupClient : public LookupClient {
 public:
  explicit CacheLookupClient(const Cache& cache) : cache_(cache) {}

  absl::StatusOr<InternalLookupResponse> GetValues(
      const std::vector<std::string>& keys) const override {
    InternalLookupResponse response;
    if (keys.empty()) {
      return response;
    }

    std::vector<std::string_view> key_list(keys.begin(), keys.end());
    auto kv_pairs = cache_.GetKeyValuePairs(key_list);
    for (const auto& key : key_list) {
      VLOG(9) << "Processing requested key: " << key;
      SingleLookupResult result;
      const auto key_iter = kv_pairs.find(key);
      if (key_iter == kv_pairs.end()) {
        auto status = result.mutable_status();
        status->set_code(static_cast<int>(absl::StatusCode::kNotFound));
      } else {
        result.set_value(std::move(key_iter->second));
      }
      (*response.mutable_kv_pairs())[std::string(key)] = std::move(result);
    }
    VLOG(9) << "Returning InternalLookupResponse: " << response.DebugString();
    return response;
  }

 private:
  const Cache& cache_;
};

}  // namespace

std::unique_ptr<LookupClient> CreateCacheLookupClient(const Cache& cache) {
  return std::make_unique<CacheLookupClient>(cache);
}

}  // namespace kv_server
