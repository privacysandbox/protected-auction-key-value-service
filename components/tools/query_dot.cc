// Copyright 2023 Google LLC
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

#include "components/tools/query_dot.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_join.h"
#include "src/util/status_macro/status_macros.h"

namespace kv_server::query_toy {

namespace {

// General purpose Vistor capable of returning a string representation of a Node
// upon inspection.
class ASTNameVisitor : public ASTStringVisitor {
 public:
  virtual absl::StatusOr<std::string> Visit(const UnionNode&) {
    return "Union";
  }
  virtual absl::StatusOr<std::string> Visit(const DifferenceNode&) {
    return "Difference";
  }
  virtual absl::StatusOr<std::string> Visit(const IntersectionNode&) {
    return "Intersection";
  }
  virtual absl::StatusOr<std::string> Visit(const ValueNode&) {
    return "Value";
  }
  virtual absl::StatusOr<std::string> Visit(const NumberSetNode&) {
    return "NumberSet";
  }
  virtual absl::StatusOr<std::string> Visit(const StringViewSetNode&) {
    return "StringViewSet";
  }
};

class ASTDotGraphLabelVisitor : public ASTStringVisitor {
 public:
  explicit ASTDotGraphLabelVisitor(
      absl::AnyInvocable<
          absl::flat_hash_set<std::string_view>(std::string_view key) const>
          lookup_fn)
      : lookup_fn_(std::move(lookup_fn)) {}

  virtual absl::StatusOr<std::string> Visit(const UnionNode& node) {
    return name_visitor_.Visit(node);
  }

  virtual absl::StatusOr<std::string> Visit(const DifferenceNode& node) {
    return name_visitor_.Visit(node);
  }

  virtual absl::StatusOr<std::string> Visit(const IntersectionNode& node) {
    return name_visitor_.Visit(node);
  }

  virtual absl::StatusOr<std::string> Visit(const ValueNode& node) {
    PS_ASSIGN_OR_RETURN(
        auto values,
        Eval<absl::flat_hash_set<std::string_view>>(
            node, [this](std::string_view key) { return lookup_fn_(key); }));
    return absl::StrCat(ToString(node.Keys()), "->", ToString(values));
  }

  virtual absl::StatusOr<std::string> Visit(const NumberSetNode& node) {
    auto numbers = node.GetValues();
    std::vector<std::string> strings(numbers.size());
    std::transform(numbers.begin(), numbers.end(), strings.begin(),
                   [](uint64_t num) { return std::to_string(num); });
    return absl::StrCat("NumberSet(", ToString(strings), ")");
  }

  virtual absl::StatusOr<std::string> Visit(const StringViewSetNode& node) {
    return absl::StrCat("StringViewSet(", ToString(node.GetValues()), ")");
  }

 private:
  ASTNameVisitor name_visitor_;
  absl::AnyInvocable<absl::flat_hash_set<std::string_view>(std::string_view key)
                         const>
      lookup_fn_;
};

absl::StatusOr<std::string> DotNodeName(const Node& node, uint32_t namecnt) {
  ASTNameVisitor name_visitor;
  PS_ASSIGN_OR_RETURN(auto name, node.Accept(name_visitor));
  return absl::StrCat(name, namecnt);
}

absl::StatusOr<std::string> ToDotGraphBody(
    const Node& node, uint32_t* namecnt,
    std::function<absl::flat_hash_set<std::string_view>(std::string_view)>
        lookup_fn) {
  ASTDotGraphLabelVisitor label_visitor(lookup_fn);
  PS_ASSIGN_OR_RETURN(const std::string label, node.Accept(label_visitor));
  PS_ASSIGN_OR_RETURN(const std::string node_name, DotNodeName(node, *namecnt));
  std::string dot_str = absl::StrCat(node_name, " [label=\"", label, "\"]\n");
  if (node.Left() != nullptr) {
    *namecnt = *namecnt + 1;
    PS_ASSIGN_OR_RETURN(const auto node_name_left,
                        DotNodeName(*node.Left(), *namecnt));
    const std::string arrow = absl::StrCat(node_name, " -- ", node_name_left);
    PS_ASSIGN_OR_RETURN(const auto dgb,
                        ToDotGraphBody(*node.Left(), namecnt, lookup_fn));
    absl::StrAppend(&dot_str, arrow, "\n", dgb);
  }
  if (node.Right() != nullptr) {
    *namecnt = *namecnt + 1;
    PS_ASSIGN_OR_RETURN(const auto node_name_right,
                        DotNodeName(*node.Right(), *namecnt));
    const std::string arrow = absl::StrCat(node_name, " -- ", node_name_right);
    PS_ASSIGN_OR_RETURN(const auto dgb,
                        ToDotGraphBody(*node.Right(), namecnt, lookup_fn));
    absl::StrAppend(&dot_str, arrow, "\n", dgb);
  }
  return dot_str;
}

}  // namespace

absl::Status QueryDotWriter::WriteAst(
    std::string_view query, const Node& node,
    std::function<absl::flat_hash_set<std::string_view>(std::string_view)>
        lookup_fn) {
  uint32_t namecnt = 0;
  const std::string title =
      absl::StrCat("labelloc=\"t\"\nlabel=\"AST for Query: ", query, "\"\n");
  PS_ASSIGN_OR_RETURN(const auto dgb,
                      ToDotGraphBody(node, &namecnt, std::move(lookup_fn)));
  file_ << absl::StrCat("graph {\n", title, dgb, "\n}\n");
  return absl::OkStatus();
}

void QueryDotWriter::Flush() { file_.flush(); }
}  // namespace kv_server::query_toy
