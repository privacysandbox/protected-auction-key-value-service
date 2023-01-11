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

#include "components/util/duration.h"

#include <limits>

#include "absl/time/time.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

TEST(DelayedFlagTest, NeverSet) {
  SimulatedSteadyClock clock;
  DelayedFlag flag(clock);

  EXPECT_FALSE(flag.Get());

  // at infinite time
  clock.SetTime(SteadyTime::Max());
  EXPECT_FALSE(flag.Get());
}

TEST(DelayedFlagTest, SetImmediately) {
  SimulatedSteadyClock clock;
  DelayedFlag flag(clock);

  EXPECT_FALSE(flag.Get());

  flag.SetAfter(absl::ZeroDuration());
  EXPECT_TRUE(flag.Get());

  // at 1 second
  clock.AdvanceTime(absl::Seconds(1));
  EXPECT_TRUE(flag.Get());
}

TEST(DelayedFlagTest, SetImmediatelyAndReset) {
  SimulatedSteadyClock clock;
  DelayedFlag flag(clock);

  EXPECT_FALSE(flag.Get());

  flag.SetAfter(absl::ZeroDuration());
  EXPECT_TRUE(flag.Get());

  // at 1 second
  clock.AdvanceTime(absl::Seconds(1));
  EXPECT_TRUE(flag.Get());
  flag.Reset();
  EXPECT_FALSE(flag.Get());

  // at infinite time
  clock.SetTime(SteadyTime::Max());
  EXPECT_FALSE(flag.Get());
}

TEST(DelayedFlagTest, SetWithDelay) {
  SimulatedSteadyClock clock;
  DelayedFlag flag(clock);

  EXPECT_FALSE(flag.Get());

  flag.SetAfter(absl::Seconds(1));
  EXPECT_FALSE(flag.Get());

  // at 1 second
  clock.AdvanceTime(absl::Seconds(1));
  EXPECT_TRUE(flag.Get());

  // at 2 seconds
  clock.AdvanceTime(absl::Seconds(1));
  EXPECT_TRUE(flag.Get());
}

TEST(DelayedFlagTest, InterruptedSet) {
  SimulatedSteadyClock clock;
  DelayedFlag flag(clock);

  EXPECT_FALSE(flag.Get());

  flag.SetAfter(absl::Seconds(1));
  EXPECT_FALSE(flag.Get());

  // at 0.5 seconds
  clock.AdvanceTime(absl::Milliseconds(500));
  EXPECT_FALSE(flag.Get());
  flag.Reset();
  EXPECT_FALSE(flag.Get());

  // at 2.5 seconds
  clock.AdvanceTime(absl::Seconds(2));
  EXPECT_FALSE(flag.Get());

  // at infinite time
  clock.SetTime(SteadyTime::Max());
  EXPECT_FALSE(flag.Get());
}

TEST(DelayedFlagTest, OverrideSetSimultaneously) {
  SimulatedSteadyClock clock;
  DelayedFlag flag(clock);

  EXPECT_FALSE(flag.Get());

  flag.SetAfter(absl::Seconds(2));
  flag.SetAfter(absl::Seconds(1));
  EXPECT_FALSE(flag.Get());

  // at 1 seconds
  clock.AdvanceTime(absl::Seconds(1));
  EXPECT_TRUE(flag.Get());

  // at 2 seconds
  clock.AdvanceTime(absl::Seconds(1));
  EXPECT_TRUE(flag.Get());

  // at 3 seconds
  clock.AdvanceTime(absl::Seconds(1));
  EXPECT_TRUE(flag.Get());
}

TEST(DelayedFlagTest, OverrideSetSequentially) {
  SimulatedSteadyClock clock;
  DelayedFlag flag(clock);

  EXPECT_FALSE(flag.Get());

  flag.SetAfter(absl::Seconds(3));
  EXPECT_FALSE(flag.Get());

  // at 1 second
  clock.AdvanceTime(absl::Seconds(1));
  EXPECT_FALSE(flag.Get());
  flag.SetAfter(absl::Seconds(1));
  EXPECT_FALSE(flag.Get());

  // at 2 seconds
  clock.AdvanceTime(absl::Seconds(1));
  EXPECT_TRUE(flag.Get());

  // at 3 seconds
  clock.AdvanceTime(absl::Seconds(1));
  EXPECT_TRUE(flag.Get());

  // at 4 seconds
  clock.AdvanceTime(absl::Seconds(1));
  EXPECT_TRUE(flag.Get());
}

TEST(SteadyTimeTest, SaturatingIncrement) {
  const absl::Duration large_duration =
      absl::Seconds(std::numeric_limits<int64_t>::max());

  SteadyTime t1;
  t1 += large_duration;
  t1 += large_duration;
  EXPECT_GT(t1, SteadyTime());
  EXPECT_EQ(t1, SteadyTime::Max());

  SteadyTime t2;
  t2 += -large_duration;
  t2 += -large_duration;
  EXPECT_LT(t2, SteadyTime());
  EXPECT_EQ(t2, SteadyTime::Min());
}

TEST(SteadyTimeTest, SaturatingSum) {
  const absl::Duration large_duration =
      absl::Seconds(std::numeric_limits<int64_t>::max());

  const SteadyTime t1 = (SteadyTime() + large_duration) + large_duration;
  EXPECT_GT(t1, SteadyTime());
  EXPECT_EQ(t1, SteadyTime::Max());

  const SteadyTime t2 = (SteadyTime() + (-large_duration)) + (-large_duration);
  EXPECT_LT(t2, SteadyTime());
  EXPECT_EQ(t2, SteadyTime::Min());
}

TEST(SteadyTimeTest, SaturatingDifference) {
  const absl::Duration large_duration =
      absl::Seconds(std::numeric_limits<int64_t>::max());

  const SteadyTime t1 = (SteadyTime() - large_duration) - large_duration;
  EXPECT_LT(t1, SteadyTime());
  EXPECT_EQ(t1, SteadyTime::Min());

  const SteadyTime t2 = (SteadyTime() - (-large_duration)) - (-large_duration);
  EXPECT_GT(t2, SteadyTime());
  EXPECT_EQ(t2, SteadyTime::Max());
}

}  // namespace
}  // namespace kv_server
