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

namespace kv_server {
namespace {

TEST(MessageServiceText, QueueNameProperlyPadded) {
  std::string prefix = "prefix";
  auto padded_prefix = GenerateQueueName(prefix);
  EXPECT_TRUE(absl::StartsWith(padded_prefix, prefix));
  EXPECT_EQ(padded_prefix.lenght, 80);
}

}  // namespace
}  // namespace kv_server
