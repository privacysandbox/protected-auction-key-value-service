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
#include <thread>

#include "absl/status/statusor.h"
#include "components/cloud_config/parameter_client.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

TEST(ParameterClientLocal, ExpectedFlagDefaultsArePresent) {
  std::unique_ptr<ParameterClient> client = ParameterClient::Create();
  ASSERT_TRUE(client != nullptr);

  {
    const auto statusor = client->GetParameter("kv-server-local-directory");
    ASSERT_TRUE(statusor.ok());
    EXPECT_EQ("", *statusor);
  }
  {
    const auto statusor =
        client->GetParameter("kv-server-local-realtime-directory");
    ASSERT_TRUE(statusor.ok());
    EXPECT_EQ("", *statusor);
  }
  {
    const auto statusor = client->GetParameter("kv-server-local-launch-hook");
    ASSERT_TRUE(statusor.ok());
    EXPECT_EQ("", *statusor);
  }
  {
    const auto statusor = client->GetParameter("kv-server-local-mode");
    ASSERT_TRUE(statusor.ok());
    EXPECT_EQ("DSP", *statusor);
  }
  {
    const auto statusor = client->GetInt32Parameter(
        "kv-server-local-metrics-export-interval-millis");
    ASSERT_TRUE(statusor.ok());
    EXPECT_EQ(30000, *statusor);
  }
  {
    const auto statusor = client->GetInt32Parameter(
        "kv-server-local-metrics-export-timeout-millis");
    ASSERT_TRUE(statusor.ok());
    EXPECT_EQ(5000, *statusor);
  }
  {
    const auto statusor =
        client->GetInt32Parameter("kv-server-local-backup-poll-frequency-secs");
    ASSERT_TRUE(statusor.ok());
    EXPECT_EQ(5, *statusor);
  }
  {
    const auto statusor = client->GetInt32Parameter(
        "kv-server-local-realtime-updater-num-threads");
    ASSERT_TRUE(statusor.ok());
    EXPECT_EQ(1, *statusor);
  }
  {
    const auto statusor =
        client->GetInt32Parameter("kv-server-local-data-loading-num-threads");
    ASSERT_TRUE(statusor.ok());
    EXPECT_EQ(std::thread::hardware_concurrency(), *statusor);
  }
  {
    const auto statusor =
        client->GetInt32Parameter("kv-server-local-s3client-max-connections");
    ASSERT_TRUE(statusor.ok());
    EXPECT_EQ(1, *statusor);
  }
  {
    const auto statusor =
        client->GetInt32Parameter("kv-server-local-s3client-max-range-bytes");
    ASSERT_TRUE(statusor.ok());
    EXPECT_EQ(1, *statusor);
  }
}

}  // namespace
}  // namespace kv_server
