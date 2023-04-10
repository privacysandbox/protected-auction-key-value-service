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

#include "components/udf/code_fetcher.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cc/roma/interface/roma.h"
#include "components/internal_lookup/lookup.grpc.pb.h"
#include "components/internal_lookup/lookup_client.h"
#include "components/udf/code_config.h"
#include "glog/logging.h"
#include "public/udf/constants.h"

namespace kv_server {

constexpr char kId[] = "UDF";

class CodeFetcherImpl : public CodeFetcher {
 public:
  CodeFetcherImpl() = default;

  absl::StatusOr<CodeConfig> FetchCodeConfig(
      const LookupClient& lookup_client) {
    absl::StatusOr<InternalLookupResponse> lookup_response_or_status =
        lookup_client.GetValues({kUdfHandlerNameKey, kUdfCodeSnippetKey});
    if (!lookup_response_or_status.ok()) {
      return lookup_response_or_status.status();
    }

    auto lookup_response = std::move(lookup_response_or_status).value();
    const auto udf_handler_name_pair =
        lookup_response.kv_pairs().find(kUdfHandlerNameKey);
    if (udf_handler_name_pair == lookup_response.kv_pairs().end()) {
      return absl::NotFoundError("UDF handler name not found");
    } else if (udf_handler_name_pair->second.has_status()) {
      return absl::UnknownError(
          udf_handler_name_pair->second.status().message());
    } else if (udf_handler_name_pair->second.value().empty()) {
      return absl::NotFoundError("UDF handler name value empty");
    }

    const auto udf_code_snippet_pair =
        lookup_response.kv_pairs().find(kUdfCodeSnippetKey);
    if (udf_code_snippet_pair == lookup_response.kv_pairs().end()) {
      return absl::NotFoundError("UDF code snippet not found");
    } else if (udf_code_snippet_pair->second.has_status()) {
      return absl::UnknownError(
          udf_code_snippet_pair->second.status().message());
    } else if (udf_code_snippet_pair->second.value().empty()) {
      return absl::NotFoundError("UDF code snippet value empty");
    }

    return CodeConfig{
        .js = std::move(udf_code_snippet_pair->second.value()),
        .udf_handler_name = std::move(udf_handler_name_pair->second.value())};
  }
};

std::unique_ptr<CodeFetcher> CodeFetcher::Create() {
  return std::make_unique<CodeFetcherImpl>();
}
}  // namespace kv_server
