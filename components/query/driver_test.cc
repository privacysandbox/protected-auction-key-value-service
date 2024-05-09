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

#include "components/query/driver.h"

#include <thread>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/notification.h"
#include "components/query/scanner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

const absl::flat_hash_map<std::string, absl::flat_hash_set<std::string_view>>
    kStringSetDB = {
        {"A", {"a", "b", "c"}},
        {"B", {"b", "c", "d"}},
        {"C", {"c", "d", "e"}},
        {"D", {"d", "e", "f"}},
};

absl::flat_hash_set<std::string_view> Lookup(std::string_view key) {
  if (const auto& it = kStringSetDB.find(key); it != kStringSetDB.end()) {
    return it->second;
  }
  return {};
}

class DriverTest : public ::testing::Test {
 protected:
  void SetUp() override {
    driver_ = std::make_unique<Driver>();
    for (int i = 1000; i < 1; i++) {
      drivers_.emplace_back();
    }
  }

  void Parse(const std::string& query) {
    std::istringstream stream(query);
    Scanner scanner(stream);
    Parser parse(*driver_, scanner);
    parse();
  }

  std::unique_ptr<Driver> driver_;
  std::vector<Driver> drivers_;
};

TEST_F(DriverTest, EmptyQuery) {
  Parse("");
  EXPECT_EQ(driver_->GetRootNode(), nullptr);
  auto result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  absl::flat_hash_set<std::string_view> expected;
  EXPECT_EQ(*result, expected);
}

TEST_F(DriverTest, InvalidTokensQuery) {
  Parse("!! hi");
  EXPECT_EQ(driver_->GetRootNode(), nullptr);
  auto result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(DriverTest, MissingOperatorVar) {
  Parse("A A");
  EXPECT_EQ(driver_->GetRootNode(), nullptr);
  auto result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(DriverTest, MissingOperatorExp) {
  Parse("(A) (A)");
  EXPECT_EQ(driver_->GetRootNode(), nullptr);
  auto result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(DriverTest, InvalidOp) {
  Parse("A UNION ");
  EXPECT_EQ(driver_->GetRootNode(), nullptr);
  auto result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(DriverTest, KeyOnly) {
  Parse("A");
  auto result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, testing::UnorderedElementsAre("a", "b", "c"));

  Parse("B");
  result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, testing::UnorderedElementsAre("b", "c", "d"));
}

TEST_F(DriverTest, Union) {
  Parse("A UNION B");
  auto result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, testing::UnorderedElementsAre("a", "b", "c", "d"));

  Parse("A | B");
  result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, testing::UnorderedElementsAre("a", "b", "c", "d"));
}

TEST_F(DriverTest, Difference) {
  Parse("A - B");
  auto result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, testing::UnorderedElementsAre("a"));

  Parse("A DIFFERENCE B");
  result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, testing::UnorderedElementsAre("a"));

  Parse("B - A");
  result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, testing::UnorderedElementsAre("d"));

  Parse("B DIFFERENCE A");
  result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, testing::UnorderedElementsAre("d"));
}

TEST_F(DriverTest, Intersection) {
  Parse("A INTERSECTION B");
  auto result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, testing::UnorderedElementsAre("b", "c"));

  Parse("A & B");
  result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, testing::UnorderedElementsAre("b", "c"));
}

TEST_F(DriverTest, OrderOfOperations) {
  Parse("A - B - C");
  auto result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, testing::UnorderedElementsAre("a"));

  Parse("A - (B - C)");
  result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, testing::UnorderedElementsAre("a", "c"));
}

TEST_F(DriverTest, MultipleOperations) {
  Parse("(A-B) | (C&D)");
  auto result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, testing::UnorderedElementsAre("a", "d", "e"));
}

TEST_F(DriverTest, MultipleThreads) {
  absl::Notification notification;
  auto test_func = [&notification](Driver* driver) {
    notification.WaitForNotification();
    std::string query = "(A-B) | (C&D)";
    std::istringstream stream(query);
    Scanner scanner(stream);
    Parser parse(*driver, scanner);
    parse();
    auto result =
        driver->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
    ASSERT_TRUE(result.ok());
    EXPECT_THAT(*result, testing::UnorderedElementsAre("a", "d", "e"));
  };

  std::vector<std::thread> threads;
  for (Driver& driver : drivers_) {
    threads.push_back(std::thread(test_func, &driver));
  }
  notification.Notify();
  for (auto& th : threads) {
    th.join();
  }
}

TEST_F(DriverTest, EmptyResults) {
  // no overlap
  Parse("A & D");
  auto result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->size(), 0);

  // missing key
  Parse("A & E");
  result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->size(), 0);
}

TEST_F(DriverTest, DriverErrorsClearedOnParse) {
  Parse("A &");
  auto result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_FALSE(result.ok());
  Parse("A");
  result =
      driver_->EvaluateQuery<absl::flat_hash_set<std::string_view>>(Lookup);
  ASSERT_TRUE(result.ok());
}

}  // namespace
}  // namespace kv_server
