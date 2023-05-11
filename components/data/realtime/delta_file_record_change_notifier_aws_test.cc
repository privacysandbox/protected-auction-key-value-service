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

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "absl/strings/escaping.h"
#include "components/data/common/change_notifier.h"
#include "components/data/common/mocks.h"
#include "components/data/realtime/delta_file_record_change_notifier.h"
#include "gtest/gtest.h"
#include "src/cpp/telemetry/mocks.h"

using testing::_;

namespace kv_server {
namespace {

using privacy_sandbox::server_common::MockMetricsRecorder;

constexpr std::string_view kX64EncodedMessage =
    "g69w0Q2ISj8AAAAAAAAAAEAAAAAAAAAAkbrCPJKH4akAAAAAAAAAAOGfE8DpscNycwAAAAAA"
    "AAAA\nAAAAAAAAANqGdBsDqETKEQAAAAAAAAC5sOpcC5pTLm0AAAAAAAAABQAAAAAAAAAADg"
    "EBAQECA5Ky\nkU0AAgABAMeW/"
    "RFgxGxvLAMAAAAAAABlmmreeUvoa3IFAAAAAAAAIAMAAAAAAAAACqABoAGgAaAB\noAEUAAA"
    "AAAAAAAwAFAAAAAwABAAIAAwAAAB8AAAADAAAADM6dQcAAAAAZAAAAFJSUlJSUlJSUlJS\nU"
    "lJSUlJSUlJSUlJSUlJSUlJSUlJSUlJSUlJSUlJSUlJSUlJSUlJSUlJSUlJSUlJSUlJSUlJSU"
    "lJS\nUlJSUlJSUlJSUlJSUlJSUlJSUlJSUlJSUlJSUlJSUlIAAAAABAAAAGZvbzAAAAAAFAA"
    "AAAAAAAAM\nABQAAAAMAAQACAAMAAAAfAAAAAwAAAAzOnUHAAAAAGQAAABTU1NTU1NTU1NTU"
    "1NTU1NTU1NTU1NT\nU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1N"
    "TU1NTU1NTU1NTU1NTU1NT\nU1NTU1NTU1NTU1NTU1NTU1NTU1NTAAAAAAQAAABmb28xAAAAA"
    "BQAAAAAAAAADAAUAAAADAAEAAgA\nDAAAAHwAAAAMAAAAMzp1BwAAAABkAAAAVFRUVFRUVFR"
    "UVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRU\nVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUV"
    "FRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRU\nVFRUVFRUVFRUVAAAAAAEAAAAZm9vMgA"
    "AAAAUAAAAAAAAAAwAFAAAAAwABAAIAAwAAAB8AAAADAAA\nADM6dQcAAAAAZAAAAFVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV\nVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVUA\nAAAABAAAAGZvb"
    "zMAAAAAFAAAAAAAAAAMABQAAAAMAAQACAAMAAAAfAAAAAwAAAAzOnUHAAAAAGQA\nAABWVlZ"
    "WVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZW\nV"
    "lZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWAAAAAAQAAABmb"
    "280\nAAAAAA==";

class DeltaFileRecordChangeNotifierAwsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_change_notifier_ = std::make_unique<kv_server::MockChangeNotifier>();
  }
  std::unique_ptr<MockChangeNotifier> mock_change_notifier_;
  MockMetricsRecorder mock_metrics_recorder_;
};

TEST_F(DeltaFileRecordChangeNotifierAwsTest, FailureStatusPropagated) {
  EXPECT_CALL(*mock_change_notifier_, GetNotifications(_, _)).WillOnce([]() {
    return absl::InvalidArgumentError("stuff");
  });

  auto delta_file_record_change_notifier =
      DeltaFileRecordChangeNotifier::Create(std::move(mock_change_notifier_),
                                            mock_metrics_recorder_);
  auto status = delta_file_record_change_notifier->GetNotifications(
      absl::Seconds(1), []() { return false; });
  ASSERT_FALSE(status.ok());
}

TEST_F(DeltaFileRecordChangeNotifierAwsTest, MessageWithoutInsertionTime) {
  EXPECT_CALL(*mock_change_notifier_, GetNotifications(_, _)).WillOnce([]() {
    std::ostringstream expected_delta_osstream;
    expected_delta_osstream
        << std::ifstream(
               "components/data/realtime/testdata/proper-message-no-attributes")
               .rdbuf();

    std::vector<std::string> parsed_strings{expected_delta_osstream.str()};
    return parsed_strings;
  });

  auto delta_file_record_change_notifier =
      DeltaFileRecordChangeNotifier::Create(std::move(mock_change_notifier_),
                                            mock_metrics_recorder_);
  auto notifications_context =
      delta_file_record_change_notifier->GetNotifications(
          absl::Seconds(1), []() { return false; });
  ASSERT_TRUE(notifications_context.ok());
  std::string string_decoded;
  absl::Base64Unescape(kX64EncodedMessage, &string_decoded);
  ASSERT_EQ(notifications_context->realtime_messages[0].parsed_notification,
            string_decoded);
  ASSERT_EQ(notifications_context->realtime_messages[0].notifications_inserted,
            std::nullopt);
}

TEST_F(DeltaFileRecordChangeNotifierAwsTest,
       MessageWithInsertionTimePropagated) {
  EXPECT_CALL(*mock_change_notifier_, GetNotifications(_, _)).WillOnce([]() {
    std::ostringstream expected_delta_osstream;
    expected_delta_osstream << std::ifstream(
                                   "components/data/realtime/testdata/"
                                   "proper-message-with-attributes")
                                   .rdbuf();
    std::vector<std::string> parsed_strings{expected_delta_osstream.str()};
    return parsed_strings;
  });

  auto delta_file_record_change_notifier =
      DeltaFileRecordChangeNotifier::Create(std::move(mock_change_notifier_),
                                            mock_metrics_recorder_);
  auto notifications_context =
      delta_file_record_change_notifier->GetNotifications(
          absl::Seconds(1), []() { return false; });
  ASSERT_TRUE(notifications_context.ok());

  std::string string_decoded;
  absl::Base64Unescape(kX64EncodedMessage, &string_decoded);
  ASSERT_EQ(notifications_context->realtime_messages[0].parsed_notification,
            string_decoded);
  ASSERT_EQ(notifications_context->realtime_messages[0].notifications_inserted,
            absl::FromUnixNanos(1677096720486518762));
}

TEST_F(DeltaFileRecordChangeNotifierAwsTest,
       MessageWithAggregatedInsertionTimePropagated) {
  EXPECT_CALL(*mock_change_notifier_, GetNotifications(_, _)).WillOnce([]() {
    std::ostringstream expected_delta_osstream;
    expected_delta_osstream << std::ifstream(
                                   "components/data/realtime/testdata/"
                                   "proper-message-with-attributes")
                                   .rdbuf();

    std::vector<std::string> parsed_strings{expected_delta_osstream.str()};
    std::ostringstream expected_delta_osstream_2;
    expected_delta_osstream_2 << std::ifstream(
                                     "components/data/realtime/testdata/"
                                     "proper-second-message-with-attributes")
                                     .rdbuf();
    parsed_strings.push_back(expected_delta_osstream_2.str());
    return parsed_strings;
  });

  auto delta_file_record_change_notifier =
      DeltaFileRecordChangeNotifier::Create(std::move(mock_change_notifier_),
                                            mock_metrics_recorder_);
  auto notifications_context =
      delta_file_record_change_notifier->GetNotifications(
          absl::Seconds(1), []() { return false; });
  ASSERT_TRUE(notifications_context.ok());
  std::string string_decoded;
  absl::Base64Unescape(kX64EncodedMessage, &string_decoded);
  ASSERT_EQ(notifications_context->realtime_messages[0].parsed_notification,
            string_decoded);
  ASSERT_EQ(notifications_context->realtime_messages[0].notifications_inserted,
            absl::FromUnixNanos(1677096720486518762));
}

}  // namespace
}  // namespace kv_server
