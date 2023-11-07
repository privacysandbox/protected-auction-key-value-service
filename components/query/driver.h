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

#include <memory>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_set.h"
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
//   Driver driver(LookupFn);
//   std::istringstream stream(query);
//   Scanner scanner(stream);
//   Parser parse(driver, scanner);
//   int parse_result = parse();
//   auto result = driver.GetResult();
// parse_result is only expected to be non-zero when result is a failure.
class Driver {
 public:
  // `lookup_fn` returns the set associated with the provided key.
  // If no key is present, an empty set should be returned.
  explicit Driver(absl::AnyInvocable<absl::flat_hash_set<std::string_view>(
                      std::string_view key) const>
                      lookup_fn);

  // The result contains views of the data within the DB.
  absl::StatusOr<absl::flat_hash_set<std::string_view>> GetResult() const;

  // Returns the the `Node` associated with `SetAst`
  // or nullptr if unset.
  const kv_server::Node* GetRootNode() const;

  // Clients should not call these functions, they are called by the parser.
  void SetAst(std::unique_ptr<kv_server::Node>);
  void SetError(std::string error);
  void ClearError() { status_ = absl::OkStatus(); }

  // Looks up the set which contains a view of the DB data.
  absl::flat_hash_set<std::string_view> Lookup(std::string_view key) const;

 private:
  absl::AnyInvocable<absl::flat_hash_set<std::string_view>(std::string_view key)
                         const>
      lookup_fn_;
  std::unique_ptr<kv_server::Node> ast_;
  absl::Status status_ = absl::OkStatus();
};

}  // namespace kv_server
#endif  // COMPONENTS_QUERY_DRIVER_H_
