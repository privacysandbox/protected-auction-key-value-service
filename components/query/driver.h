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

#ifndef COMPONENTS_QUERY_DRIVER_H_
#define COMPONENTS_QUERY_DRIVER_H_

#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/query/ast.h"

namespace kv_server {

// Driver is responsible for:
//   * Gathering the AST from the parser
//   * Creating the exeuction plan
//   * Executing the query
//   * Storing the result
// Typical usage:
//   Driver driver;
//   std::istringstream stream(query);
//   Scanner scanner(stream);
//   Parser parse(driver, scanner);
//   int parse_result = parse();
//   auto result = driver.GetResult(LookupFn);
// parse_result is only expected to be non-zero when result is a failure.
class Driver {
 public:
  // The result contains views of the data within the DB.
  template <typename SetType>
  absl::StatusOr<SetType> EvaluateQuery(
      absl::AnyInvocable<SetType(std::string_view key) const> lookup_fn) const;

  // Returns the the `Node` associated with `SetAst`
  // or nullptr if unset.
  const kv_server::Node* GetRootNode() const;

  // Clients should not call these functions, they are called by the parser.
  void SetAst(std::unique_ptr<kv_server::Node>);
  void SetError(std::string error);
  void Clear() {
    status_ = absl::OkStatus();
    ast_ = nullptr;
    buffer_.clear();
  }
  std::vector<std::string_view> StoreStrings(std::vector<std::string> strings) {
    std::vector<std::string_view> views;
    views.reserve(strings.size());
    for (auto& string : strings) {
      buffer_.push_back(std::move(string));
      views.push_back(buffer_.back());
    }
    return views;
  }

 private:
  std::unique_ptr<kv_server::Node> ast_;
  // using list since we require pointer stabilty on string_view that references
  // them.
  std::list<std::string> buffer_;
  absl::Status status_ = absl::OkStatus();
};

template <typename SetType>
absl::StatusOr<SetType> Driver::EvaluateQuery(
    absl::AnyInvocable<SetType(std::string_view key) const> lookup_fn) const {
  if (!status_.ok()) {
    return status_;
  }
  if (ast_ == nullptr) {
    return SetType();
  }
  return Eval<SetType>(*ast_, std::move(lookup_fn));
}

}  // namespace kv_server
#endif  // COMPONENTS_QUERY_DRIVER_H_
