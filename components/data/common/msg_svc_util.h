/*
 * Copyright 2023 Google LLC
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

#ifndef COMPONENTS_DATA_COMMON_MSG_SVC_UTIL_H_
#define COMPONENTS_DATA_COMMON_MSG_SVC_UTIL_H_

#include <string>

namespace kv_server {

// Generates a string that starts with the passed in `name` and is padded to the
// length of 80 with randomly picked alphanumerical characters.
std::string GenerateQueueName(std::string name);

}  // namespace kv_server

#endif  // COMPONENTS_DATA_COMMON_MSG_SVC_UTIL_H_
