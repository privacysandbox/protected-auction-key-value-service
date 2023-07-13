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

#include "components/udf/run_query_hook.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "components/internal_server/run_query_client.h"
#include "glog/logging.h"

namespace kv_server {
namespace {

class RunQueryHookImpl : public RunQueryHook {
 public:
  explicit RunQueryHookImpl(
      absl::AnyInvocable<std::unique_ptr<RunQueryClient>()>
          query_client_supplier)
      : query_client_supplier_(std::move(query_client_supplier)) {}

  // TODO(b/283091615): Add tests.
  std::vector<std::string> operator()(std::tuple<std::string>& input) {
    if (query_client_ == nullptr) {
      query_client_ = query_client_supplier_();
    }
    // TODO(b/261181061): Determine where to InitTracer.
    VLOG(9) << "Calling internal run query client";
    absl::StatusOr<InternalRunQueryResponse> response_or_status =
        query_client_->RunQuery(std::get<0>(input));

    VLOG(9) << "Processing internal run query response";
    if (!response_or_status.ok()) {
      LOG(ERROR) << "Internal run query returned error: "
                 << response_or_status.status();
      return std::vector<std::string>();
    }
    std::vector<std::string> result;
    for (auto&& element :
         *std::move(response_or_status).value().mutable_elements()) {
      result.push_back(std::move(element));
    }
    return result;
  }

 private:
  // `query_client_` is lazy loaded because getting one can cause thread
  // creation. Lazy load is used to ensure that it only happens after Roma
  // forks.
  absl::AnyInvocable<std::unique_ptr<RunQueryClient>()> query_client_supplier_;
  std::unique_ptr<RunQueryClient> query_client_;
};
}  // namespace

std::unique_ptr<RunQueryHook> RunQueryHook::Create(
    absl::AnyInvocable<std::unique_ptr<RunQueryClient>()>
        query_client_supplier) {
  return std::make_unique<RunQueryHookImpl>(std::move(query_client_supplier));
}

}  // namespace kv_server
