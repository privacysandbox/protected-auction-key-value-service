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
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {

using privacy_sandbox::server_common::MockMetricsRecorder;

class MockParameterClient : public ParameterClient {
 public:
  MOCK_METHOD(absl::StatusOr<std::string>, GetParameter,
              (std::string_view parameter_name), (const, override));
  MOCK_METHOD(absl::StatusOr<int32_t>, GetInt32Parameter,
              (std::string_view parameter_name), (const, override));
  MOCK_METHOD(absl::StatusOr<bool>, GetBoolParameter,
              (std::string_view parameter_name), (const, override));
};

TEST(ParameterFetcherTest, CreateChangeNotifierSmokeTest) {
  MockParameterClient client;
  EXPECT_CALL(client, GetParameter("kv-server-local-directory"))
      .Times(1)
      .WillOnce(::testing::Return(::testing::TempDir()));
  MockMetricsRecorder metrics_recorder;
  ParameterFetcher fetcher(
      /*environment=*/"local", client, &metrics_recorder);

  const auto metadata = fetcher.GetBlobStorageNotifierMetadata();
  auto local_notifier_metadata = std::get<LocalNotifierMetadata>(metadata);

  EXPECT_EQ(::testing::TempDir(), local_notifier_metadata.local_directory);
}

TEST(ParameterFetcherTest, CreateDeltaFileRecordChangeNotifierSmokeTest) {
  MockParameterClient client;
  EXPECT_CALL(client, GetParameter("kv-server-local-realtime-directory"))
      .Times(1)
      .WillOnce(::testing::Return(::testing::TempDir()));
  MockMetricsRecorder metrics_recorder;
  ParameterFetcher fetcher(
      /*environment=*/"local", client, &metrics_recorder);

  const int32_t num_shards = 1;
  const int32_t shard_num = 0;
  const auto notifier_metadata =
      fetcher.GetRealtimeNotifierMetadata(num_shards, shard_num);
  auto local_notifier_metadata =
      std::get<LocalNotifierMetadata>(notifier_metadata);

  EXPECT_EQ(::testing::TempDir(), local_notifier_metadata.local_directory);
}

}  // namespace kv_server
