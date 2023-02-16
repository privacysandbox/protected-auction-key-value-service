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

#include <string>

#include "absl/status/statusor.h"
#include "components/cloud_config/parameter_client.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

TEST(ParameterClientLocal, NotImplemented) {
  std::unique_ptr<ParameterClient> client = ParameterClient::Create();
  ASSERT_TRUE(client != nullptr);

  EXPECT_EQ(absl::StatusCode::kUnimplemented,
            client->GetParameter("unused").status().code());
  EXPECT_EQ(absl::StatusCode::kUnimplemented,
            client->GetInt32Parameter("unused").status().code());
}

// TODO(237669491): Add tests here

}  // namespace
}  // namespace kv_server
