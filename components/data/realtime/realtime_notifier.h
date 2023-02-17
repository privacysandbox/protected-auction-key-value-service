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

#ifndef COMPONENTS_DATA_REALTIME_NOTIFIER_H_
#define COMPONENTS_DATA_REALTIME_NOTIFIER_H_

#include <memory>
#include <string>

#include "components/data/common/thread_notifier.h"
#include "components/data/realtime/delta_file_record_change_notifier.h"
#include "components/errors/retry.h"

namespace kv_server {
class RealtimeNotifier {
 public:
  virtual ~RealtimeNotifier() = default;

  // Starts to monitor high priority updates.
  // `DeltaFileRecordChangeNotifier` must not be deallocated until `Stop`
  // returns or `RealtimeNotifier` is destroyed.
  // Calls `callback` on every high priority update.
  // `callback` blocks this object's operations so it should
  // return as soon as possible.
  //
  // Start and Stop should be called on the same thread as
  // the constructor.
  virtual absl::Status Start(
      DeltaFileRecordChangeNotifier& change_notifier,
      std::function<void(const std::string& key)> callback) = 0;

  // Blocks until `IsRunning` is False.
  virtual absl::Status Stop() = 0;

  // Returns False before calling `Start` or after `Stop` is
  // successful.
  virtual bool IsRunning() const = 0;

  static std::unique_ptr<RealtimeNotifier> Create(
      ThreadNotifier& thread_notifier,
      const SleepFor& sleep_for = SleepFor::Real());
};

}  // namespace kv_server
#endif  // COMPONENTS_DATA_REALTIME_NOTIFIER_H_
