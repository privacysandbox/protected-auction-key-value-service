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

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "components/query/sets.h"

namespace kv_server {

namespace {
// Traverses the binary tree starting at root.
// Returns a vector of `Node`s in post order.
// This is represents the infix input as postfix.
// Postfix can then be more easily evaluated.
std::vector<const Node*> PostOrderTraversal(const Node* root) {
  std::vector<const Node*> result;
  std::vector<const Node*> stack;
  stack.push_back(root);
  while (!stack.empty()) {
    const Node* top = stack.back();
    stack.pop_back();
    result.push_back(top);
    if (top->Left()) {
      stack.push_back(top->Left());
    }
    if (top->Right()) {
      stack.push_back(top->Right());
    }
  }
  std::reverse(result.begin(), result.end());
  return result;
}

}  // namespace

void ASTStackVisitor::Visit(const OpNode& node, std::vector<KVSetView>& stack) {
  KVSetView right = std::move(stack.back());
  stack.pop_back();
  KVSetView left = std::move(stack.back());
  stack.pop_back();
  stack.emplace_back(node.Op(std::move(left), std::move(right)));
}

void ASTStackVisitor::Visit(const ValueNode& node,
                            std::vector<KVSetView>& stack) {
  stack.emplace_back(lookup_fn_(node.Key()));
}

KVSetView Compute(const std::vector<const Node*>& postorder,
                  absl::AnyInvocable<absl::flat_hash_set<std::string_view>(
                      std::string_view key) const>
                      lookup_fn) {
  std::vector<KVSetView> stack;
  ASTStackVisitor visitor(std::move(lookup_fn));
  // Apply the operations on the postorder stack
  for (const auto* node : postorder) {
    node->Accept(visitor, stack);
  }
  return stack.back();
}

KVSetView Eval(const Node& node,
               absl::AnyInvocable<absl::flat_hash_set<std::string_view>(
                   std::string_view key) const>
                   lookup_fn) {
  std::vector<const Node*> postorder = PostOrderTraversal(&node);
  return Compute(postorder, std::move(lookup_fn));
}

void OpNode::Accept(ASTStackVisitor& visitor,
                    std::vector<KVSetView>& stack) const {
  visitor.Visit(*this, stack);
}

std::string UnionNode::Accept(ASTStringVisitor& visitor) const {
  return visitor.Visit(*this);
}
std::string DifferenceNode::Accept(ASTStringVisitor& visitor) const {
  return visitor.Visit(*this);
}
std::string IntersectionNode::Accept(ASTStringVisitor& visitor) const {
  return visitor.Visit(*this);
}

absl::flat_hash_set<std::string_view> OpNode::Keys() const {
  std::vector<const Node*> nodes;
  absl::flat_hash_set<std::string_view> key_set;
  nodes.push_back(this);
  while (!nodes.empty()) {
    const Node* next = nodes.back();
    nodes.pop_back();
    const Node* left = next->Left();
    const Node* right = next->Right();
    if (left == nullptr && right == nullptr) {
      // ValueNode
      absl::flat_hash_set<std::string_view> value_keys = next->Keys();
      assert(value_keys.size() == 1);
      key_set.merge(std::move(value_keys));
    }
    if (left != nullptr) {
      nodes.push_back(left);
    }
    if (right != nullptr) {
      nodes.push_back(right);
    }
  }
  return key_set;
}

ValueNode::ValueNode(std::string key) : key_(std::move(key)) {}

void ValueNode::Accept(ASTStackVisitor& visitor,
                       std::vector<KVSetView>& stack) const {
  visitor.Visit(*this, stack);
}

std::string ValueNode::Accept(ASTStringVisitor& visitor) const {
  return visitor.Visit(*this);
}

absl::flat_hash_set<std::string_view> ValueNode::Keys() const {
  // Return a set containing a view into this instances, `key_`.
  // Be sure that the reference is not to any temp string.
  return {
      {key_},
  };
}

}  // namespace kv_server
