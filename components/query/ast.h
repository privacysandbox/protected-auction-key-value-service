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
#include "components/query/sets.h"

namespace kv_server {
// All set operations using `KVStringSetView` operate on a reference to the data
// in the DB This means that the data in the DB must be locked throughout the
// lifetime of the result.
using KVStringSetView = absl::flat_hash_set<std::string_view>;

class ASTVisitor;
class ASTStringVisitor;

class Node {
 public:
  virtual ~Node() = default;
  virtual Node* Left() const { return nullptr; }
  virtual Node* Right() const { return nullptr; }
  // Return all Keys associated with ValueNodes in the tree.
  virtual absl::flat_hash_set<std::string_view> Keys() const = 0;
  virtual void Accept(ASTVisitor& visitor) const = 0;
  virtual std::string Accept(ASTStringVisitor& visitor) const = 0;
};

// The value associated with a `ValueNode` is the set with its associated `key`.
class ValueNode : public Node {
 public:
  explicit ValueNode(std::string key) : key_(std::move(key)) {}
  std::string_view Key() const { return key_; }
  absl::flat_hash_set<std::string_view> Keys() const override;
  void Accept(ASTVisitor& visitor) const override;
  std::string Accept(ASTStringVisitor& visitor) const override;

 private:
  std::string key_;
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
  void Accept(ASTVisitor& visitor) const override;
  std::string Accept(ASTStringVisitor& visitor) const override;
};

class IntersectionNode : public OpNode {
 public:
  using OpNode::OpNode;
  void Accept(ASTVisitor& visitor) const override;
  std::string Accept(ASTStringVisitor& visitor) const override;
};

class DifferenceNode : public OpNode {
 public:
  using OpNode::OpNode;
  void Accept(ASTVisitor& visitor) const override;
  std::string Accept(ASTStringVisitor& visitor) const override;
};

// Traverses the binary tree starting at root and returns a vector of `Node`s in
// post order. Represents the infix input as postfix which can then be more
// easily evaluated.
std::vector<const Node*> ComputePostfixOrder(const Node* root);

// General purpose Vistor capable of returning a string representation of a Node
// upon inspection.
class ASTStringVisitor {
 public:
  virtual std::string Visit(const UnionNode&) = 0;
  virtual std::string Visit(const DifferenceNode&) = 0;
  virtual std::string Visit(const IntersectionNode&) = 0;
  virtual std::string Visit(const ValueNode&) = 0;
};

// Defines a general AST visitor interface which can be extended to implement
// concrete ast algorithms, e.g., ast evaluation.
class ASTVisitor {
 public:
  // Entrypoint for running the visitor algorithm on a given AST tree, `root`.
  virtual void ConductVisit(const Node& root) = 0;
  virtual void Visit(const ValueNode& node) = 0;
  virtual void Visit(const UnionNode& node) = 0;
  virtual void Visit(const DifferenceNode& node) = 0;
  virtual void Visit(const IntersectionNode& node) = 0;
};

// Implements AST tree evaluation using iterative post order processing.
template <typename ValueT>
class ASTPostOrderEvalVisitor final : public ASTVisitor {
 public:
  explicit ASTPostOrderEvalVisitor(
      absl::AnyInvocable<ValueT(std::string_view) const> lookup_fn)
      : lookup_fn_(std::move(lookup_fn)) {}

  void ConductVisit(const Node& root) override {
    stack_.clear();
    for (const auto* node : ComputePostfixOrder(&root)) {
      node->Accept(*this);
    }
  }

  void Visit(const ValueNode& node) override {
    stack_.push_back(std::move(lookup_fn_(node.Key())));
  }
  void Visit(const UnionNode& node) override { Visit(node, Union<ValueT>); }

  void Visit(const DifferenceNode& node) override {
    Visit(node, Difference<ValueT>);
  }

  void Visit(const IntersectionNode& node) override {
    Visit(node, Intersection<ValueT>);
  }

  ValueT GetResult() {
    if (stack_.empty()) {
      return ValueT();
    }
    return stack_.back();
  }

 private:
  void Visit(const OpNode& node,
             absl::AnyInvocable<ValueT(ValueT&&, ValueT&&)> op_fn) {
    auto right = std::move(stack_.back());
    stack_.pop_back();
    auto left = std::move(stack_.back());
    stack_.pop_back();
    stack_.push_back(op_fn(std::move(left), std::move(right)));
  }

  absl::AnyInvocable<ValueT(std::string_view) const> lookup_fn_;
  std::vector<ValueT> stack_;
};

// Accepts an AST representing a set query, creates execution plan and runs it.
template <typename ValueT>
ValueT Eval(const Node& node,
            absl::AnyInvocable<ValueT(std::string_view) const> lookup_fn) {
  auto visitor = ASTPostOrderEvalVisitor<ValueT>(std::move(lookup_fn));
  visitor.ConductVisit(node);
  return visitor.GetResult();
}

}  // namespace kv_server
#endif  // COMPONENTS_QUERY_AST_H_
