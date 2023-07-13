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

#include <memory>

#include "src/cpp/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"
#include "src/cpp/encryption/key_fetcher/src/fake_key_fetcher_manager.h"

namespace kv_server {
std::unique_ptr<privacy_sandbox::server_common::KeyFetcherManagerInterface>
CreateKeyFetcherManager() {
  return std::make_unique<
      privacy_sandbox::server_common::FakeKeyFetcherManager>();
}
}  // namespace kv_server
