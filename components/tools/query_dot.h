/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMPONENTS_TOOLS_QUERY_DOT_H_
#define COMPONENTS_TOOLS_QUERY_DOT_H_

#include <algorithm>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_join.h"
#include "components/query/ast.h"

namespace kv_server::query_toy {
class QueryDotWriter {
 public:
  explicit QueryDotWriter(std::string_view path) : file_(path.data()) {}
  ~QueryDotWriter() { file_.close(); }
  // Outputs the dot representation of the AST node to the output path.
  void WriteAst(
      const std::string_view query, const Node& node,
      std::function<absl::flat_hash_set<std::string_view>(std::string_view key)>
          lookup_fn);
  void Flush();

 private:
  std::ofstream file_;
};

template <typename T>
std::string ToString(const T& set) {
  std::vector<std::string_view> sorted_set(set.begin(), set.end());
  std::sort(sorted_set.begin(), sorted_set.end());
  return absl::StrCat("[", absl::StrJoin(sorted_set, ","), "]");
}

};  // namespace kv_server::query_toy

#endif /* COMPONENTS_TOOLS_QUERY_DOT_H_ */
