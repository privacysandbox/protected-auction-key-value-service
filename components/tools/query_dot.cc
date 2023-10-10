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
#include <vector>

#include "absl/strings/str_join.h"

namespace kv_server::query_toy {

namespace {

// General purpose Vistor capable of returning a string representation of a Node
// upon inspection.
class ASTNameVisitor : public ASTStringVisitor {
 public:
  virtual std::string Visit(const UnionNode&) { return "Union"; }
  virtual std::string Visit(const DifferenceNode&) { return "Difference"; }
  virtual std::string Visit(const IntersectionNode&) { return "Intersection"; }
  virtual std::string Visit(const ValueNode&) { return "Value"; }
};

class ASTDotGraphLabelVisitor : public ASTStringVisitor {
 public:
  virtual std::string Visit(const UnionNode& node) {
    return name_visitor_.Visit(node);
  }

  virtual std::string Visit(const DifferenceNode& node) {
    return name_visitor_.Visit(node);
  }

  virtual std::string Visit(const IntersectionNode& node) {
    return name_visitor_.Visit(node);
  }

  virtual std::string Visit(const ValueNode& node) {
    return absl::StrCat(ToString(node.Keys()), "->", ToString(Eval(node)));
  }

 private:
  ASTNameVisitor name_visitor_;
};

std::string DotNodeName(const Node& node, uint32_t namecnt) {
  ASTNameVisitor name_visitor;
  return absl::StrCat(node.Accept(name_visitor), namecnt);
}

std::string ToDotGraphBody(const Node& node, uint32_t* namecnt) {
  ASTDotGraphLabelVisitor label_visitor;
  const std::string label = node.Accept(label_visitor);
  const std::string node_name = DotNodeName(node, *namecnt);
  std::string dot_str = absl::StrCat(node_name, " [label=\"", label, "\"]\n");
  if (node.Left() != nullptr) {
    *namecnt = *namecnt + 1;
    const std::string arrow =
        absl::StrCat(node_name, " -- ", DotNodeName(*node.Left(), *namecnt));
    absl::StrAppend(&dot_str, arrow, "\n",
                    ToDotGraphBody(*node.Left(), namecnt));
  }
  if (node.Right() != nullptr) {
    *namecnt = *namecnt + 1;
    const std::string arrow =
        absl::StrCat(node_name, " -- ", DotNodeName(*node.Right(), *namecnt));
    absl::StrAppend(&dot_str, arrow, "\n",
                    ToDotGraphBody(*node.Right(), namecnt));
  }
  return dot_str;
}

}  // namespace

void QueryDotWriter::WriteAst(std::string_view query, const Node& node) {
  uint32_t namecnt = 0;
  const std::string title =
      absl::StrCat("labelloc=\"t\"\nlabel=\"AST for Query: ", query, "\"\n");
  file_ << absl::StrCat("graph {\n", title, ToDotGraphBody(node, &namecnt),
                        "\n}\n");
}

void QueryDotWriter::Flush() { file_.flush(); }
}  // namespace kv_server::query_toy
