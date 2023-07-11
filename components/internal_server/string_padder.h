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

#ifndef COMPONENTS_INTERNAL_SERVER_STRING_PADDER_H_
#define COMPONENTS_INTERNAL_SERVER_STRING_PADDER_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace kv_server {
// Returns the string of the following format:
// [32 bit unsigned int][string_to_pad][padding]
//  length               data           filler
// filler.size() == extra_padding
std::string Pad(std::string_view string_to_pad, int32_t extra_padding);
// Takes the string padded with the method above OR in the same format
// and returns the string.
absl::StatusOr<std::string> Unpad(std::string_view padded_string);
}  // namespace kv_server

#endif  // COMPONENTS_INTERNAL_SERVER_STRING_PADDER_H_
