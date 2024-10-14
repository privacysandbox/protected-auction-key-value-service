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

#include "components/query/sets.h"

#include <utility>

#include "components/data_server/cache/uint_value_set.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

TEST(SetsTest, VerifyBitwiseUnion) {
  UInt32ValueSet::bitset_type left({1, 2, 3, 4, 5});
  UInt32ValueSet::bitset_type right({6, 7, 8, 9, 10});
  EXPECT_EQ(Union(std::move(left), std::move(right)),
            UInt32ValueSet::bitset_type({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
}

TEST(SetsTest, VerifyBitwiseIntersection) {
  {
    UInt32ValueSet::bitset_type left({1, 2, 3, 4, 5});
    UInt32ValueSet::bitset_type right({6, 7, 8, 9, 10});
    EXPECT_EQ(Intersection(std::move(left), std::move(right)),
              UInt32ValueSet::bitset_type());
  }
  {
    UInt32ValueSet::bitset_type left({1, 2, 3, 4, 5});
    UInt32ValueSet::bitset_type right({1, 2, 3, 9, 10});
    EXPECT_EQ(Intersection(std::move(left), std::move(right)),
              UInt32ValueSet::bitset_type({1, 2, 3}));
  }
}

TEST(SetsTest, VerifyBitwiseDifference) {
  {
    UInt32ValueSet::bitset_type left({1, 2, 3, 4, 5});
    UInt32ValueSet::bitset_type right({6, 7, 8, 9, 10});
    EXPECT_EQ(Difference(std::move(left), std::move(right)),
              UInt32ValueSet::bitset_type({1, 2, 3, 4, 5}));
  }
  {
    UInt32ValueSet::bitset_type left({1, 2, 3, 4, 5});
    UInt32ValueSet::bitset_type right({1, 2, 3, 9, 10});
    EXPECT_EQ(Difference(std::move(left), std::move(right)),
              UInt32ValueSet::bitset_type({4, 5}));
  }
}

}  // namespace
}  // namespace kv_server
