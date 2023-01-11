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

#ifndef COMPONENTS_UTIL_DURATION_H_
#define COMPONENTS_UTIL_DURATION_H_

#include <chrono>
#include <memory>
#include <string>

#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"

namespace kv_server {

class SteadyTime {
 public:
  // Constructs a default time. Note that the value is not specified, but it
  // should be relatively distant from both Max and Min. (On many systems, it
  // will represent a time near the start of the process, or last reboot.)
  SteadyTime() : time_(std::chrono::time_point<std::chrono::steady_clock>()) {}

  // Construct the current time
  static SteadyTime Now();

  // Construct the maximum representable time
  static SteadyTime Max();

  // Construct the minimum representable time
  static SteadyTime Min();

  SteadyTime& operator+=(const absl::Duration d);

  friend SteadyTime operator+(const SteadyTime& t, const absl::Duration d);
  friend SteadyTime operator+(const absl::Duration d, const SteadyTime& t) {
    return t + d;
  }

  friend SteadyTime operator-(const SteadyTime& t, const absl::Duration d);

  friend absl::Duration operator-(const SteadyTime& t1, const SteadyTime& t2);

  friend bool operator<(const SteadyTime& t1, const SteadyTime& t2) {
    return t1.time_ < t2.time_;
  }
  friend bool operator<=(const SteadyTime& t1, const SteadyTime& t2) {
    return t1.time_ <= t2.time_;
  }
  friend bool operator>(const SteadyTime& t1, const SteadyTime& t2) {
    return t1.time_ > t2.time_;
  }
  friend bool operator>=(const SteadyTime& t1, const SteadyTime& t2) {
    return t1.time_ >= t2.time_;
  }
  friend bool operator==(const SteadyTime& t1, const SteadyTime& t2) {
    return t1.time_ == t2.time_;
  }
  friend bool operator!=(const SteadyTime& t1, const SteadyTime& t2) {
    return t1.time_ != t2.time_;
  }

 private:
  explicit SteadyTime(std::chrono::time_point<std::chrono::steady_clock> time)
      : time_(time) {}

  std::chrono::time_point<std::chrono::steady_clock> time_;
};

// An abstract interface representing a SteadyClock, which is an object that can
// tell you the current steady time.
//
// This interface allows decoupling code that uses time from the code that
// creates a point in time. You can use this to your advantage by injecting
// SteadyClocks into interfaces rather than having implementations call
// SteadyTime::Now() directly.
//
// The SteadyClock::RealClock() function returns a reference to the global
// realtime clock.
//
// Example:
//
//   bool OneSecondSince(SteadyTime time, SteadyClock& clock) {
//     return (clock->Now() - time) >= absl::Seconds(1);
//   }
//
//   // Production code.
//   OneSecondSince(start_time, Clock::RealClock());
//
//   // Test code:
//   MyTestClock test_clock(<TIME>);
//   OneSecondSince(start_time, &test_clock);
//
class SteadyClock {
 public:
  // Returns a reference to the global realtime clock.
  // The returned clock is thread-safe.
  static SteadyClock& RealClock();

  virtual ~SteadyClock();

  virtual SteadyTime Now() = 0;
};

// A simulated clock is a concrete Clock implementation that does not "tick"
// on its own.  Time is advanced by explicit calls to the AdvanceTime() or
// SetTime() functions.
//
// Example:
//   SimulatedSteadyClock sim_clock;
//   SteadyTime now = sim_clock.Now();
//   // now == SteadyTime()
//
//   now = sim_clock.Now();
//   // now == SteadyTime() (still)
//
//   sim_clock.AdvanceTime(absl::Seconds(3));
//   now = sim_clock.Now();
//   // now == SteadyTime() + absl::Seconds(3)
//
// This code is thread-safe.
class SimulatedSteadyClock : public SteadyClock {
 public:
  explicit SimulatedSteadyClock(SteadyTime t) : now_(t) {}
  SimulatedSteadyClock() : SimulatedSteadyClock(SteadyTime()) {}

  // Returns the simulated steady time.
  SteadyTime Now() override;

  // Advances the simulated time by the specified duration.
  void AdvanceTime(absl::Duration d);

  // Sets the simulated steady time to the argument.
  void SetTime(SteadyTime t);

 private:
  absl::Mutex lock_;
  SteadyTime now_ ABSL_GUARDED_BY(lock_);
};

// Helper class that allows simple wall time measurements.
class Stopwatch {
 public:
  // Starts the timer.
  explicit Stopwatch(SteadyClock& clock);
  Stopwatch() : Stopwatch(SteadyClock::RealClock()) {}

  // Resets the start time.
  void Reset();

  // Returns the time this was last started.
  SteadyTime GetStartTime() const;

  // Returns `now() - start time` as an absl::Duration.
  absl::Duration GetElapsedTime() const;

 private:
  SteadyClock& clock_;
  SteadyTime start_time_;
};

// Same as above for CPU thread time
class CpuThreadTimeStopwatch {
 public:
  // Starts the timer
  CpuThreadTimeStopwatch();

  // Resets the start time.
  void Reset();

  // Returns the time this was last started. (The origin should be approximately
  // when the thread was started.)
  absl::Duration GetStartTime() const;

  // Returns `now() - start time` as an absl::Duration.
  absl::Duration GetElapsedTime() const;

 private:
  absl::Duration start_time_;
};

// Allows to set a flag with an expiration time.
class ExpiringFlag {
 public:
  explicit ExpiringFlag(SteadyClock& clock = SteadyClock::RealClock())
      : time_since_set_(clock) {}

  // Makes Get() return true for the next `max_duration` time.
  void Set(absl::Duration max_duration) {
    is_set_ = true;
    max_duration_ = max_duration;
    time_since_set_.Reset();
  }

  // Resets the flag to false.
  void Reset() { is_set_ = false; }

  // Returns true if the flag was set and hasn't expired. The flag is reset if
  // it has expired.
  bool Get() {
    if (!is_set_) {
      return false;
    }
    if (time_since_set_.GetElapsedTime() > max_duration_) {
      Reset();
    }
    return is_set_;
  }

  absl::Duration GetTimeRemaining() {
    if (!is_set_) {
      return absl::ZeroDuration();
    }
    const absl::Duration time_remaining =
        max_duration_ - time_since_set_.GetElapsedTime();
    if (time_remaining < absl::ZeroDuration()) {
      Reset();
    }
    return time_remaining;
  }

 private:
  bool is_set_ = false;
  absl::Duration max_duration_;
  Stopwatch time_since_set_;
};

// A flag that can be set after some time has passed.
class DelayedFlag {
 public:
  explicit DelayedFlag(SteadyClock& clock = SteadyClock::RealClock())
      : is_set_(false),
        wait_duration_(absl::InfiniteDuration()),
        time_since_set_(clock) {}

  // Ensure Get() will return true after `wait_duration` has passed.
  // Get() may return true before that time, if another call to SetAfter()
  // requires it.
  void SetAfter(absl::Duration duration) {
    if (!WillBeSetWithin(duration)) {
      wait_duration_ = duration;
      time_since_set_.Reset();
    }
  }

  // Resets the flag to false, and cancels any pending set events.
  void Reset() {
    is_set_ = false;
    wait_duration_ = absl::InfiniteDuration();
  }

  // Returns true if the flag has been set.
  bool Get() {
    if (is_set_) {
      return true;
    }
    const absl::Duration elapsed_time = time_since_set_.GetElapsedTime();
    if (wait_duration_ != absl::InfiniteDuration() &&
        elapsed_time >= wait_duration_) {
      is_set_ = true;
    }
    return is_set_;
  }

  // Returns true if the flag has a timer running after which it will be set.
  bool WillBeSet() { return wait_duration_ != absl::InfiniteDuration(); }

 private:
  // Returns true if the flag has a timer running and will be set by the time
  // `duration` has passed.
  bool WillBeSetWithin(absl::Duration duration) {
    if (duration == absl::InfiniteDuration()) {
      return WillBeSet();
    }
    const absl::Duration elapsed_time = time_since_set_.GetElapsedTime();
    const absl::Duration remaining_wait = wait_duration_ - elapsed_time;
    return remaining_wait <= duration;
  }

  bool is_set_;
  absl::Duration wait_duration_;
  Stopwatch time_since_set_;
};

std::string GetMsecTimestamp();

std::string GetUsecTimestamp();

}  // namespace kv_server

#endif  // COMPONENTS_UTIL_DURATION_H_
