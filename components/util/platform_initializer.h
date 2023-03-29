// Copyright 2022 Google LLC
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

#ifndef COMPONENTS_UTIL_PLATFORM_INITIALIZER_H_
#define COMPONENTS_UTIL_PLATFORM_INITIALIZER_H_

namespace kv_server {

// This class performs any platform specific (e.g. AWS) setup that's necessary
// on startup and then cleanup on shutdown.
// This should be kept in scope until cleanup is needed.
class PlatformInitializer {
 public:
  PlatformInitializer();

  ~PlatformInitializer();
};

}  // namespace kv_server

#endif  // COMPONENTS_UTIL_PLATFORM_INITIALIZER_H_
