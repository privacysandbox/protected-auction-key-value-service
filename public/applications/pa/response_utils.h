/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PUBLIC_APPLICATIONS_PA_RESPONSE_UTILS_H_
#define PUBLIC_APPLICATIONS_PA_RESPONSE_UTILS_H_

#include <string>

#include "absl/status/statusor.h"
#include "public/applications/pa/api_overlay.pb.h"

namespace kv_server::application_pa {

absl::StatusOr<PartitionOutput> PartitionOutputFromJson(
    std::string_view json_str);

absl::StatusOr<std::string> PartitionOutputToJson(
    const PartitionOutput& key_group_outputs);

}  // namespace kv_server::application_pa

#endif  // PUBLIC_APPLICATIONS_PA_RESPONSE_UTILS_H_
