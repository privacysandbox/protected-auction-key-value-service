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

#include "components/data/blob_storage/delta_file_notifier.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/synchronization/notification.h"
#include "components/data/common/mocks.h"
#include "components/util/sleepfor_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "public/data_loading/filename_utils.h"

using testing::_;
using testing::Field;
using testing::Return;

namespace kv_server {
namespace {

using privacy_sandbox::server_common::SimulatedSteadyClock;

class DeltaFileNotifierTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<MockSleepFor> mock_sleep_for =
        std::make_unique<MockSleepFor>();
    sleep_for_ = mock_sleep_for.get();
    notifier_ = DeltaFileNotifier::Create(
        client_, poll_frequency_, std::move(mock_sleep_for), sim_clock_);
  }

  MockBlobStorageClient client_;
  std::unique_ptr<DeltaFileNotifier> notifier_;
  MockBlobStorageChangeNotifier change_notifier_;
  std::string initial_key_ = ToDeltaFileName(1).value();
  MockSleepFor* sleep_for_;
  SimulatedSteadyClock sim_clock_;
  absl::Duration poll_frequency_ = absl::Minutes(5);
};

TEST_F(DeltaFileNotifierTest, NotRunning) {
  ASSERT_FALSE(notifier_->IsRunning());
}

TEST_F(DeltaFileNotifierTest, StartFailure) {
  BlobStorageClient::DataLocation location = {.bucket = "testbucket"};
  absl::Status status =
      notifier_->Start(change_notifier_, {.bucket = "testbucket"}, initial_key_,
                       [](const std::string&) {});
  ASSERT_TRUE(status.ok());
  status = notifier_->Start(change_notifier_, {.bucket = "testbucket"},
                            initial_key_, [](const std::string&) {});
  ASSERT_FALSE(status.ok());
}

TEST_F(DeltaFileNotifierTest, StartsAndStops) {
  BlobStorageClient::DataLocation location = {.bucket = "testbucket"};
  absl::Status status =
      notifier_->Start(change_notifier_, {.bucket = "testbucket"}, initial_key_,
                       [](const std::string&) {});
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(notifier_->IsRunning());
  status = notifier_->Stop();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE(notifier_->IsRunning());
}

TEST_F(DeltaFileNotifierTest, NotifiesWithNewFiles) {
  BlobStorageClient::DataLocation location = {.bucket = "testbucket"};
  EXPECT_CALL(change_notifier_, GetNotifications(_, _))
      .WillOnce(Return(std::vector<std::string>({ToDeltaFileName(3).value()})))
      .WillOnce(Return(std::vector<std::string>({ToDeltaFileName(4).value()})))
      .WillRepeatedly(Return(std::vector<std::string>()));
  EXPECT_CALL(
      client_,
      ListBlobs(Field(&BlobStorageClient::DataLocation::bucket, "testbucket"),
                Field(&BlobStorageClient::ListOptions::start_after,
                      ToDeltaFileName(1).value())))
      .WillOnce(Return(std::vector<std::string>({})))
      .WillOnce(Return(std::vector<std::string>({ToDeltaFileName(3).value()})));
  EXPECT_CALL(
      client_,
      ListBlobs(Field(&BlobStorageClient::DataLocation::bucket, "testbucket"),
                Field(&BlobStorageClient::ListOptions::start_after,
                      ToDeltaFileName(3).value())))
      .WillOnce(Return(std::vector<std::string>({ToDeltaFileName(4).value()})));

  absl::Notification finished;
  testing::MockFunction<void(const std::string& record)> callback;
  EXPECT_CALL(callback, Call)
      .Times(2)
      .WillOnce([&](const std::string& key) {
        EXPECT_EQ(key, ToDeltaFileName(3).value());
      })
      .WillOnce([&](const std::string& key) {
        EXPECT_EQ(key, ToDeltaFileName(4).value());
        finished.Notify();
      });

  absl::Status status =
      notifier_->Start(change_notifier_, {.bucket = "testbucket"}, initial_key_,
                       callback.AsStdFunction());
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(notifier_->IsRunning());
  finished.WaitForNotification();
  status = notifier_->Stop();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE(notifier_->IsRunning());
}

TEST_F(DeltaFileNotifierTest, NotifiesWithInvalidFilesIngored) {
  BlobStorageClient::DataLocation location = {.bucket = "testbucket"};
  std::string invalid_delta_name = "DELTA_5";
  EXPECT_CALL(change_notifier_, GetNotifications(_, _))
      .WillOnce(Return(std::vector<std::string>({ToDeltaFileName(3).value()})))
      .WillOnce(Return(std::vector<std::string>({invalid_delta_name})))
      .WillOnce(Return(std::vector<std::string>({ToDeltaFileName(4).value()})))
      .WillOnce(Return(std::vector<std::string>(
          {invalid_delta_name, ToDeltaFileName(5).value()})))
      .WillRepeatedly(Return(std::vector<std::string>()));
  EXPECT_CALL(
      client_,
      ListBlobs(Field(&BlobStorageClient::DataLocation::bucket, "testbucket"),
                Field(&BlobStorageClient::ListOptions::start_after,
                      ToDeltaFileName(1).value())))
      .WillOnce(Return(std::vector<std::string>({})))
      .WillOnce(Return(std::vector<std::string>(
          {ToDeltaFileName(3).value(), invalid_delta_name})));
  EXPECT_CALL(
      client_,
      ListBlobs(Field(&BlobStorageClient::DataLocation::bucket, "testbucket"),
                Field(&BlobStorageClient::ListOptions::start_after,
                      ToDeltaFileName(3).value())))
      .WillOnce(Return(std::vector<std::string>({ToDeltaFileName(4).value()})));
  EXPECT_CALL(
      client_,
      ListBlobs(Field(&BlobStorageClient::DataLocation::bucket, "testbucket"),
                Field(&BlobStorageClient::ListOptions::start_after,
                      ToDeltaFileName(4).value())))
      .WillOnce(Return(std::vector<std::string>({ToDeltaFileName(5).value()})));

  absl::Notification finished;
  testing::MockFunction<void(const std::string& record)> callback;
  EXPECT_CALL(callback, Call)
      .Times(3)
      .WillOnce([&](const std::string& key) {
        EXPECT_EQ(key, ToDeltaFileName(3).value());
      })
      .WillOnce([&](const std::string& key) {
        EXPECT_EQ(key, ToDeltaFileName(4).value());
      })
      .WillOnce([&](const std::string& key) {
        EXPECT_EQ(key, ToDeltaFileName(5).value());
        finished.Notify();
      });

  absl::Status status =
      notifier_->Start(change_notifier_, {.bucket = "testbucket"}, initial_key_,
                       callback.AsStdFunction());
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(notifier_->IsRunning());
  finished.WaitForNotification();
  status = notifier_->Stop();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE(notifier_->IsRunning());
}

TEST_F(DeltaFileNotifierTest, GetChangesFailure) {
  BlobStorageClient::DataLocation location = {.bucket = "testbucket"};
  EXPECT_CALL(change_notifier_, GetNotifications(_, _))
      .WillOnce(Return(absl::InvalidArgumentError("stuff")))
      .WillOnce(Return(absl::InvalidArgumentError("stuff")))
      .WillOnce(Return(std::vector<std::string>({ToDeltaFileName(1).value()})))
      .WillRepeatedly(Return(std::vector<std::string>()));
  EXPECT_CALL(
      client_,
      ListBlobs(Field(&BlobStorageClient::DataLocation::bucket, "testbucket"),
                Field(&BlobStorageClient::ListOptions::start_after,
                      ToDeltaFileName(1).value())))
      .WillOnce(Return(std::vector<std::string>({})))
      .WillOnce(Return(std::vector<std::string>({ToDeltaFileName(1).value()})));

  absl::Notification finished;
  testing::MockFunction<void(const std::string& record)> callback;
  EXPECT_CALL(callback, Call).Times(1).WillOnce([&](const std::string& key) {
    EXPECT_EQ(key, ToDeltaFileName(1).value());
    finished.Notify();
  });
  EXPECT_CALL(*sleep_for_, Duration(absl::Seconds(2)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*sleep_for_, Duration(absl::Seconds(4)))
      .Times(1)
      .WillOnce(Return(true));

  absl::Status status =
      notifier_->Start(change_notifier_, {.bucket = "testbucket"}, initial_key_,
                       callback.AsStdFunction());
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(notifier_->IsRunning());
  finished.WaitForNotification();
  status = notifier_->Stop();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE(notifier_->IsRunning());
}

TEST_F(DeltaFileNotifierTest, BackupPoll) {
  EXPECT_CALL(change_notifier_, GetNotifications(_, _))
      .WillOnce(Return(absl::DeadlineExceededError("too long")))
      .WillRepeatedly(Return(std::vector<std::string>()));
  EXPECT_CALL(
      client_,
      ListBlobs(Field(&BlobStorageClient::DataLocation::bucket, "testbucket"),
                Field(&BlobStorageClient::ListOptions::start_after,
                      ToDeltaFileName(1).value())))
      .WillOnce(Return(std::vector<std::string>({ToDeltaFileName(2).value()})));
  EXPECT_CALL(
      client_,
      ListBlobs(Field(&BlobStorageClient::DataLocation::bucket, "testbucket"),
                Field(&BlobStorageClient::ListOptions::start_after,
                      ToDeltaFileName(2).value())))
      .WillOnce(Return(std::vector<std::string>({ToDeltaFileName(3).value()})));
  EXPECT_CALL(
      client_,
      ListBlobs(Field(&BlobStorageClient::DataLocation::bucket, "testbucket"),
                Field(&BlobStorageClient::ListOptions::start_after,
                      ToDeltaFileName(3).value())))
      .WillOnce(Return(std::vector<std::string>({ToDeltaFileName(4).value()})));

  absl::Notification finished;
  testing::MockFunction<void(const std::string& record)> callback;
  EXPECT_CALL(callback, Call)
      .Times(3)
      .WillOnce([&](const std::string& key) {
        // Initial poll
        EXPECT_EQ(key, ToDeltaFileName(2).value());
        sim_clock_.AdvanceTime(poll_frequency_ + absl::Seconds(1));
      })
      .WillOnce([&](const std::string& key) {
        // Backup poll due to expired flag
        EXPECT_EQ(key, ToDeltaFileName(3).value());
      })
      .WillOnce([&](const std::string& key) {
        // Backup poll due to WaitForNotification returning DeadlineExceeded
        EXPECT_EQ(key, ToDeltaFileName(4).value());
        finished.Notify();
      });

  absl::Status status =
      notifier_->Start(change_notifier_, {.bucket = "testbucket"}, initial_key_,
                       callback.AsStdFunction());
  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(notifier_->IsRunning());
  finished.WaitForNotification();
  status = notifier_->Stop();
  ASSERT_TRUE(status.ok());
  EXPECT_FALSE(notifier_->IsRunning());
}

}  // namespace
}  // namespace kv_server
