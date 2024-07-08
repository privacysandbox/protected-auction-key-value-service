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

#include "components/data_server/server/key_fetcher_factory.h"

namespace kv_server {
std::unique_ptr<KeyFetcherFactory> KeyFetcherFactory::Create(
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  return std::make_unique<CloudKeyFetcherFactory>(log_context);
}

}  // namespace kv_server
