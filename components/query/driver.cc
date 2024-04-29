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

#include "components/query/driver.h"

#include <string_view>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/functional/bind_front.h"
#include "components/query/ast.h"

namespace kv_server {

Driver::Driver(absl::AnyInvocable<absl::flat_hash_set<std::string_view>(
                   std::string_view key) const>
                   lookup_fn)
    : lookup_fn_(std::move(lookup_fn)) {}

void Driver::SetAst(std::unique_ptr<Node> ast) { ast_ = std::move(ast); }

absl::StatusOr<absl::flat_hash_set<std::string_view>> Driver::GetResult()
    const {
  if (!status_.ok()) {
    return status_;
  }
  if (ast_ == nullptr) {
    return absl::flat_hash_set<std::string_view>();
  }
  return Eval(*ast_, [this](std::string_view key) { return lookup_fn_(key); });
}

void Driver::SetError(std::string error) {
  status_ = absl::InvalidArgumentError(std::move(error));
}

const Node* Driver::GetRootNode() const { return ast_.get(); }

}  // namespace kv_server
