/*
 * Copyright 2022 Google LLC
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

#ifndef TOOLS_DATA_CLI_CSV_CONSTANTS_H_
#define TOOLS_DATA_CLI_CSV_CONSTANTS_H_

#include <array>
#include <string_view>

namespace kv_server {

inline constexpr std::string_view kUpdateMutationType = "update";
inline constexpr std::string_view kDeleteMutationType = "delete";

inline constexpr std::string_view kMutationTypeColumn = "mutation_type";
inline constexpr std::string_view kLogicalCommitTimeColumn =
    "logical_commit_time";
inline constexpr std::string_view kKeyColumn = "key";
inline constexpr std::string_view kSubKeyColumn = "subkey";
inline constexpr std::string_view kValueColumn = "value";

}  //  namespace kv_server

#endif  // TOOLS_DATA_CLI_CSV_CONSTANTS_H_
