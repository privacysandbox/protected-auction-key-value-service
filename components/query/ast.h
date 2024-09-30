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

#ifndef COMPONENTS_QUERY_AST_H_
#define COMPONENTS_QUERY_AST_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/query/sets.h"
#include "src/util/status_macro/status_macros.h"

#include "roaring.hh"
#include "roaring64map.hh"

namespace kv_server {
// All set operations using `KVStringSetView` operate on a reference to the data
// in the DB This means that the data in the DB must be locked throughout the
// lifetime of the result.
using KVStringSetView = absl::flat_hash_set<std::string_view>;

class ASTVisitor;
class ASTStringVisitor;

// SFINAE with std::void_t
template <typename, typename = std::void_t<>>
struct has_value_type : std::false_type {};

template <typename T>
struct has_value_type<T, std::void_t<typename T::value_type>> : std::true_type {
};

template <typename T>
inline constexpr bool has_value_type_v = has_value_type<T>::value;

class Node {
 public:
  virtual ~Node() = default;
  virtual Node* Left() const { return nullptr; }
  virtual Node* Right() const { return nullptr; }
  // Return all Keys associated with ValueNodes in the tree.
  virtual absl::flat_hash_set<std::string_view> Keys() const = 0;
  virtual absl::Status Accept(ASTVisitor& visitor) const = 0;
  virtual absl::StatusOr<std::string> Accept(
      ASTStringVisitor& visitor) const = 0;
};

// The value associated with a `ValueNode` is the set with its associated `key`.
class ValueNode : public Node {
 public:
  explicit ValueNode(std::string key) : key_(std::move(key)) {}
  std::string_view Key() const { return key_; }
  absl::flat_hash_set<std::string_view> Keys() const override;
  absl::Status Accept(ASTVisitor& visitor) const override;
  absl::StatusOr<std::string> Accept(ASTStringVisitor& visitor) const override;

 private:
  std::string key_;
};

template <typename T>
class SetNode : public Node {
 public:
  using value_type = T;

  explicit SetNode(std::vector<T> values)
      : values_(values.begin(), values.end()) {}
  absl::flat_hash_set<std::string_view> Keys() const override { return {}; };
  // TODO(b/353502448): Consider changing Vistor argument
  // from const-ref to value.  Then we can return an r-value and avoid copy.
  const absl::flat_hash_set<T>& GetValues() const { return values_; }

 private:
  absl::flat_hash_set<T> values_;
};

class NumberSetNode : public SetNode<uint64_t> {
 public:
  using SetNode<uint64_t>::SetNode;
  absl::Status Accept(ASTVisitor& visitor) const override;
  absl::StatusOr<std::string> Accept(ASTStringVisitor& visitor) const override;
};

// View to strings who's lifetime is managed externally,
// typically the `Driver`.
class StringViewSetNode : public SetNode<std::string_view> {
 public:
  using SetNode<std::string_view>::SetNode;
  absl::Status Accept(ASTVisitor& visitor) const override;
  absl::StatusOr<std::string> Accept(ASTStringVisitor& visitor) const override;
};

class OpNode : public Node {
 public:
  OpNode(std::unique_ptr<Node> left, std::unique_ptr<Node> right)
      : left_(std::move(left)), right_(std::move(right)) {}
  absl::flat_hash_set<std::string_view> Keys() const override;
  inline Node* Left() const override { return left_.get(); }
  inline Node* Right() const override { return right_.get(); }

 private:
  std::unique_ptr<Node> left_;
  std::unique_ptr<Node> right_;
};

class UnionNode : public OpNode {
 public:
  using OpNode::OpNode;
  absl::Status Accept(ASTVisitor& visitor) const override;
  absl::StatusOr<std::string> Accept(ASTStringVisitor& visitor) const override;
};

class IntersectionNode : public OpNode {
 public:
  using OpNode::OpNode;
  absl::Status Accept(ASTVisitor& visitor) const override;
  absl::StatusOr<std::string> Accept(ASTStringVisitor& visitor) const override;
};

class DifferenceNode : public OpNode {
 public:
  using OpNode::OpNode;
  absl::Status Accept(ASTVisitor& visitor) const override;
  absl::StatusOr<std::string> Accept(ASTStringVisitor& visitor) const override;
};

// Traverses the binary tree starting at root and returns a vector of `Node`s in
// post order. Represents the infix input as postfix which can then be more
// easily evaluated.
std::vector<const Node*> ComputePostfixOrder(const Node* root);

// General purpose Vistor capable of returning a string representation of a Node
// upon inspection.
class ASTStringVisitor {
 public:
  virtual absl::StatusOr<std::string> Visit(const UnionNode&) = 0;
  virtual absl::StatusOr<std::string> Visit(const DifferenceNode&) = 0;
  virtual absl::StatusOr<std::string> Visit(const IntersectionNode&) = 0;
  virtual absl::StatusOr<std::string> Visit(const ValueNode&) = 0;
  virtual absl::StatusOr<std::string> Visit(const NumberSetNode&) = 0;
  virtual absl::StatusOr<std::string> Visit(const StringViewSetNode&) = 0;
};

// Defines a general AST visitor interface which can be extended to implement
// concrete ast algorithms, e.g., ast evaluation.
class ASTVisitor {
 public:
  // Entrypoint for running the visitor algorithm on a given AST tree, `root`.
  virtual ~ASTVisitor() = default;
  virtual absl::Status ConductVisit(const Node& root) = 0;
  virtual absl::Status Visit(const ValueNode& node) = 0;
  virtual absl::Status Visit(const UnionNode& node) = 0;
  virtual absl::Status Visit(const DifferenceNode& node) = 0;
  virtual absl::Status Visit(const IntersectionNode& node) = 0;
  virtual absl::Status Visit(const NumberSetNode& node) = 0;
  virtual absl::Status Visit(const StringViewSetNode& node) = 0;
};

// Implements AST tree evaluation using iterative post order processing.
template <typename ValueT>
class ASTPostOrderEvalVisitor final : public ASTVisitor {
 public:
  explicit ASTPostOrderEvalVisitor(
      absl::AnyInvocable<ValueT(std::string_view) const> lookup_fn)
      : lookup_fn_(std::move(lookup_fn)) {}

  absl::Status ConductVisit(const Node& root) override {
    stack_.clear();
    for (const auto* node : ComputePostfixOrder(&root)) {
      PS_RETURN_IF_ERROR(node->Accept(*this));
    }
    return absl::OkStatus();
  }

  absl::Status Visit(const ValueNode& node) override {
    stack_.push_back(std::move(lookup_fn_(node.Key())));
    return absl::OkStatus();
  }
  absl::Status Visit(const UnionNode& node) override {
    return Visit(node, Union<ValueT>);
  }

  absl::Status Visit(const DifferenceNode& node) override {
    return Visit(node, Difference<ValueT>);
  }

  absl::Status Visit(const IntersectionNode& node) override {
    return Visit(node, Intersection<ValueT>);
  }

  absl::Status Visit(const NumberSetNode& node) override {
    if constexpr (std::is_same_v<ValueT, roaring::Roaring> ||
                  std::is_same_v<ValueT, roaring::Roaring64Map>) {
      ValueT r;
      for (const auto v : node.GetValues()) {
        r.add(v);
      }
      stack_.push_back(std::move(r));
      return absl::OkStatus();
    }
    return absl::InvalidArgumentError("Unexpected set type");
  }

  absl::Status Visit(const StringViewSetNode& node) override {
    if constexpr (has_value_type_v<ValueT>) {
      if constexpr (std::is_same_v<StringViewSetNode::value_type,
                                   typename ValueT::value_type>) {
        stack_.push_back(node.GetValues());
        return absl::OkStatus();
      }
    }
    return absl::InvalidArgumentError("Unexpected set type");
  }

  ValueT GetResult() {
    if (stack_.empty()) {
      return ValueT();
    }
    return stack_.back();
  }

 private:
  absl::Status Visit(const OpNode& node,
                     absl::AnyInvocable<ValueT(ValueT&&, ValueT&&)> op_fn) {
    auto right = std::move(stack_.back());
    stack_.pop_back();
    auto left = std::move(stack_.back());
    stack_.pop_back();
    stack_.push_back(op_fn(std::move(left), std::move(right)));
    return absl::OkStatus();
  }

  absl::AnyInvocable<ValueT(std::string_view) const> lookup_fn_;
  std::vector<ValueT> stack_;
};

// Accepts an AST representing a set query, creates execution plan and runs it.
template <typename ValueT>
absl::StatusOr<ValueT> Eval(
    const Node& node,
    absl::AnyInvocable<ValueT(std::string_view) const> lookup_fn) {
  auto visitor = ASTPostOrderEvalVisitor<ValueT>(std::move(lookup_fn));
  PS_RETURN_IF_ERROR(visitor.ConductVisit(node));
  return visitor.GetResult();
}

}  // namespace kv_server
#endif  // COMPONENTS_QUERY_AST_H_
