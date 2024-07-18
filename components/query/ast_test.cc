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

#include "components/query/ast.h"

#include <limits>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
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

// Copied from driver_test
// TODO: move to a shared header.
template <typename T>
struct SetTypeConverter;

template <>
struct SetTypeConverter<absl::flat_hash_set<std::string_view>> {
  using type = std::string_view;
};

template <>
struct SetTypeConverter<roaring::Roaring> {
  using type = uint32_t;
};

template <>
struct SetTypeConverter<roaring::Roaring64Map> {
  using type = uint64_t;
};

template <typename T>
using ConvertedSetType = typename SetTypeConverter<T>::type;

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

template <typename SetType>
class ASTTest : public ::testing::Test {};

using SetTypes = testing::Types<absl::flat_hash_set<std::string_view>,
                                roaring::Roaring, roaring::Roaring64Map>;
TYPED_TEST_SUITE(ASTTest, SetTypes, NameGenerator);

TYPED_TEST(ASTTest, Value) {
  ValueNode value("A");
  EXPECT_EQ(Eval<TypeParam>(value, Lookup<TypeParam>), Lookup<TypeParam>("A"));
  ValueNode value2("B");
  EXPECT_EQ(Eval<TypeParam>(value2, Lookup<TypeParam>), Lookup<TypeParam>("B"));
  ValueNode value3("C");
  EXPECT_EQ(Eval<TypeParam>(value3, Lookup<TypeParam>), Lookup<TypeParam>("C"));
  ValueNode value4("D");
  EXPECT_EQ(Eval<TypeParam>(value4, Lookup<TypeParam>), Lookup<TypeParam>("D"));
  ValueNode value5("E");
  EXPECT_EQ(Eval<TypeParam>(value5, Lookup<TypeParam>), Lookup<TypeParam>("E"));
}

TYPED_TEST(ASTTest, Set) {
  StringViewSetNode ssn({"a", "b", "c"});
  auto str_result = Eval<TypeParam>(ssn, Lookup<TypeParam>);
  if constexpr (std::is_same_v<TypeParam,
                               absl::flat_hash_set<std::string_view>>) {
    EXPECT_THAT(str_result, testing::UnorderedElementsAre("a", "b", "c"));
  } else {
    // For number evals, we expect string sets to result as an empty set
    // See TODO for eval to return an error.
    EXPECT_EQ(str_result, decltype(str_result)());
  }

  if constexpr (std::is_same_v<TypeParam,
                               absl::flat_hash_set<std::string_view>>) {
    // For string evals, we expect strings to result as an empty set
    // See TODO for eval to return an error.
    auto result = Eval<TypeParam>(NumberSetNode({1, 2, 3}), Lookup<TypeParam>);
    EXPECT_THAT(result, decltype(result)());
  } else {
    std::vector<ConvertedSetType<TypeParam>> vals = {
        0, std::numeric_limits<ConvertedSetType<TypeParam>>::max(),
        std::numeric_limits<ConvertedSetType<TypeParam>>::max() - 1,
        std::numeric_limits<ConvertedSetType<TypeParam>>::max() - 2};
    // Currently no bounds checking on 64-bit type fits into 32-bit range
    // for 32-bit Roaring eval type.
    // See TODO for eval to return an error.
    NumberSetNode nsn({vals.begin(), vals.end()});
    auto num_result = Eval<TypeParam>(nsn, Lookup<TypeParam>);
    decltype(num_result) expected(vals.size(), vals.data());
    EXPECT_EQ(num_result, expected);
  }
}

TYPED_TEST(ASTTest, Union) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>("A");
  std::unique_ptr<ValueNode> b = std::make_unique<ValueNode>("B");
  UnionNode op(std::move(a), std::move(b));
  auto result = Eval<TypeParam>(op, Lookup<TypeParam>);
  if constexpr (std::is_same_v<TypeParam,
                               absl::flat_hash_set<std::string_view>>) {
    EXPECT_THAT(result, testing::UnorderedElementsAre("a", "b", "c", "d"));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
    EXPECT_EQ(result, roaring::Roaring({1, 2, 3, 4}));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
    EXPECT_EQ(result, roaring::Roaring64Map(
                          {18446744073709551609UL, 18446744073709551610UL,
                           18446744073709551611UL, 18446744073709551612UL}));
  }
}

TYPED_TEST(ASTTest, UnionSelf) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>("A");
  std::unique_ptr<ValueNode> a2 = std::make_unique<ValueNode>("A");
  UnionNode op(std::move(a), std::move(a2));
  EXPECT_EQ(Eval<TypeParam>(op, Lookup<TypeParam>), Lookup<TypeParam>("A"));
}

TYPED_TEST(ASTTest, Intersection) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>("A");
  std::unique_ptr<ValueNode> b = std::make_unique<ValueNode>("B");
  IntersectionNode op(std::move(a), std::move(b));
  auto result = Eval<TypeParam>(op, Lookup<TypeParam>);
  if constexpr (std::is_same_v<TypeParam,
                               absl::flat_hash_set<std::string_view>>) {
    EXPECT_THAT(result, testing::UnorderedElementsAre("b", "c"));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
    EXPECT_EQ(result, roaring::Roaring({2, 3}));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
    EXPECT_EQ(result, roaring::Roaring64Map(
                          {18446744073709551610UL, 18446744073709551611UL}));
  }
}

TYPED_TEST(ASTTest, IntersectionSelf) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>("A");
  std::unique_ptr<ValueNode> a2 = std::make_unique<ValueNode>("A");
  IntersectionNode op(std::move(a), std::move(a2));
  EXPECT_EQ(Eval<TypeParam>(op, Lookup<TypeParam>), Lookup<TypeParam>("A"));
}

TYPED_TEST(ASTTest, Difference) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>("A");
  std::unique_ptr<ValueNode> b = std::make_unique<ValueNode>("B");
  DifferenceNode op(std::move(a), std::move(b));
  auto result = Eval<TypeParam>(op, Lookup<TypeParam>);
  if constexpr (std::is_same_v<TypeParam,
                               absl::flat_hash_set<std::string_view>>) {
    EXPECT_THAT(result, testing::UnorderedElementsAre("a"));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
    EXPECT_EQ(result, roaring::Roaring({1}));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
    EXPECT_EQ(result, roaring::Roaring64Map({18446744073709551609UL}));
  }

  std::unique_ptr<ValueNode> a2 = std::make_unique<ValueNode>("A");
  std::unique_ptr<ValueNode> b2 = std::make_unique<ValueNode>("B");
  DifferenceNode op2(std::move(b2), std::move(a2));
  result = Eval<TypeParam>(op2, Lookup<TypeParam>);
  if constexpr (std::is_same_v<TypeParam,
                               absl::flat_hash_set<std::string_view>>) {
    EXPECT_THAT(result, testing::UnorderedElementsAre("d"));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
    EXPECT_EQ(result, roaring::Roaring({4}));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
    EXPECT_EQ(result, roaring::Roaring64Map({18446744073709551612UL}));
  }
}

TYPED_TEST(ASTTest, DifferenceSelf) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>("A");
  std::unique_ptr<ValueNode> a2 = std::make_unique<ValueNode>("A");
  DifferenceNode op(std::move(a), std::move(a2));
  EXPECT_EQ(Eval<TypeParam>(op, Lookup<TypeParam>), TypeParam());
}

TYPED_TEST(ASTTest, All) {
  // (A-B) | (C&D) =
  // {a} | {d,e} =
  // {a, d, e}
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>("A");
  std::unique_ptr<ValueNode> b = std::make_unique<ValueNode>("B");
  std::unique_ptr<ValueNode> c = std::make_unique<ValueNode>("C");
  std::unique_ptr<ValueNode> d = std::make_unique<ValueNode>("D");
  std::unique_ptr<DifferenceNode> left =
      std::make_unique<DifferenceNode>(std::move(a), std::move(b));
  std::unique_ptr<IntersectionNode> right =
      std::make_unique<IntersectionNode>(std::move(c), std::move(d));
  UnionNode center(std::move(left), std::move(right));
  auto result = Eval<TypeParam>(center, Lookup<TypeParam>);
  if constexpr (std::is_same_v<TypeParam,
                               absl::flat_hash_set<std::string_view>>) {
    EXPECT_THAT(result, testing::UnorderedElementsAre("a", "d", "e"));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring>) {
    EXPECT_EQ(result, roaring::Roaring({1, 4, 5}));
  }
  if constexpr (std::is_same_v<TypeParam, roaring::Roaring64Map>) {
    EXPECT_EQ(result, roaring::Roaring64Map({18446744073709551609UL,
                                             18446744073709551612UL,
                                             18446744073709551613UL}));
  }
}

TEST(ASTKeysTest, ValueNodeKeys) {
  ValueNode v("A");
  EXPECT_THAT(v.Keys(), testing::UnorderedElementsAre("A"));
}

TEST(ASTKeysTest, OpNodeKeys) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>("A");
  std::unique_ptr<ValueNode> b = std::make_unique<ValueNode>("B");
  DifferenceNode op(std::move(b), std::move(a));
  EXPECT_THAT(op.Keys(), testing::UnorderedElementsAre("A", "B"));
}

TEST(ASTKeysTest, DupeNodeKeys) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>("A");
  std::unique_ptr<ValueNode> b = std::make_unique<ValueNode>("B");
  std::unique_ptr<ValueNode> c = std::make_unique<ValueNode>("C");
  std::unique_ptr<ValueNode> a2 = std::make_unique<ValueNode>("A");
  std::unique_ptr<DifferenceNode> left =
      std::make_unique<DifferenceNode>(std::move(a), std::move(b));
  std::unique_ptr<IntersectionNode> right =
      std::make_unique<IntersectionNode>(std::move(c), std::move(a2));
  UnionNode center(std::move(left), std::move(right));
  EXPECT_THAT(center.Keys(), testing::UnorderedElementsAre("A", "B", "C"));
}

}  // namespace
}  // namespace kv_server
