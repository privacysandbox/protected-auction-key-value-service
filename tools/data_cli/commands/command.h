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

#ifndef TOOLS_DATA_CLI_COMMANDS_COMMAND_H_
#define TOOLS_DATA_CLI_COMMANDS_COMMAND_H_

#include "absl/status/status.h"

namespace kv_server {

// A `Command` defines an interface for objects that can be executed to do some
// work.
class Command {
 public:
  virtual ~Command() = default;
  // Executes some work.
  // Returns:
  // - absl::OkStatus() - if executing is successful.
  // - !absl::OkStatus() - if execution fails. The returned status provides
  // details about what failed.
  virtual absl::Status Execute() = 0;
};

}  //  namespace kv_server

#endif  // TOOLS_DATA_CLI_COMMANDS_COMMAND_H_
