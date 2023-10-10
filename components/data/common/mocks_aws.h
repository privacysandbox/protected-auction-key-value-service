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

#ifndef COMPONENTS_DATA_COMMON_MOCKS_AWS_H_
#define COMPONENTS_DATA_COMMON_MOCKS_AWS_H_

#include "components/data/realtime/delta_file_record_change_notifier.h"
#include "gmock/gmock.h"

namespace kv_server {

class MockDeltaFileRecordChangeNotifier : public DeltaFileRecordChangeNotifier {
 public:
  MOCK_METHOD(absl::StatusOr<NotificationsContext>, GetNotifications,
              (absl::Duration max_wait,
               const std::function<bool()>& should_stop_callback),
              (override));
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_COMMON_MOCKS_AWS_H_
