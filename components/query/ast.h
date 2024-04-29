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
#include "absl/functional/bind_front.h"
#include "components/query/sets.h"

namespace kv_server {
class ASTStackVisitor;
class ASTStringVisitor;

// All set operations operate on a reference to the data in the DB
// This means that the data in the DB must be locked throughout the lifetime of
// the result.
using KVSetView = absl::flat_hash_set<std::string_view>;

class Node {
 public:
  virtual ~Node() = default;
  virtual Node* Left() const { return nullptr; }
  virtual Node* Right() const { return nullptr; }
  // Return all Keys associated with ValueNodes in the tree.
  virtual absl::flat_hash_set<std::string_view> Keys() const = 0;
  // Uses the Visitor pattern for the concrete class
  // to mutate the stack accordingly for `Eval` (ValueNode vs. OpNode)
  virtual void Accept(ASTStackVisitor& visitor,
                      std::vector<KVSetView>& stack) const = 0;
  virtual std::string Accept(ASTStringVisitor& visitor) const = 0;
};

// The value associated with a `ValueNode` is the set with its associated `key`.
class ValueNode : public Node {
 public:
  explicit ValueNode(std::string key);
  std::string_view Key() const { return key_; }
  absl::flat_hash_set<std::string_view> Keys() const override;
  void Accept(ASTStackVisitor& visitor,
              std::vector<KVSetView>& stack) const override;
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
  // Computes the operation over the `left` and `right` nodes.
  virtual KVSetView Op(KVSetView left, KVSetView right) const = 0;
  void Accept(ASTStackVisitor& visitor,
              std::vector<KVSetView>& stack) const override;

 private:
  std::unique_ptr<Node> left_;
  std::unique_ptr<Node> right_;
};

class UnionNode : public OpNode {
 public:
  using OpNode::Accept;
  using OpNode::OpNode;
  inline KVSetView Op(KVSetView left, KVSetView right) const override {
    return Union(std::move(left), std::move(right));
  }
  std::string Accept(ASTStringVisitor& visitor) const override;
};

class IntersectionNode : public OpNode {
 public:
  using OpNode::Accept;
  using OpNode::OpNode;
  inline KVSetView Op(KVSetView left, KVSetView right) const override {
    return Intersection(std::move(left), std::move(right));
  }
  std::string Accept(ASTStringVisitor& visitor) const override;
};

class DifferenceNode : public OpNode {
 public:
  using OpNode::Accept;
  using OpNode::OpNode;
  inline KVSetView Op(KVSetView left, KVSetView right) const override {
    return Difference(std::move(left), std::move(right));
  }
  std::string Accept(ASTStringVisitor& visitor) const override;
};

// Creates execution plan and runs it.
KVSetView Eval(const Node& node,
               absl::AnyInvocable<absl::flat_hash_set<std::string_view>(
                   std::string_view key) const>
                   lookup_fn);

// Responsible for mutating the stack with the given `Node`.
// Avoids downcasting for subclass specific behaviors.
class ASTStackVisitor {
 public:
  explicit ASTStackVisitor(
      absl::AnyInvocable<
          absl::flat_hash_set<std::string_view>(std::string_view key) const>
          lookup_fn)
      : lookup_fn_(std::move(lookup_fn)) {}

  // Applies the operation to the top two values on the stack.
  // Replaces the top two values with the result.
  void Visit(const OpNode& node, std::vector<KVSetView>& stack);
  // Pushes the result of `Lookup` to the stack.
  void Visit(const ValueNode& node, std::vector<KVSetView>& stack);

 private:
  absl::AnyInvocable<absl::flat_hash_set<std::string_view>(std::string_view key)
                         const>
      lookup_fn_;
};

// General purpose Vistor capable of returning a string representation of a Node
// upon inspection.
class ASTStringVisitor {
 public:
  virtual std::string Visit(const UnionNode&) = 0;
  virtual std::string Visit(const DifferenceNode&) = 0;
  virtual std::string Visit(const IntersectionNode&) = 0;
  virtual std::string Visit(const ValueNode&) = 0;
};

}  // namespace kv_server
#endif  // COMPONENTS_QUERY_AST_H_
