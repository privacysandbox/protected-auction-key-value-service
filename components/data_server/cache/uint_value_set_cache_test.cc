/*
 * Copyright 2024 Google LLC
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

#include "components/data_server/cache/uint_value_set_cache.h"

#include <limits>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {

class SafePathTestLogContext
    : public privacy_sandbox::server_common::log::SafePathContext {
 public:
  SafePathTestLogContext() = default;
};

class UIntValueSetCacheTest : public ::testing::Test {
 public:
  UIntValueSetCacheTest() {
    InitMetricsContextMap();
    request_context_ = std::make_shared<RequestContext>();
  }

  template <typename SetType>
  static const auto& ReadDeletedNodes(UIntValueSetCache<SetType>& cache,
                                      std::string_view prefix = "") {
    return cache.deleted_sets_map_;
  }

  std::shared_ptr<RequestContext> request_context_;
  SafePathTestLogContext safe_path_log_context_;
};

namespace {

using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

template <typename SetType>
const SetType* GetValueSet(std::string_view key,
                           GetKeyValueSetResult& key_value_result) {
  if constexpr (std::is_same_v<UInt32ValueSet, SetType>) {
    return key_value_result.GetUInt32ValueSet(key);
  }
  if constexpr (std::is_same_v<UInt64ValueSet, SetType>) {
    return key_value_result.GetUInt64ValueSet(key);
  }
}

template <typename SetType>
void VerifyUpdatingSets(std::shared_ptr<RequestContext> request_context,
                        SafePathTestLogContext& safe_path_log_context) {
  UIntValueSetCache<SetType> cache;
  const auto keys = absl::flat_hash_set<std::string_view>({"set1", "set2"});
  {
    auto result = cache.GetValueSet(*request_context, keys);
    for (const auto& key : keys) {
      const auto* set = GetValueSet<SetType>(key, *result);
      EXPECT_EQ(set, nullptr);
    }
  }
  auto max = std::numeric_limits<typename SetType::value_type>::max();
  auto set1_values = std::vector<typename SetType::value_type>(
      {max - 1, max - 2, max - 3, max - 4, max - 5});
  auto logical_commit_time = 1;
  {
    // For uint64 sets, if we errorneously store values in uint32 sets, then
    // this test would catch the overflow.
    cache.UpdateSetValues(safe_path_log_context, "set1",
                          absl::MakeSpan(set1_values), logical_commit_time);
    auto result = cache.GetValueSet(*request_context, keys);
    const auto* set = GetValueSet<SetType>("set1", *result);
    ASSERT_TRUE(set != nullptr);
    EXPECT_THAT(set->GetValues(), UnorderedElementsAreArray(set1_values));
  }
  auto set2_values = std::vector<typename SetType::value_type>(
      {max - 6, max - 7, max - 8, max - 9, max - 10});
  {
    cache.UpdateSetValues(safe_path_log_context, "set1",
                          absl::MakeSpan(set2_values), logical_commit_time);
    auto result = cache.GetValueSet(*request_context, keys);
    const auto* set = GetValueSet<SetType>("set1", *result);
    set1_values.insert(set1_values.end(), set2_values.begin(),
                       set2_values.end());
    ASSERT_TRUE(set != nullptr);
    EXPECT_THAT(set->GetValues(), UnorderedElementsAreArray(set1_values));
  }
}

template <typename SetType>
void VerifyDeletingSets(std::shared_ptr<RequestContext> request_context,
                        SafePathTestLogContext& safe_path_log_context) {
  UIntValueSetCache<SetType> cache;
  const auto keys = absl::flat_hash_set<std::string_view>({"set1", "set2"});
  const auto max = std::numeric_limits<typename SetType::value_type>::max();
  auto delete_values = std::vector<typename SetType::value_type>(
      {max - 1, max - 2, max - 6, max - 7});
  {
    auto set1_values = std::vector<typename SetType::value_type>(
        {max - 1, max - 2, max - 3, max - 4, max - 5});
    cache.UpdateSetValues(safe_path_log_context, "set1",
                          absl::MakeSpan(set1_values), 1);
    cache.DeleteSetValues(safe_path_log_context, "set1",
                          absl::MakeSpan(delete_values), 2);
    auto result = cache.GetValueSet(*request_context, keys);
    const auto* set = GetValueSet<SetType>("set1", *result);
    ASSERT_TRUE(set != nullptr);
    EXPECT_THAT(set->GetValues(),
                UnorderedElementsAre(max - 3, max - 4, max - 5));
  }
  {
    auto set2_values = std::vector<typename SetType::value_type>(
        {max - 6, max - 7, max - 8, max - 9, max - 10});
    cache.UpdateSetValues(safe_path_log_context, "set2",
                          absl::MakeSpan(set2_values), 1);
    cache.DeleteSetValues(safe_path_log_context, "set2",
                          absl::MakeSpan(delete_values), 2);
    auto result = cache.GetValueSet(*request_context, keys);
    const auto* set = GetValueSet<SetType>("set2", *result);
    ASSERT_TRUE(set != nullptr);
    EXPECT_THAT(set->GetValues(),
                UnorderedElementsAre(max - 8, max - 9, max - 10));
  }
}

template <typename SetType>
void VerifyCleaningUpSets(std::shared_ptr<RequestContext> request_context,
                          SafePathTestLogContext& safe_path_log_context) {
  UIntValueSetCache<SetType> cache;
  const auto keys = absl::flat_hash_set<std::string_view>({"set1"});
  const auto max = std::numeric_limits<typename SetType::value_type>::max();
  auto set1_values = std::vector<typename SetType::value_type>(
      {max - 1, max - 2, max - 3, max - 4, max - 5});
  auto delete_values =
      std::vector<typename SetType::value_type>({max - 1, max - 2});
  {
    cache.UpdateSetValues(safe_path_log_context, "set1",
                          absl::MakeSpan(set1_values), 1);
    cache.DeleteSetValues(safe_path_log_context, "set1",
                          absl::MakeSpan(delete_values), 2);
    auto result = cache.GetValueSet(*request_context, keys);
    const auto* set = GetValueSet<SetType>("set1", *result);
    ASSERT_TRUE(set != nullptr);
    EXPECT_THAT(set->GetValues(),
                UnorderedElementsAre(max - 3, max - 4, max - 5));
    EXPECT_THAT(set->GetRemovedValues(),
                UnorderedElementsAreArray(delete_values));
  }
  const auto& deleted_nodes =
      UIntValueSetCacheTest::ReadDeletedNodes<SetType>(cache);
  {
    auto prefix_nodes = deleted_nodes.CGet("");
    ASSERT_TRUE(prefix_nodes.is_present());
    auto iter = prefix_nodes.value()->find(2);
    ASSERT_NE(iter, prefix_nodes.value()->end());
    EXPECT_THAT(iter->first, 2);
    EXPECT_TRUE(iter->second.contains("set1"));
  }
  {
    cache.CleanUpValueSets(safe_path_log_context, 3);
    auto prefix_nodes = deleted_nodes.CGet("");
    ASSERT_TRUE(prefix_nodes.is_present());
    auto iter = prefix_nodes.value()->find(2);
    ASSERT_EQ(iter, prefix_nodes.value()->end());
  }
  {
    auto result = cache.GetValueSet(*request_context, keys);
    const auto* set = GetValueSet<SetType>("set1", *result);
    ASSERT_TRUE(set != nullptr);
    EXPECT_THAT(set->GetValues(),
                UnorderedElementsAre(max - 3, max - 4, max - 5));
    EXPECT_TRUE(set->GetRemovedValues().empty());
  }
}

TEST_F(UIntValueSetCacheTest, VerifyUpdatingSets) {
  VerifyUpdatingSets<UInt32ValueSet>(request_context_, safe_path_log_context_);
  VerifyUpdatingSets<UInt64ValueSet>(request_context_, safe_path_log_context_);
}

TEST_F(UIntValueSetCacheTest, VerifyDeletingSets) {
  VerifyDeletingSets<UInt32ValueSet>(request_context_, safe_path_log_context_);
  VerifyDeletingSets<UInt64ValueSet>(request_context_, safe_path_log_context_);
}

TEST_F(UIntValueSetCacheTest, VerifyCleaningUpSets) {
  VerifyCleaningUpSets<UInt32ValueSet>(request_context_,
                                       safe_path_log_context_);
  VerifyCleaningUpSets<UInt64ValueSet>(request_context_,
                                       safe_path_log_context_);
}

}  // namespace
}  // namespace kv_server
