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

#include "components/udf/hooks/run_query_hook.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "components/internal_server/lookup.h"
#include "nlohmann/json.hpp"

namespace kv_server {
namespace {

using google::scp::roma::FunctionBindingPayload;

class RunQueryHookImpl : public RunQueryHook {
 public:
  void FinishInit(std::unique_ptr<Lookup> lookup) {
    if (lookup_ == nullptr) {
      lookup_ = std::move(lookup);
    }
  }

  void operator()(FunctionBindingPayload<>& payload) {
    if (lookup_ == nullptr) {
      nlohmann::json status;
      status["code"] = absl::StatusCode::kInternal;
      status["message"] = "runQuery has not been initialized yet";
      payload.io_proto.mutable_output_list_of_string()->add_data(status.dump());
      LOG(ERROR)
          << "runQuery hook is not initialized properly: lookup is nullptr";
      return;
    }

    VLOG(9) << "runQuery request: " << payload.io_proto.DebugString();
    if (!payload.io_proto.has_input_string()) {
      nlohmann::json status;
      status["code"] = absl::StatusCode::kInvalidArgument;
      status["message"] = "runQuery input must be a string";
      payload.io_proto.mutable_output_list_of_string()->add_data(status.dump());
      VLOG(1) << "runQuery result: " << payload.io_proto.DebugString();
      return;
    }

    VLOG(9) << "Calling internal run query client";
    absl::StatusOr<InternalRunQueryResponse> response_or_status =
        lookup_->RunQuery(payload.io_proto.input_string());

    if (!response_or_status.ok()) {
      LOG(ERROR) << "Internal run query returned error: "
                 << response_or_status.status();
      payload.io_proto.mutable_output_list_of_string()->mutable_data();
      VLOG(1) << "runQuery result: " << payload.io_proto.DebugString();
      return;
    }

    VLOG(9) << "Processing internal run query response";
    *payload.io_proto.mutable_output_list_of_string()->mutable_data() =
        *std::move(response_or_status.value().mutable_elements());
    VLOG(9) << "runQuery result: " << payload.io_proto.DebugString();
  }

 private:
  // `lookup_` is initialized separately, since its dependencies create threads.
  // Lazy load is used to ensure that it only happens after Roma forks.
  std::unique_ptr<Lookup> lookup_;
};
}  // namespace

std::unique_ptr<RunQueryHook> RunQueryHook::Create() {
  return std::make_unique<RunQueryHookImpl>();
}

}  // namespace kv_server
