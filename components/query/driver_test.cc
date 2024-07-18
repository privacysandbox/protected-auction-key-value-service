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
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/notification.h"
#include "components/query/scanner.h"
#include "components/query/template_test_utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "roaring.hh"
#include "roaring64map.hh"

namespace kv_server {
namespace {

const absl::flat_hash_map<std::string, absl::flat_hash_set<std::string_view>>
    kStringSetDB = {
        {"A", {"a", "b", "c"}},
        {"B", {"b", "c", "d"}},
        {"C", {"c", "d", "e"}},
        {"D", {"d", "e", "f"}},
};

const absl::flat_hash_map<std::string, roaring::Roaring> kUInt32SetDb = {
    {"A", {1, 2, 3}},
    {"B", {2, 3, 4}},
    {"C", {3, 4, 5}},
    {"D", {4, 5, 6}},
};

const absl::flat_hash_map<std::string, roaring::Roaring64Map> kUInt64SetDb = {
    {"A",
     {18446744073709551609UL, 18446744073709551610UL, 18446744073709551611UL}},
    {"B",
     {18446744073709551610UL, 18446744073709551611UL, 18446744073709551612UL}},
    {"C",
     {18446744073709551611UL, 18446744073709551612UL, 18446744073709551613UL}},
    {"D",
     {18446744073709551612UL, 18446744073709551613UL, 18446744073709551614UL}},
};

template <typename SetType>
SetType GetExpectations(std::vector<ConvertedSetType<SetType>> elems);

template <>
absl::flat_hash_set<std::string_view> GetExpectations(
    std::vector<std::string_view> expected) {
  absl::flat_hash_set<std::string_view> views;
  for (auto e : expected) {
    if (e.front() == '"' && e.back() == '"') {
      views.insert(e.substr(1, e.size() - 2));
    }
  }
  return views;
}

template <>
roaring::Roaring GetExpectations(std::vector<uint32_t> expected) {
  return roaring::Roaring(expected.size(), expected.data());
}

template <>
roaring::Roaring64Map GetExpectations(std::vector<uint64_t> expected) {
  return roaring::Roaring64Map(expected.size(), expected.data());
}

template <typename SetType>
SetType Lookup(std::string_view key);

template <>
absl::flat_hash_set<std::string_view> Lookup(std::string_view key) {
  if (const auto& it = kStringSetDB.find(key); it != kStringSetDB.end()) {
    return it->second;
  }
  return {};
}

template <>
roaring::Roaring Lookup(std::string_view key) {
  if (const auto& it = kUInt32SetDb.find(key); it != kUInt32SetDb.end()) {
    return it->second;
  }
  return {};
}

template <>
roaring::Roaring64Map Lookup(std::string_view key) {
  if (const auto& it = kUInt64SetDb.find(key); it != kUInt64SetDb.end()) {
    return it->second;
  }
  return {};
}

template <typename SetType>
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

class NameGenerator {
 public:
  template <typename SetType>
  static std::string GetName(int) {
    if constexpr (std::is_same_v<SetType,
                                 absl::flat_hash_set<std::string_view>>) {
      return "StringSet";
    }
    if constexpr (std::is_same_v<SetType, roaring::Roaring>) {
      return "UInt32Set";
    }
    if constexpr (std::is_same_v<SetType, roaring::Roaring64Map>) {
      return "UInt64Set";
    }
  }
};
using SetTypes = testing::Types<absl::flat_hash_set<std::string_view>,
                                roaring::Roaring, roaring::Roaring64Map>;
TYPED_TEST_SUITE(DriverTest, SetTypes, NameGenerator);

TYPED_TEST(DriverTest, EmptyQuery) {
  this->Parse("");
  EXPECT_EQ(this->driver_->GetRootNode(), nullptr);
  auto result =
      this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(*result, TypeParam());
}

TYPED_TEST(DriverTest, InvalidTokensQuery) {
  this->Parse("!! hi");
  EXPECT_EQ(this->driver_->GetRootNode(), nullptr);
  auto result =
      this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TYPED_TEST(DriverTest, MissingOperatorVar) {
  this->Parse("A A");
  EXPECT_EQ(this->driver_->GetRootNode(), nullptr);
  auto result =
      this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TYPED_TEST(DriverTest, MissingOperatorExp) {
  this->Parse("(A) (A)");
  EXPECT_EQ(this->driver_->GetRootNode(), nullptr);
  auto result =
      this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TYPED_TEST(DriverTest, InvalidOp) {
  this->Parse("A UNION ");
  EXPECT_EQ(this->driver_->GetRootNode(), nullptr);
  auto result =
      this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TYPED_TEST(DriverTest, KeyOnly) {
  this->Parse("A");
  auto result =
      this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  ASSERT_TRUE(result.ok());
  if constexpr (std::is_same_v<TypeParam,
                               absl::flat_hash_set<std::string_view>>) {
    EXPECT_THAT(*result, testing::UnorderedElementsAre("a", "b", "c"));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
    EXPECT_EQ(*result, roaring::Roaring({1, 2, 3}));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
    EXPECT_EQ(*result, roaring::Roaring64Map({18446744073709551609UL,
                                              18446744073709551610UL,
                                              18446744073709551611UL}));
  }

  this->Parse("B");
  result = this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  ASSERT_TRUE(result.ok());
  if constexpr (std::is_same_v<TypeParam,
                               absl::flat_hash_set<std::string_view>>) {
    EXPECT_THAT(*result, testing::UnorderedElementsAre("b", "c", "d"));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
    EXPECT_EQ(*result, roaring::Roaring({2, 3, 4}));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
    EXPECT_EQ(*result, roaring::Roaring64Map({18446744073709551610UL,
                                              18446744073709551611UL,
                                              18446744073709551612UL}));
  }
}

TYPED_TEST(DriverTest, InlineSetOnly) {
  std::vector<ConvertedSetType<TypeParam>> multi_elem;
  ConvertedSetType<TypeParam> elem;
  if constexpr (std::is_same_v<TypeParam,
                               absl::flat_hash_set<std::string_view>>) {
    elem = "\"a\"";
    multi_elem = {"\"a\"", "\"b\""};
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
    elem = 1;
    multi_elem = {2, 3};
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
    elem = 184467440737551610UL;
    multi_elem = {18446744073709551609UL, 18446744073709551610UL};
  }
  this->Parse(absl::StrCat("Set(", elem, ")"));
  auto result =
      this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, GetExpectations<TypeParam>({elem}));

  this->Parse(absl::StrCat("Set(", absl::StrJoin(multi_elem, ","), ")"));
  result = this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, GetExpectations<TypeParam>(multi_elem));
}

TYPED_TEST(DriverTest, InlineIntegerSetTooBig) {
  this->Parse("Set(99999999999999999999)");
  auto result =
      this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  ASSERT_FALSE(result.ok());
}

TYPED_TEST(DriverTest, Union) {
  for (std::string_view query : {"A UNION B", "A | B"}) {
    this->Parse(std::string(query));
    auto result =
        this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
    ASSERT_TRUE(result.ok());
    if constexpr (std::is_same_v<TypeParam,
                                 absl::flat_hash_set<std::string_view>>) {
      EXPECT_THAT(*result, testing::UnorderedElementsAre("a", "b", "c", "d"));
    }
    if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
      EXPECT_EQ(*result, roaring::Roaring({1, 2, 3, 4}));
    }
    if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
      EXPECT_EQ(*result, roaring::Roaring64Map(
                             {18446744073709551609UL, 18446744073709551610UL,
                              18446744073709551611UL, 18446744073709551612UL}));
    }
  }
}

TYPED_TEST(DriverTest, Difference) {
  for (std::string_view query : {"A - B", "A DIFFERENCE B"}) {
    this->Parse(std::string(query));
    auto result =
        this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
    ASSERT_TRUE(result.ok());
    if constexpr (std::is_same_v<TypeParam,
                                 absl::flat_hash_set<std::string_view>>) {
      EXPECT_THAT(*result, testing::UnorderedElementsAre("a"));
    }
    if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
      EXPECT_EQ(*result, roaring::Roaring({1}));
    }
    if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
      EXPECT_EQ(*result, roaring::Roaring64Map({18446744073709551609UL}));
    }
  }
  for (std::string_view query : {"B - A", "B DIFFERENCE A"}) {
    this->Parse(std::string(query));
    auto result =
        this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
    ASSERT_TRUE(result.ok());
    if constexpr (std::is_same_v<TypeParam,
                                 absl::flat_hash_set<std::string_view>>) {
      EXPECT_THAT(*result, testing::UnorderedElementsAre("d"));
    }
    if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
      EXPECT_EQ(*result, roaring::Roaring({4}));
    }
    if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
      EXPECT_EQ(*result, roaring::Roaring64Map({18446744073709551612UL}));
    }
  }
}

TYPED_TEST(DriverTest, Intersection) {
  for (std::string_view query : {"A & B", "A INTERSECTION B"}) {
    this->Parse(std::string(query));
    auto result =
        this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
    ASSERT_TRUE(result.ok());
    if constexpr (std::is_same_v<TypeParam,
                                 absl::flat_hash_set<std::string_view>>) {
      EXPECT_THAT(*result, testing::UnorderedElementsAre("b", "c"));
    }
    if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
      EXPECT_EQ(*result, roaring::Roaring({2, 3}));
    }
    if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
      EXPECT_EQ(*result, roaring::Roaring64Map(
                             {18446744073709551610UL, 18446744073709551611UL}));
    }
  }
}

TYPED_TEST(DriverTest, OrderOfOperations) {
  this->Parse("A - B - C");
  auto result =
      this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  ASSERT_TRUE(result.ok());
  if constexpr (std::is_same_v<TypeParam,
                               absl::flat_hash_set<std::string_view>>) {
    EXPECT_THAT(*result, testing::UnorderedElementsAre("a"));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
    EXPECT_EQ(*result, roaring::Roaring({1}));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
    EXPECT_EQ(*result, roaring::Roaring64Map({18446744073709551609UL}));
  }

  this->Parse("A - (B - C)");
  result = this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  ASSERT_TRUE(result.ok());
  if constexpr (std::is_same_v<TypeParam,
                               absl::flat_hash_set<std::string_view>>) {
    EXPECT_THAT(*result, testing::UnorderedElementsAre("a", "c"));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
    EXPECT_EQ(*result, roaring::Roaring({1, 3}));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
    EXPECT_EQ(*result, roaring::Roaring64Map(
                           {18446744073709551609UL, 18446744073709551611UL}));
  }
}

TYPED_TEST(DriverTest, MultipleOperations) {
  this->Parse("(A-B) | (C&D)");
  auto result =
      this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  ASSERT_TRUE(result.ok());
  if constexpr (std::is_same_v<TypeParam,
                               absl::flat_hash_set<std::string_view>>) {
    EXPECT_THAT(*result, testing::UnorderedElementsAre("a", "d", "e"));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
    EXPECT_EQ(*result, roaring::Roaring({1, 4, 5}));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
    EXPECT_EQ(*result, roaring::Roaring64Map({18446744073709551609UL,
                                              18446744073709551612UL,
                                              18446744073709551613UL}));
  }
}

TYPED_TEST(DriverTest, MultipleThreads) {
  absl::Notification notification;
  auto test_func = [&notification](Driver* driver) {
    notification.WaitForNotification();
    std::string query = "(A-B) | (C&D)";
    std::istringstream stream(query);
    Scanner scanner(stream);
    Parser parse(*driver, scanner);
    parse();
    auto result = driver->EvaluateQuery<TypeParam>(Lookup<TypeParam>);
    ASSERT_TRUE(result.ok());
    if constexpr (std::is_same_v<TypeParam,
                                 absl::flat_hash_set<std::string_view>>) {
      EXPECT_THAT(*result, testing::UnorderedElementsAre("a", "d", "e"));
    }
    if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
      EXPECT_EQ(*result, roaring::Roaring({1, 4, 5}));
    }
    if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
      EXPECT_EQ(*result, roaring::Roaring64Map({18446744073709551609UL,
                                                18446744073709551612UL,
                                                18446744073709551613UL}));
    }
  };

  std::vector<std::thread> threads;
  for (Driver& driver : this->drivers_) {
    threads.push_back(std::thread(test_func, &driver));
  }
  notification.Notify();
  for (auto& th : threads) {
    th.join();
  }
}

TYPED_TEST(DriverTest, EmptyResults) {
  // no overlap
  this->Parse("A & D");
  auto result =
      this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  ASSERT_TRUE(result.ok());
  if constexpr (std::is_same_v<TypeParam,
                               absl::flat_hash_set<std::string_view>>) {
    EXPECT_EQ(result->size(), 0);
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
    EXPECT_EQ(*result, roaring::Roaring());
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
    EXPECT_EQ(*result, roaring::Roaring64Map());
  }
  // missing key
  this->Parse("A & E");
  result = this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  ASSERT_TRUE(result.ok());
  if constexpr (std::is_same_v<TypeParam,
                               absl::flat_hash_set<std::string_view>>) {
    EXPECT_EQ(result->size(), 0);
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
    EXPECT_EQ(*result, roaring::Roaring());
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
    EXPECT_EQ(*result, roaring::Roaring64Map());
  }
}

TYPED_TEST(DriverTest, DriverErrorsClearedOnParse) {
  this->Parse("A &");
  auto result =
      this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  ASSERT_FALSE(result.ok());
  this->Parse("A");
  result = this->driver_->template EvaluateQuery<TypeParam>(Lookup<TypeParam>);
  ASSERT_TRUE(result.ok());
}

}  // namespace
}  // namespace kv_server
