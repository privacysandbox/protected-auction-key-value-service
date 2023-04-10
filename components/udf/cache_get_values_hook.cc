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

#include "components/udf/cache_get_values_hook.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/strings/str_join.h"
#include "components/data_server/cache/cache.h"
#include "components/internal_lookup/lookup.pb.h"
#include "components/udf/get_values_hook.h"
#include "glog/logging.h"
#include "google/protobuf/util/json_util.h"
#include "nlohmann/json.hpp"

namespace kv_server {
namespace {

using google::protobuf::util::MessageToJsonString;

// GetValues hook implementation that directly has access to the cache.
// Use for testing purposes only.
class CacheGetValuesHookImpl : public GetValuesHook {
 public:
  explicit CacheGetValuesHookImpl(const Cache& cache) : cache_(cache) {}

  std::string operator()(std::tuple<std::vector<std::string>>& input) {
    std::vector<std::string> keys = std::get<0>(input);
    std::vector<std::string_view> keys2(keys.begin(), keys.end());
    auto kv_pairs = cache_.GetKeyValuePairs(keys2);

    // TODO(b/270549052): Add kv pairs with errors

    // TODO(b/263858988): Change cache API to return map and set status for
    // missing keys.
    InternalLookupResponse response;
    for (auto&& [k, v] : std::move(kv_pairs)) {
      VLOG(9) << "Processing kv pair:" << k << ", " << v;
      SingleLookupResult result;
      result.set_value(std::move(v));
      (*response.mutable_kv_pairs())[k] = std::move(result);
    }

    std::string kv_pairs_json;
    MessageToJsonString(response, &kv_pairs_json);

    VLOG(5) << "Get values hook response: " << kv_pairs_json;
    return kv_pairs_json;
  }

 private:
  const Cache& cache_;
};

}  // namespace

std::unique_ptr<GetValuesHook> NewCacheGetValuesHook(const Cache& cache) {
  return std::make_unique<CacheGetValuesHookImpl>(cache);
}

}  // namespace kv_server
