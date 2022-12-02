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

#ifndef COMPONENTS_ERRORS_MOCKS_H_
#define COMPONENTS_ERRORS_MOCKS_H_

#include "components/errors/retry.h"
#include "gmock/gmock.h"

namespace fledge::kv_server {
class MockSleepFor : public SleepFor {
 public:
  MOCK_METHOD(void, Duration, (absl::Duration), (const override));
};
}  // namespace fledge::kv_server

#endif  // COMPONENTS_ERRORS_MOCKS_H_
