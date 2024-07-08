/*
 * Copyright 2024 Google LLC
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

#ifndef COMPONENTS_UTIL_SAFE_PATH_LOG_CONTEXT_H_
#define COMPONENTS_UTIL_SAFE_PATH_LOG_CONTEXT_H_

#include "src/logger/request_context_impl.h"

namespace kv_server {

// Token that allows otel logging for safe code execution path
class KVServerSafeLogContext
    : public privacy_sandbox::server_common::log::SafePathContext {
  ~KVServerSafeLogContext() override = default;

 private:
  KVServerSafeLogContext() = default;
  friend class Server;
};

}  // namespace kv_server

#endif  // COMPONENTS_UTIL_SAFE_PATH_LOG_CONTEXT_H_
