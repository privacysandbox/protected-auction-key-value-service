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

#include <thread>

#include "components/data_server/server/mocks.h"
#include "components/data_server/server/server.h"
#include "components/udf/mocks.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "opentelemetry/sdk/resource/resource.h"

namespace kv_server {
namespace {

using opentelemetry::sdk::resource::Resource;
using privacy_sandbox::server_common::ConfigureMetrics;
using testing::_;

class MockParameterClient : public ParameterClient {
 public:
  MOCK_METHOD(absl::StatusOr<std::string>, GetParameter,
              (std::string_view parameter_name), (const, override));
  MOCK_METHOD(absl::StatusOr<int32_t>, GetInt32Parameter,
              (std::string_view parameter_name), (const, override));
  MOCK_METHOD(absl::StatusOr<bool>, GetBoolParameter,
              (std::string_view parameter_name), (const, override));

  void RegisterRequiredTelemetryExpectations() {
    EXPECT_CALL(*this,
                GetInt32Parameter(
                    "kv-server-environment-metrics-export-interval-millis"))
        .WillOnce(::testing::Return(100));
    EXPECT_CALL(*this,
                GetInt32Parameter(
                    "kv-server-environment-metrics-export-timeout-millis"))
        .WillOnce(::testing::Return(200));

    EXPECT_CALL(*this, GetParameter("kv-server-environment-launch-hook"))
        .WillOnce(::testing::Return("mock launch hook"));
    EXPECT_CALL(*this, GetInt32Parameter(
                           "kv-server-environment-backup-poll-frequency-secs"))
        .WillOnce(::testing::Return(123));
    EXPECT_CALL(*this, GetParameter("kv-server-environment-mode"))
        .WillOnce(::testing::Return("DSP"));
  }
};

void InitializeMetrics() {
  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
      metrics_options;
  // The defaults for these values are 30 and 60s and we don't want to wait that
  // long for metrics to be flushed when each test finishes.
  metrics_options.export_interval_millis = std::chrono::milliseconds(200);
  metrics_options.export_timeout_millis = std::chrono::milliseconds(100);
  ConfigureMetrics(Resource::Create({}), metrics_options);
}

TEST(ServerLocalTest, WaitWithoutStart) {
  InitializeMetrics();
  kv_server::Server server;
  // This should be a no-op as the server was never started:
  server.Wait();
}

TEST(ServerLocalTest, ShutdownWithoutStart) {
  InitializeMetrics();
  kv_server::Server server;
  // These should be a no-op as the server was never started:
  server.GracefulShutdown(absl::Seconds(1));
  server.ForceShutdown();
}

TEST(ServerLocalTest, InitFailsWithNoDeltaDirectory) {
  auto instance_client = std::make_unique<MockInstanceClient>();
  auto parameter_client = std::make_unique<MockParameterClient>();
  parameter_client->RegisterRequiredTelemetryExpectations();
  auto mock_udf_client = std::make_unique<MockUdfClient>();

  EXPECT_CALL(*instance_client, GetEnvironmentTag())
      .WillOnce(::testing::Return("environment"));
  EXPECT_CALL(*instance_client, GetInstanceId())
      .WillOnce(::testing::Return("instance id"));
  EXPECT_CALL(*instance_client, GetShardNumTag())
      .WillOnce(::testing::Return("1"));

  EXPECT_CALL(*parameter_client,
              GetParameter("kv-server-environment-directory"))
      .WillOnce(::testing::Return("this is not a directory"));
  EXPECT_CALL(
      *parameter_client,
      GetInt32Parameter("kv-server-environment-data-loading-num-threads"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      *parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-connections"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      *parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-range-bytes"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(*parameter_client,
              GetInt32Parameter("kv-server-environment-num-shards"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(*parameter_client,
              GetInt32Parameter("kv-server-environment-udf-num-workers"))
      .WillOnce(::testing::Return(2));
  EXPECT_CALL(*parameter_client,
              GetBoolParameter("kv-server-environment-route-v1-to-v2"))
      .WillOnce(::testing::Return(false));

  kv_server::Server server;
  absl::Status status =
      server.Init(std::move(parameter_client), std::move(instance_client),
                  std::move(mock_udf_client));
  EXPECT_FALSE(status.ok());
}

TEST(ServerLocalTest, InitPassesWithDeltaDirectoryAndRealtimeDirectory) {
  auto instance_client = std::make_unique<MockInstanceClient>();
  auto parameter_client = std::make_unique<MockParameterClient>();
  parameter_client->RegisterRequiredTelemetryExpectations();
  auto mock_udf_client = std::make_unique<MockUdfClient>();

  EXPECT_CALL(*instance_client, GetEnvironmentTag())
      .WillOnce(::testing::Return("environment"));
  EXPECT_CALL(*instance_client, GetInstanceId())
      .WillOnce(::testing::Return("instance id"));
  EXPECT_CALL(*instance_client, GetShardNumTag())
      .WillOnce(::testing::Return("1"));

  EXPECT_CALL(*parameter_client,
              GetParameter("kv-server-environment-directory"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(*parameter_client,
              GetParameter("kv-server-environment-data-bucket-id"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(*parameter_client,
              GetParameter("kv-server-environment-realtime-directory"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(
      *parameter_client,
      GetInt32Parameter("kv-server-environment-realtime-updater-num-threads"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      *parameter_client,
      GetInt32Parameter("kv-server-environment-data-loading-num-threads"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      *parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-connections"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      *parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-range-bytes"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(*parameter_client,
              GetInt32Parameter("kv-server-environment-num-shards"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(*parameter_client,
              GetInt32Parameter("kv-server-environment-udf-num-workers"))
      .WillOnce(::testing::Return(2));
  EXPECT_CALL(*parameter_client,
              GetBoolParameter("kv-server-environment-route-v1-to-v2"))
      .WillOnce(::testing::Return(false));

  EXPECT_CALL(*mock_udf_client, SetCodeObject(_))
      .WillOnce(testing::Return(absl::OkStatus()));

  kv_server::Server server;
  absl::Status status =
      server.Init(std::move(parameter_client), std::move(instance_client),
                  std::move(mock_udf_client));
  EXPECT_TRUE(status.ok());
}

TEST(ServerLocalTest, GracefulServerShutdown) {
  auto instance_client = std::make_unique<MockInstanceClient>();
  auto parameter_client = std::make_unique<MockParameterClient>();
  parameter_client->RegisterRequiredTelemetryExpectations();
  auto mock_udf_client = std::make_unique<MockUdfClient>();

  EXPECT_CALL(*instance_client, GetEnvironmentTag())
      .WillOnce(::testing::Return("environment"));
  EXPECT_CALL(*instance_client, GetInstanceId())
      .WillOnce(::testing::Return("instance id"));
  EXPECT_CALL(*instance_client, GetShardNumTag())
      .WillOnce(::testing::Return("1"));

  EXPECT_CALL(*parameter_client,
              GetParameter("kv-server-environment-directory"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(*parameter_client,
              GetParameter("kv-server-environment-data-bucket-id"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(*parameter_client,
              GetParameter("kv-server-environment-realtime-directory"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(
      *parameter_client,
      GetInt32Parameter("kv-server-environment-realtime-updater-num-threads"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      *parameter_client,
      GetInt32Parameter("kv-server-environment-data-loading-num-threads"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      *parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-connections"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      *parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-range-bytes"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(*parameter_client,
              GetInt32Parameter("kv-server-environment-num-shards"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(*parameter_client,
              GetInt32Parameter("kv-server-environment-udf-num-workers"))
      .WillOnce(::testing::Return(2));
  EXPECT_CALL(*parameter_client,
              GetBoolParameter("kv-server-environment-route-v1-to-v2"))
      .WillOnce(::testing::Return(false));

  EXPECT_CALL(*mock_udf_client, SetCodeObject(_))
      .WillOnce(testing::Return(absl::OkStatus()));

  kv_server::Server server;
  absl::Status status =
      server.Init(std::move(parameter_client), std::move(instance_client),
                  std::move(mock_udf_client));
  ASSERT_TRUE(status.ok());
  std::thread server_thread(&kv_server::Server::Wait, &server);
  server.GracefulShutdown(absl::Seconds(5));
  server_thread.join();
}

TEST(ServerLocalTest, ForceServerShutdown) {
  auto instance_client = std::make_unique<MockInstanceClient>();
  auto parameter_client = std::make_unique<MockParameterClient>();
  parameter_client->RegisterRequiredTelemetryExpectations();
  auto mock_udf_client = std::make_unique<MockUdfClient>();

  EXPECT_CALL(*instance_client, GetEnvironmentTag())
      .WillOnce(::testing::Return("environment"));
  EXPECT_CALL(*instance_client, GetInstanceId())
      .WillOnce(::testing::Return("instance id"));
  EXPECT_CALL(*instance_client, GetShardNumTag())
      .WillOnce(::testing::Return("1"));

  EXPECT_CALL(*parameter_client,
              GetParameter("kv-server-environment-directory"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(*parameter_client,
              GetParameter("kv-server-environment-data-bucket-id"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(*parameter_client,
              GetParameter("kv-server-environment-realtime-directory"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(
      *parameter_client,
      GetInt32Parameter("kv-server-environment-realtime-updater-num-threads"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      *parameter_client,
      GetInt32Parameter("kv-server-environment-data-loading-num-threads"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      *parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-connections"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      *parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-range-bytes"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(*parameter_client,
              GetInt32Parameter("kv-server-environment-num-shards"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(*parameter_client,
              GetInt32Parameter("kv-server-environment-udf-num-workers"))
      .WillOnce(::testing::Return(2));
  EXPECT_CALL(*parameter_client,
              GetBoolParameter("kv-server-environment-route-v1-to-v2"))
      .WillOnce(::testing::Return(false));

  EXPECT_CALL(*mock_udf_client, SetCodeObject(_))
      .WillOnce(testing::Return(absl::OkStatus()));

  kv_server::Server server;
  absl::Status status =
      server.Init(std::move(parameter_client), std::move(instance_client),
                  std::move(mock_udf_client));
  ASSERT_TRUE(status.ok());
  std::thread server_thread(&kv_server::Server::Wait, &server);
  server.ForceShutdown();
  server_thread.join();
}

}  // namespace
}  // namespace kv_server
