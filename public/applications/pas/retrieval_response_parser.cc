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

#include "public/applications/pas/retrieval_response_parser.h"

#include "absl/strings/str_cat.h"

namespace kv_server::application_pas {

using v2::ResponsePartition;

absl::StatusOr<std::string> GetRetrievalOutput(
    const v2::GetValuesResponse& response) {
  switch (response.single_partition().output_type_case()) {
    case ResponsePartition::kStringOutput: {
      return response.single_partition().string_output();
    }
    case ResponsePartition::kStatus: {
      return absl::Status(static_cast<absl::StatusCode>(
                              response.single_partition().status().code()),
                          response.single_partition().status().message());
    }
    default:
      return absl::UnimplementedError(
          absl::StrCat("output type unimplemented: ",
                       response.single_partition().output_type_case()));
  }
}

}  // namespace kv_server::application_pas
