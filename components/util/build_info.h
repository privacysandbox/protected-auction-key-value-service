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

namespace fledge::kv_server {

enum class TreeStatusType {
  // no uncommitted changes found in the source tree
  CLEAN,
  // the source tree contains modifications
  MODIFIED,
};

extern const int64_t kVersionBuildTimestamp;
extern const std::string_view kVersionBuildVcsRevision;
extern const TreeStatusType kVersionBuildVcsTreeStatus;
extern const std::string_view kVersionBuildVersion;
extern const std::string_view kVersionBuildVersionExtended;
extern const std::string_view kVersionBuildPlatform;
extern const std::string_view kVersionBuildToolchainHash;
extern const std::string_view kVersionBuildFlavor;

void LogBuildInfo();

// When this binary was built (used for versioning).
//
// For example: 1104963895
int64_t BuildTimestampAsInt();

// Version control system commit hash
//
// For example: 670425accbe92a1e7c9c04db3a355306d2cea485
std::string_view BuildVcsRevision();

TreeStatusType BuildVcsTreeStatus();

// String value of ClientStatus(): one of "clean" or "modified"
std::string_view BuildVcsTreeStatusAsString();

// The compiler/target-cpu combination used to compile this executable.
std::string_view BuildPlatform();

// The compiler/target-cpu combination used to compile this executable.
std::string_view BuildToolchainHash();

// String indicating the build-time config used to build this executable.
std::string_view BuildFlavor();

// semantic version of the application
std::string_view BuildVersion();

// extended version string of the application
std::string_view BuildVersionExtended();

}  // namespace fledge::kv_server

#endif  // COMPONENTS_UTIL_BUILD_INFO_H_
