// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/container/thread_safe_hash_map.h"

#include <cstdint>
#include <future>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

TEST(ThreadSafeHashMapTest, VerifyCGet) {
  ThreadSafeHashMap<int32_t, int32_t> map;
  auto node = map.CGet(10);
  EXPECT_FALSE(node.is_present());
  map.PutIfAbsent(10, 20);
  node = map.CGet(10);
  ASSERT_TRUE(node.is_present());
  EXPECT_EQ(*node.key(), 10);
  EXPECT_EQ(*node.value(), 20);
}

TEST(ThreadSafeHashMapTest, VerifyGet) {
  ThreadSafeHashMap<std::string, int32_t> map;
  {
    auto node = map.Get("key");
    EXPECT_FALSE(node.is_present());
  }
  for (auto i : std::vector<int32_t>{1, 2, 3, 4, 5}) {
    std::string key = absl::StrCat("key", i);
    map.PutIfAbsent(key, i);
    auto node = map.Get(key);
    ASSERT_TRUE(node.is_present());
    EXPECT_EQ(*node.key(), key);
    EXPECT_EQ(*node.value(), i);
  }
}

TEST(ThreadSafeHashMapTest, VerifyPutIfAbsent) {
  ThreadSafeHashMap<std::string, std::string> map;
  {
    auto result = map.PutIfAbsent("key", "value");
    EXPECT_TRUE(result.second);
    EXPECT_EQ(*result.first.value(), "value");
  }
  {
    auto result = map.PutIfAbsent("key", "not applied");
    EXPECT_FALSE(result.second);
    EXPECT_EQ(*result.first.value(), "value");
  }
}

TEST(ThreadSafeHashMapTest, VerifyRemoveIf) {
  ThreadSafeHashMap<int64_t, std::string> map;
  map.PutIfAbsent(10, "value");
  {
    auto node = map.CGet(10);
    ASSERT_TRUE(node.is_present());
    EXPECT_EQ(*node.key(), 10);
    EXPECT_EQ(*node.value(), "value");
  }
  map.RemoveIf(10,
               [](const std::string& value) { return value == "wrong value"; });
  {
    auto node = map.CGet(10);
    ASSERT_TRUE(node.is_present());
    EXPECT_EQ(*node.key(), 10);
    EXPECT_EQ(*node.value(), "value");
  }
  map.RemoveIf(10, [](const std::string& value) { return value == "value"; });
  {
    auto node = map.CGet(10);
    ASSERT_FALSE(node.is_present());
  }
}

TEST(ThreadSafeHashMapTest, VerifyIteration) {
  ThreadSafeHashMap<std::string, std::string> map;
  for (const auto& value :
       std::vector<std::string>{"one", "two", "three", "four", "five"}) {
    map.PutIfAbsent(value, value);
  }
  std::vector<std::string> values;
  for (auto& node : map) {
    values.push_back(*node.value());
  }
  EXPECT_THAT(values,
              UnorderedElementsAre("one", "two", "three", "four", "five"));
}

TEST(ThreadSafeHashMapTest, VerifyMutableLockedValue) {
  ThreadSafeHashMap<std::string, std::string> map;
  map.PutIfAbsent("key", "value");
  {
    auto node = map.Get("key");
    ASSERT_TRUE(node.is_present());
    EXPECT_EQ(*node.value(), "value");
    // Let's modify value in place.
    *node.value() = "modified";
  }
  {
    auto node = map.Get("key");
    ASSERT_TRUE(node.is_present());
    EXPECT_EQ(*node.value(), "modified");
  }
}

TEST(ThreadSafeHashMapTest, VerifyMoveOnlyValues) {
  ThreadSafeHashMap<std::string, std::unique_ptr<std::string>> map;
  std::string_view key = "key";
  {
    auto node = map.PutIfAbsent(key, std::make_unique<std::string>("value"));
    EXPECT_TRUE(node.second);
  }
  {
    auto node = map.CGet(key);
    EXPECT_TRUE(node.is_present());
    EXPECT_THAT(*node.key(), key);
    EXPECT_THAT(**node.value(), "value");
  }
}

TEST(ThreadSafeHashMapTest, VerifyMultiThreadedWritesToSimpleType) {
  ThreadSafeHashMap<int32_t, int32_t> map;
  auto key = 10;
  map.PutIfAbsent(key, 0);
  auto incr = [key, &map]() {
    auto node = map.Get(key);
    int32_t* value = node.value();
    *value = *value + 1;
  };
  int num_tasks = 100;
  std::vector<std::future<void>> tasks;
  tasks.reserve(num_tasks);
  for (int t = 0; t < num_tasks; t++) {
    tasks.push_back(std::async(std::launch::async, incr));
  }
  for (auto& task : tasks) {
    task.get();
  }
  auto node = map.CGet(key);
  EXPECT_EQ(*node.value(), num_tasks);
}

TEST(ThreadSafeHashMapTest, VerifyMultiThreadedWritesToComplexType) {
  ThreadSafeHashMap<int32_t, std::vector<int32_t>> map;
  int32_t key = 10;
  std::size_t size = 1000;
  map.PutIfAbsent(key, std::vector<int32_t>(size, 0));
  auto incr = [key, &map]() {
    auto node = map.Get(key);
    auto* values = node.value();
    for (int i = 0; i < values->size(); i++) {
      (*values)[i] = (*values)[i] + 1;
    }
  };
  int num_tasks = 100;
  std::vector<std::future<void>> tasks;
  tasks.reserve(num_tasks);
  for (int t = 0; t < num_tasks; t++) {
    tasks.push_back(std::async(std::launch::async, incr));
  }
  for (auto& task : tasks) {
    task.get();
  }
  std::vector<int32_t> expected(size, num_tasks);
  auto node = map.CGet(key);
  EXPECT_THAT(*node.value(),
              UnorderedElementsAreArray(expected.begin(), expected.end()));
}

}  // namespace
}  // namespace kv_server
