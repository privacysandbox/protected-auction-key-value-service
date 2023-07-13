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
#include "components/internal_server/lookup.pb.h"
#include "components/udf/get_values_hook.h"
#include "glog/logging.h"
#include "google/protobuf/util/json_util.h"
#include "nlohmann/json.hpp"

namespace kv_server {
namespace {

using google::protobuf::util::MessageToJsonString;
using google::scp::roma::proto::FunctionBindingIoProto;

// GetValues hook implementation that directly has access to the cache.
// Use for testing purposes only.
class CacheGetValuesHookImpl : public GetValuesHook {
 public:
  explicit CacheGetValuesHookImpl(const Cache& cache) : cache_(cache) {}

  void operator()(FunctionBindingIoProto& io) {
    std::vector<std::string_view> keys;
    for (const auto& key : io.input_list_of_string().data()) {
      keys.emplace_back(key);
    }

    auto kv_pairs = cache_.GetKeyValuePairs(keys);

    // TODO(b/270549052): Add kv pairs with errors
    InternalLookupResponse response;
    for (auto&& [k, v] : std::move(kv_pairs)) {
      VLOG(9) << "Processing kv pair:" << k << ", " << v;
      SingleLookupResult result;
      result.set_value(std::move(v));
      (*response.mutable_kv_pairs())[k] = std::move(result);
    }

    std::string kv_pairs_json;
    if (const auto json_status = MessageToJsonString(response, &kv_pairs_json);
        !json_status.ok()) {
      nlohmann::json status;
      status["code"] = json_status.code();
      status["message"] = json_status.message();
      io.set_output_string(status.dump());
      LOG(ERROR) << "MessageToJsonString failed with " << json_status;
      return;
    }

    VLOG(5) << "Get values hook response: " << kv_pairs_json;
    io.set_output_string(kv_pairs_json);
  }

 private:
  const Cache& cache_;
};

}  // namespace

std::unique_ptr<GetValuesHook> NewCacheGetValuesHook(const Cache& cache) {
  return std::make_unique<CacheGetValuesHookImpl>(cache);
}

}  // namespace kv_server
