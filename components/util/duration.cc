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

#include <cassert>
#include <chrono>  // NOLINT
#include <ctime>
#include <string>

#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace fledge::kv_server {
namespace {

constexpr std::chrono::steady_clock::duration kDurationZero =
    std::chrono::steady_clock::duration::zero();

constexpr std::chrono::steady_clock::time_point kTimePointMin =
    std::chrono::steady_clock::time_point::min();
constexpr std::chrono::steady_clock::time_point kTimePointMax =
    std::chrono::steady_clock::time_point::max();

absl::Duration GetCurrentCpuThreadTime() {
  timespec time_spec;
  const int ret = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time_spec);
  (void)ret;
  assert(ret == 0);
  return absl::DurationFromTimespec(time_spec);
}

std::chrono::steady_clock::duration ToSteadyDuration(absl::Duration d) {
  if constexpr (std::chrono::steady_clock::period::type::den > 1'000'000) {
    return absl::ToChronoNanoseconds(d);
  } else if constexpr (std::chrono::steady_clock::period::type::den > 1'000) {
    return absl::ToChronoMicroseconds(d);
  } else if constexpr (std::chrono::steady_clock::period::type::den > 1) {
    return absl::ToChronoMilliseconds(d);
  } else {
    return absl::ToChronoSeconds(d);
  }
}

}  // namespace

// static
SteadyTime SteadyTime::Now() {
  return SteadyTime(std::chrono::steady_clock::now());
}

// static
SteadyTime SteadyTime::Max() { return SteadyTime(kTimePointMax); }

// static
SteadyTime SteadyTime::Min() { return SteadyTime(kTimePointMin); }

SteadyTime& SteadyTime::operator+=(const absl::Duration d) {
  const auto chrono_duration = ToSteadyDuration(d);
  if (chrono_duration >= kDurationZero) {
    if (time_ < kTimePointMax - chrono_duration) {
      time_ += chrono_duration;
    } else {
      time_ = kTimePointMax;
    }
  } else {
    if (time_ > kTimePointMin - chrono_duration) {
      time_ += chrono_duration;
    } else {
      time_ = kTimePointMin;
    }
  }
  return *this;
}

SteadyTime operator+(const SteadyTime& t, const absl::Duration d) {
  const auto chrono_duration = ToSteadyDuration(d);
  if (chrono_duration >= kDurationZero) {
    if (t.time_ >= kTimePointMax - chrono_duration) {
      return SteadyTime::Max();
    }
  } else {
    if (t.time_ <= kTimePointMin - chrono_duration) {
      return SteadyTime::Min();
    }
  }
  return SteadyTime(t.time_ + chrono_duration);
}
SteadyTime operator-(const SteadyTime& t, const absl::Duration d) {
  return t + (-d);
}

absl::Duration operator-(const SteadyTime& t1, const SteadyTime& t2) {
  return absl::FromChrono(t1.time_ - t2.time_);
}

Stopwatch::Stopwatch(SteadyClock& clock)
    : clock_(clock), start_time_(clock_.Now()) {}

void Stopwatch::Reset() { start_time_ = clock_.Now(); }

SteadyTime Stopwatch::GetStartTime() const { return start_time_; }

absl::Duration Stopwatch::GetElapsedTime() const {
  return clock_.Now() - start_time_;
}

CpuThreadTimeStopwatch::CpuThreadTimeStopwatch()
    : start_time_(absl::ZeroDuration()) {
  Reset();
}

void CpuThreadTimeStopwatch::Reset() {
  start_time_ = GetCurrentCpuThreadTime();
}

absl::Duration CpuThreadTimeStopwatch::GetStartTime() const {
  return start_time_;
}

absl::Duration CpuThreadTimeStopwatch::GetElapsedTime() const {
  return GetCurrentCpuThreadTime() - start_time_;
}

class RealTimeSteadyClock final : public SteadyClock {
 public:
  RealTimeSteadyClock() = default;
  RealTimeSteadyClock(const RealTimeSteadyClock&) = delete;
  RealTimeSteadyClock& operator=(const RealTimeSteadyClock&) = delete;

  SteadyTime Now() override { return SteadyTime::Now(); }
};

// static
SteadyClock& SteadyClock::RealClock() {
  static RealTimeSteadyClock real_clock;
  return real_clock;
}

SteadyClock::~SteadyClock() {}

SteadyTime SimulatedSteadyClock::Now() {
  absl::MutexLock l(&lock_);
  return now_;
}

void SimulatedSteadyClock::AdvanceTime(absl::Duration d) {
  // Steady clocks are (by definition) monotonic.
  assert(d >= absl::ZeroDuration());

  absl::MutexLock l(&lock_);
  now_ += d;
}

void SimulatedSteadyClock::SetTime(SteadyTime t) {
  absl::MutexLock l(&lock_);

  // Steady clocks are (by definition) monotonic.
  assert(t >= now_);

  now_ = t;
}

std::string GetMsecTimestamp() {
  return std::to_string(absl::ToUnixMillis(absl::Now()));
}

std::string GetUsecTimestamp() {
  return std::to_string(absl::ToUnixMicros(absl::Now()));
}

}  // namespace fledge::kv_server
