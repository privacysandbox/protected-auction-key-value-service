/*
 * Copyright 2022 Google LLC
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

#ifndef COMPONENTS_UDF_CODE_FETCHER_H_
#define COMPONENTS_UDF_CODE_FETCHER_H_

#include <memory>

#include "absl/status/status.h"
#include "components/internal_lookup/lookup_client.h"
#include "components/udf/code_config.h"

namespace kv_server {

class CodeFetcher {
 public:
  virtual ~CodeFetcher() {}

  // Fetches untrusted code for UDF execution from the lookup server.
  virtual absl::StatusOr<CodeConfig> FetchCodeConfig(
      const LookupClient& lookup_client) = 0;

  static std::unique_ptr<CodeFetcher> Create();
};

}  // namespace kv_server

#endif  // COMPONENTS_UDF_CODE_FETCHER_H_
