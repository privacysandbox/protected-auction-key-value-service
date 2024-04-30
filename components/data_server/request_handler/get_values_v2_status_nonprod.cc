// Copyright 2024 Google LLC
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

#include "absl/flags/flag.h"
#include "src/util/status_macro/status_macros.h"

ABSL_FLAG(bool, propagate_v2_error_status, false,
          "Whether to propagate an error status to V2. This flag is only "
          "available in nonprod mode.");

namespace kv_server {

using privacy_sandbox::server_common::FromAbslStatus;

grpc::Status GetExternalStatusForV2(const absl::Status& status) {
  if (absl::GetFlag(FLAGS_propagate_v2_error_status)) {
    return FromAbslStatus(status);
  }
  return grpc::Status::OK;
}

}  // namespace kv_server
