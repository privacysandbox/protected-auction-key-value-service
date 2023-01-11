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

#ifndef COMPONENTS_UTIL_BUILD_INFO_H_
#define COMPONENTS_UTIL_BUILD_INFO_H_

#include <string_view>

namespace kv_server {

extern const std::string_view kVersionBuildFlavor;
extern const std::string_view kVersionBuildPlatform;
extern const std::string_view kVersionBuildToolchainHash;
extern const std::string_view kVersionBuildVersion;

void LogBuildInfo();

// String indicating the build-time config used to build this executable.
std::string_view BuildFlavor();

// The compiler/target-cpu combination used to compile this executable.
std::string_view BuildPlatform();

// The compiler/target-cpu combination used to compile this executable.
std::string_view BuildToolchainHash();

// semantic version of the application
std::string_view BuildVersion();

}  // namespace kv_server

#endif  // COMPONENTS_UTIL_BUILD_INFO_H_
