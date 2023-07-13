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

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

const absl::flat_hash_map<std::string, absl::flat_hash_set<std::string_view>>
    kDb = {
        {"A", {"a", "b", "c"}},
        {"B", {"b", "c", "d"}},
        {"C", {"c", "d", "e"}},
        {"D", {"d", "e", "f"}},
};

absl::flat_hash_set<std::string_view> Lookup(std::string_view key) {
  const auto& it = kDb.find(key);
  if (it != kDb.end()) {
    return it->second;
  }
  return {};
}

TEST(AstTest, Value) {
  ValueNode value(Lookup, "A");
  EXPECT_EQ(Eval(value), Lookup("A"));
  ValueNode value2(Lookup, "B");
  EXPECT_EQ(Eval(value2), Lookup("B"));
  ValueNode value3(Lookup, "C");
  EXPECT_EQ(Eval(value3), Lookup("C"));
  ValueNode value4(Lookup, "D");
  EXPECT_EQ(Eval(value4), Lookup("D"));
  ValueNode value5(Lookup, "E");
  EXPECT_EQ(Eval(value5), Lookup("E"));
}

TEST(AstTest, Union) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>(Lookup, "A");
  std::unique_ptr<ValueNode> b = std::make_unique<ValueNode>(Lookup, "B");
  UnionNode op(std::move(a), std::move(b));
  absl::flat_hash_set<std::string_view> expected = {"a", "b", "c", "d"};
  EXPECT_EQ(Eval(op), expected);
}

TEST(AstTest, UnionSelf) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>(Lookup, "A");
  std::unique_ptr<ValueNode> a2 = std::make_unique<ValueNode>(Lookup, "A");
  UnionNode op(std::move(a), std::move(a2));
  absl::flat_hash_set<std::string_view> expected = {"a", "b", "c"};
  EXPECT_EQ(Eval(op), expected);
}

TEST(AstTest, Intersection) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>(Lookup, "A");
  std::unique_ptr<ValueNode> b = std::make_unique<ValueNode>(Lookup, "B");
  IntersectionNode op(std::move(a), std::move(b));
  absl::flat_hash_set<std::string_view> expected = {"b", "c"};
  EXPECT_EQ(Eval(op), expected);
}

TEST(AstTest, IntersectionSelf) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>(Lookup, "A");
  std::unique_ptr<ValueNode> a2 = std::make_unique<ValueNode>(Lookup, "A");
  IntersectionNode op(std::move(a), std::move(a2));
  absl::flat_hash_set<std::string_view> expected = {"a", "b", "c"};
  EXPECT_EQ(Eval(op), expected);
}

TEST(AstTest, Difference) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>(Lookup, "A");
  std::unique_ptr<ValueNode> b = std::make_unique<ValueNode>(Lookup, "B");
  DifferenceNode op(std::move(a), std::move(b));
  absl::flat_hash_set<std::string_view> expected = {"a"};
  EXPECT_EQ(Eval(op), expected);

  std::unique_ptr<ValueNode> a2 = std::make_unique<ValueNode>(Lookup, "A");
  std::unique_ptr<ValueNode> b2 = std::make_unique<ValueNode>(Lookup, "B");
  DifferenceNode op2(std::move(b2), std::move(a2));
  absl::flat_hash_set<std::string_view> expected2 = {"d"};
  EXPECT_EQ(Eval(op2), expected2);
}

TEST(AstTest, DifferenceSelf) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>(Lookup, "A");
  std::unique_ptr<ValueNode> a2 = std::make_unique<ValueNode>(Lookup, "A");
  DifferenceNode op(std::move(a), std::move(a2));
  absl::flat_hash_set<std::string_view> expected = {};
  EXPECT_EQ(Eval(op), expected);
}

TEST(AstTest, All) {
  // (A-B) | (C&D) =
  // {a} | {d,e} =
  // {a, d, e}
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>(Lookup, "A");
  std::unique_ptr<ValueNode> b = std::make_unique<ValueNode>(Lookup, "B");
  std::unique_ptr<ValueNode> c = std::make_unique<ValueNode>(Lookup, "C");
  std::unique_ptr<ValueNode> d = std::make_unique<ValueNode>(Lookup, "D");
  std::unique_ptr<DifferenceNode> left =
      std::make_unique<DifferenceNode>(std::move(a), std::move(b));
  std::unique_ptr<IntersectionNode> right =
      std::make_unique<IntersectionNode>(std::move(c), std::move(d));
  UnionNode center(std::move(left), std::move(right));
  absl::flat_hash_set<std::string_view> expected = {"a", "d", "e"};
  EXPECT_EQ(Eval(center), expected);
}

TEST(AstTest, ValueNodeKeys) {
  ValueNode v(Lookup, "A");
  EXPECT_THAT(v.Keys(), testing::UnorderedElementsAre("A"));
}

TEST(AstTest, OpNodeKeys) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>(Lookup, "A");
  std::unique_ptr<ValueNode> b = std::make_unique<ValueNode>(Lookup, "B");
  DifferenceNode op(std::move(b), std::move(a));
  EXPECT_THAT(op.Keys(), testing::UnorderedElementsAre("A", "B"));
}

TEST(AstTest, DupeNodeKeys) {
  std::unique_ptr<ValueNode> a = std::make_unique<ValueNode>(Lookup, "A");
  std::unique_ptr<ValueNode> b = std::make_unique<ValueNode>(Lookup, "B");
  std::unique_ptr<ValueNode> c = std::make_unique<ValueNode>(Lookup, "C");
  std::unique_ptr<ValueNode> a2 = std::make_unique<ValueNode>(Lookup, "A");
  std::unique_ptr<DifferenceNode> left =
      std::make_unique<DifferenceNode>(std::move(a), std::move(b));
  std::unique_ptr<IntersectionNode> right =
      std::make_unique<IntersectionNode>(std::move(c), std::move(a2));
  UnionNode center(std::move(left), std::move(right));
  EXPECT_THAT(center.Keys(), testing::UnorderedElementsAre("A", "B", "C"));
}

}  // namespace
}  // namespace kv_server
