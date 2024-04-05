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

#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "components/data_server/server/parameter_fetcher.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {

TEST(ParameterFetcherTest, SmokeTest) {
  // TODO(b/237669491): Write tests once we work out how to mock out GCP.
  EXPECT_TRUE(true);
}

}  // namespace kv_server
