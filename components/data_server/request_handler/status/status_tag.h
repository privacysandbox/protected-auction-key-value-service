/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMPONENTS_DATA_SERVER_REQUEST_HANDLER_STATUS_STATUS_TAG_H_
#define COMPONENTS_DATA_SERVER_REQUEST_HANDLER_STATUS_STATUS_TAG_H_

#include <string_view>

#include "absl/status/status.h"
#include "absl/strings/cord.h"

namespace kv_server {

constexpr inline std::string_view kSafeToPropagate =
    "error_during_v2_request_parsing";

// Wrap a status as safe to propagate to return as an HTTP status code.
// This is only true for V2 handler errors that *purely* due to the
// request format being incorrect.
inline absl::Status V2RequestFormatErrorAsExternalHttpError(
    absl::Status status) {
  status.SetPayload(kSafeToPropagate, absl::Cord("true"));
  return status;
}

inline bool IsV2RequestFormatError(const absl::Status& status) {
  std::optional<absl::Cord> propagate = status.GetPayload(kSafeToPropagate);
  return propagate.has_value() && *propagate == "true";
}

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_REQUEST_HANDLER_STATUS_STATUS_TAG_H_
