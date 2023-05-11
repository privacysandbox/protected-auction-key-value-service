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

#include "components/udf/get_values_hook_impl.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "components/internal_lookup/lookup.grpc.pb.h"
#include "components/udf/get_values_hook.h"
#include "glog/logging.h"
#include "google/protobuf/util/json_util.h"
#include "nlohmann/json.hpp"
#include "src/cpp/telemetry/telemetry.h"

namespace kv_server {
namespace {

constexpr char kGetValuesHookSpan[] = "GetValuesHook";
constexpr char kProcessClientResponseSpan[] = "ProcessLookupClientResponse";
constexpr char kProtoToJsonSpan[] = "ProtoToJson";

using google::protobuf::util::MessageToJsonString;
using privacy_sandbox::server_common::GetTracer;

class GetValuesHookImpl : public GetValuesHook {
 public:
  explicit GetValuesHookImpl(
      const absl::AnyInvocable<const LookupClient&() const>& get_lookup_client)
      : lookup_client_factory_(std::move(get_lookup_client)) {}

  std::string operator()(std::tuple<std::vector<std::string>>& input) {
    // TODO(b/261181061): Determine where to InitTracer.

    auto span = GetTracer()->StartSpan(kGetValuesHookSpan);
    auto scope = opentelemetry::trace::Scope(span);

    LOG(INFO) << "Calling internal lookup client";
    absl::StatusOr<InternalLookupResponse> response_or_status =
        lookup_client_factory_().GetValues(std::get<0>(input));

    auto process_response_span =
        GetTracer()->StartSpan(kProcessClientResponseSpan);
    auto process_response_scope =
        opentelemetry::trace::Scope(process_response_span);

    LOG(INFO) << "Processing internal lookup response";
    if (!response_or_status.ok()) {
      nlohmann::json status;
      status["code"] = response_or_status.status().code();
      status["message"] = response_or_status.status().message();
      return status.dump();
    }

    auto proto_to_json_span = GetTracer()->StartSpan(kProtoToJsonSpan);
    auto proto_to_json_scope = opentelemetry::trace::Scope(proto_to_json_span);

    std::string kv_pairs_json;
    MessageToJsonString(response_or_status.value(), &kv_pairs_json);

    return kv_pairs_json;
  }

 private:
  const absl::AnyInvocable<const LookupClient&() const>& lookup_client_factory_;
};
}  // namespace

std::unique_ptr<GetValuesHook> NewGetValuesHook(
    const absl::AnyInvocable<const LookupClient&() const>& get_lookup_client) {
  return std::make_unique<GetValuesHookImpl>(std::move(get_lookup_client));
}

}  // namespace kv_server
