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

#include "components/data_server/server/server.h"
#include "components/udf/mocks.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "opentelemetry/sdk/resource/resource.h"

namespace kv_server {
namespace {

using opentelemetry::sdk::resource::Resource;
using testing::_;

class MockInstanceClient : public InstanceClient {
 public:
  MOCK_METHOD(absl::StatusOr<std::string>, GetEnvironmentTag, (), (override));
  MOCK_METHOD(absl::Status, RecordLifecycleHeartbeat,
              (std::string_view lifecycle_hook_name), (override));
  MOCK_METHOD(absl::Status, CompleteLifecycle,
              (std::string_view lifecycle_hook_name), (override));
  MOCK_METHOD(absl::StatusOr<std::string>, GetInstanceId, (), (override));
};

class MockParameterClient : public ParameterClient {
 public:
  MOCK_METHOD(absl::StatusOr<std::string>, GetParameter,
              (std::string_view parameter_name), (const, override));
  MOCK_METHOD(absl::StatusOr<int32_t>, GetInt32Parameter,
              (std::string_view parameter_name), (const, override));

  void RegisterRequiredTelemetryExpectations() {
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
  // These should be a no-op as the server was never started:
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
  MockInstanceClient instance_client;
  MockParameterClient parameter_client;
  parameter_client.RegisterRequiredTelemetryExpectations();

  EXPECT_CALL(parameter_client, GetParameter("kv-server-environment-directory"))
      .WillOnce(::testing::Return("this is not a directory"));
  EXPECT_CALL(
      parameter_client,
      GetInt32Parameter("kv-server-environment-data-loading-num-threads"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-connections"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-range-bytes"))
      .WillOnce(::testing::Return(1));

  InitializeMetrics();
  auto mock_udf_client = std::make_unique<MockUdfClient>();
  MockCodeFetcher code_fetcher;

  kv_server::Server server;
  absl::Status status =
      server.Init(parameter_client, instance_client, code_fetcher,
                  std::move(mock_udf_client), "environment", "instance id");
  EXPECT_FALSE(status.ok());
}

TEST(ServerLocalTest, InitPassesWithDeltaDirectoryAndRealtimeDirectory) {
  MockInstanceClient instance_client;
  MockParameterClient parameter_client;
  parameter_client.RegisterRequiredTelemetryExpectations();

  EXPECT_CALL(parameter_client, GetParameter("kv-server-environment-directory"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(parameter_client,
              GetParameter("kv-server-environment-data-bucket-id"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(parameter_client,
              GetParameter("kv-server-environment-realtime-directory"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(
      parameter_client,
      GetInt32Parameter("kv-server-environment-realtime-updater-num-threads"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      parameter_client,
      GetInt32Parameter("kv-server-environment-data-loading-num-threads"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-connections"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-range-bytes"))
      .WillOnce(::testing::Return(1));

  InitializeMetrics();
  auto mock_udf_client = std::make_unique<MockUdfClient>();
  MockCodeFetcher code_fetcher;
  CodeConfig code_config{.js = "function SomeUDFCode(){}",
                         .udf_handler_name = "SomeUDFCode"};

  EXPECT_CALL(code_fetcher, FetchCodeConfig(_))
      .WillOnce(testing::Return(code_config));

  kv_server::Server server;
  absl::Status status =
      server.Init(parameter_client, instance_client, code_fetcher,
                  std::move(mock_udf_client), "environment", "instance id");
  EXPECT_TRUE(status.ok());
}

TEST(ServerLocalTest, GracefulServerShutdown) {
  MockInstanceClient instance_client;
  MockParameterClient parameter_client;
  parameter_client.RegisterRequiredTelemetryExpectations();

  EXPECT_CALL(parameter_client, GetParameter("kv-server-environment-directory"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(parameter_client,
              GetParameter("kv-server-environment-data-bucket-id"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(parameter_client,
              GetParameter("kv-server-environment-realtime-directory"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(
      parameter_client,
      GetInt32Parameter("kv-server-environment-realtime-updater-num-threads"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      parameter_client,
      GetInt32Parameter("kv-server-environment-data-loading-num-threads"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-connections"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-range-bytes"))
      .WillOnce(::testing::Return(1));

  InitializeMetrics();
  auto mock_udf_client = std::make_unique<MockUdfClient>();
  MockCodeFetcher code_fetcher;
  CodeConfig code_config{.js = "function SomeUDFCode(){}",
                         .udf_handler_name = "SomeUDFCode"};

  EXPECT_CALL(code_fetcher, FetchCodeConfig(_))
      .WillOnce(testing::Return(code_config));

  kv_server::Server server;
  absl::Status status =
      server.Init(parameter_client, instance_client, code_fetcher,
                  std::move(mock_udf_client), "environment", "instance id");
  ASSERT_TRUE(status.ok());
  std::thread server_thread(&kv_server::Server::Wait, &server);
  server.GracefulShutdown(absl::Seconds(5));
  server_thread.join();
}

TEST(ServerLocalTest, ForceServerShutdown) {
  MockInstanceClient instance_client;
  MockParameterClient parameter_client;
  parameter_client.RegisterRequiredTelemetryExpectations();

  EXPECT_CALL(parameter_client, GetParameter("kv-server-environment-directory"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(parameter_client,
              GetParameter("kv-server-environment-data-bucket-id"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(parameter_client,
              GetParameter("kv-server-environment-realtime-directory"))
      .WillOnce(::testing::Return(::testing::TempDir()));
  EXPECT_CALL(
      parameter_client,
      GetInt32Parameter("kv-server-environment-realtime-updater-num-threads"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      parameter_client,
      GetInt32Parameter("kv-server-environment-data-loading-num-threads"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-connections"))
      .WillOnce(::testing::Return(1));
  EXPECT_CALL(
      parameter_client,
      GetInt32Parameter("kv-server-environment-s3client-max-range-bytes"))
      .WillOnce(::testing::Return(1));

  InitializeMetrics();
  auto mock_udf_client = std::make_unique<MockUdfClient>();
  MockCodeFetcher code_fetcher;
  CodeConfig code_config{.js = "function SomeUDFCode(){}",
                         .udf_handler_name = "SomeUDFCode"};

  EXPECT_CALL(code_fetcher, FetchCodeConfig(_))
      .WillOnce(testing::Return(code_config));

  kv_server::Server server;
  absl::Status status =
      server.Init(parameter_client, instance_client, code_fetcher,
                  std::move(mock_udf_client), "environment", "instance id");
  ASSERT_TRUE(status.ok());
  std::thread server_thread(&kv_server::Server::Wait, &server);
  server.ForceShutdown();
  server_thread.join();
}

}  // namespace
}  // namespace kv_server
