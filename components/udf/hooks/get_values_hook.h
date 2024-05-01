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

#ifndef COMPONENTS_UDF_GET_VALUES_HOOK_H_
#define COMPONENTS_UDF_GET_VALUES_HOOK_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "components/data_server/cache/cache.h"
#include "components/internal_server/lookup.h"
#include "components/util/request_context.h"
#include "src/roma/config/function_binding_object_v2.h"

namespace kv_server {

// Functor that acts as a wrapper for the internal lookup client call.
class GetValuesHook {
 public:
  enum class OutputType { kString = 0, kBinary };

  virtual ~GetValuesHook() = default;

  // We need to split the hook init, since lookup depends on the cache.
  // However, UdfClient init requires the hook and it also forks.
  // The cache is only initialized after UdfClient init, so the hook
  // init can only be completed after UdfClient and cache init.
  virtual void FinishInit(std::unique_ptr<Lookup> lookup) = 0;

  // This is registered with v8 and is exposed to the UDF. Internally, it calls
  // the internal lookup client.
  virtual void operator()(
      google::scp::roma::FunctionBindingPayload<std::weak_ptr<RequestContext>>&
          payload) = 0;

  static std::unique_ptr<GetValuesHook> Create(OutputType output_type);
};

}  // namespace kv_server

#endif  // COMPONENTS_UDF_GET_VALUES_HOOK_H_
