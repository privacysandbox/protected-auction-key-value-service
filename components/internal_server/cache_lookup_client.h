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

#ifndef COMPONENTS_INTERNAL_SERVER_CACHE_LOOKUP_CLIENT_H_
#define COMPONENTS_INTERNAL_SERVER_CACHE_LOOKUP_CLIENT_H_

#include <memory>

#include "absl/status/statusor.h"
#include "components/data_server/cache/cache.h"
#include "components/internal_server/lookup.pb.h"
#include "components/internal_server/lookup_client.h"

// TODO(b/290640967): This will go away, but in the meantime it's a more
// reliable way to have a "lookup" client for testing/tools that directly
// queries the cache.
namespace kv_server {

// Creates a cache lookup client that uses the cache to look up keys directly.
std::unique_ptr<LookupClient> CreateCacheLookupClient(const Cache& cache);

}  // namespace kv_server

#endif  // COMPONENTS_INTERNAL_SERVER_CACHE_LOOKUP_CLIENT_H_
