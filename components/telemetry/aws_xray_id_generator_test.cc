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

#include "components/telemetry/aws_xray_id_generator.h"

#include <arpa/inet.h>

#include "absl/time/clock.h"
#include "gtest/gtest.h"

namespace kv_server {

TEST(XRayIdGenerator, GenerateTraceId) {
  const auto now = absl::Now();
  uint32_t seconds_since_epoch = absl::ToUnixSeconds(now);
  auto generator = CreateXrayIdGenerator([now] { return now; });
  auto trace_id = generator->GenerateTraceId();
  ASSERT_TRUE(trace_id.IsValid());
  // First 32 bits should be the time in network byte order
  auto seconds_nl = htonl(seconds_since_epoch);
  ASSERT_EQ(seconds_nl,
            *(reinterpret_cast<const uint32_t*>(trace_id.Id().data())));
}

}  // namespace kv_server
