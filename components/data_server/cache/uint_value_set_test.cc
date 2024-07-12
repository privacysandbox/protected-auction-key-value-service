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

#include "components/data_server/cache/uint_value_set.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

using testing::UnorderedElementsAre;

TEST(UInt32ValueSet, VerifyAddingValues) {
  UInt32ValueSet value_set;
  auto values = std::vector<uint32_t>{1, 2, 3, 4, 5};
  value_set.Add(absl::MakeSpan(values), 1);
  EXPECT_THAT(value_set.GetValues(), UnorderedElementsAre(1, 2, 3, 4, 5));
  EXPECT_EQ(value_set.GetValuesBitSet(), roaring::Roaring({1, 2, 3, 4, 5}));
}

TEST(UInt32ValueSet, VerifyBitSetToUint32Set) {
  roaring::Roaring bitset({1, 2, 3, 4, 5});
  EXPECT_THAT(BitSetToUint32Set(bitset), UnorderedElementsAre(1, 2, 3, 4, 5));
}

TEST(UInt32ValueSet, VerifyRemovingValues) {
  UInt32ValueSet value_set;
  auto values = std::vector<uint32_t>{1, 2, 3, 4, 5};
  value_set.Add(absl::MakeSpan(values), 1);
  values.erase(values.begin());
  values.erase(values.begin());
  // Should not remove anything because logical_commit_time did not change.
  value_set.Remove(absl::MakeSpan(values), 1);
  EXPECT_THAT(value_set.GetValues(), UnorderedElementsAre(1, 2, 3, 4, 5));
  EXPECT_EQ(value_set.GetValuesBitSet(), roaring::Roaring({1, 2, 3, 4, 5}));
  // Should now remove: {3, 4, 5}.
  value_set.Remove(absl::MakeSpan(values), 2);
  EXPECT_THAT(value_set.GetValues(), UnorderedElementsAre(1, 2));
  EXPECT_EQ(value_set.GetValuesBitSet(), roaring::Roaring({1, 2}));
}

TEST(UInt32ValueSet, VerifyCleaningUpValues) {
  UInt32ValueSet value_set;
  auto values = std::vector<uint32_t>{1, 2, 3, 4, 5};
  value_set.Add(absl::MakeSpan(values), 1);
  values.erase(values.begin());
  values.erase(values.begin());
  value_set.Remove(absl::MakeSpan(values), 2);
  EXPECT_THAT(value_set.GetRemovedValues(), UnorderedElementsAre(3, 4, 5));
  value_set.Cleanup(1);  // Does nothing.
  EXPECT_THAT(value_set.GetRemovedValues(), UnorderedElementsAre(3, 4, 5));
  value_set.Cleanup(2);
  EXPECT_TRUE(value_set.GetRemovedValues().empty());
}

}  // namespace
}  // namespace kv_server
