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

#include "components/errors/retry.h"

#include <algorithm>

#include "absl/time/time.h"

namespace fledge::kv_server {
namespace {

constexpr absl::Duration kMaxRetryInterval = absl::Minutes(2);
constexpr uint32_t kRetryBackoffBase = 2;

class RealSleepFor : public SleepFor {
 public:
  void Duration(absl::Duration d) const override { absl::SleepFor(d); }
};

}  // namespace

// static
SleepFor& SleepFor::Real() {
  static RealSleepFor sleep_for;
  return sleep_for;
}

absl::Duration ExponentialBackoffForRetry(uint32_t retries) {
  const absl::Duration backoff = absl::Seconds(pow(kRetryBackoffBase, retries));
  return std::min(backoff, kMaxRetryInterval);
}
}  // namespace fledge::kv_server
