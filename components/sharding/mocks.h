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

#ifndef COMPONENTS_DATA_SERVER_SHARDING_MOCKS_H_
#define COMPONENTS_DATA_SERVER_SHARDING_MOCKS_H_

#include <string>
#include <utility>
#include <vector>

#include "components/sharding/shard_manager.h"
#include "gmock/gmock.h"

namespace kv_server {
class MockRandomGenerator : public RandomGenerator {
 public:
  MockRandomGenerator() : RandomGenerator() {}
  MOCK_METHOD(int64_t, Get, (int64_t upper_bound), (override));
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_SHARDING_MOCKS_H_
