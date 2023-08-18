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

#include "components/data/common/msg_svc_util.h"

#include <optional>

#include "absl/random/random.h"

namespace kv_server {

constexpr uint32_t kQueueNameLen = 80;
constexpr std::string_view alphanum =
    "_-0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";

std::string GenerateQueueName(std::string name) {
  absl::BitGen bitgen;
  while (name.length() < kQueueNameLen) {
    const size_t index = absl::Uniform(bitgen, 0u, alphanum.length() - 1);
    name += alphanum[index];
  }
  return name;
}

}  // namespace kv_server
