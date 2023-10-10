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

#ifndef COMPONENTS_TOOLS_PUBLISHER_SERVICE_H_
#define COMPONENTS_TOOLS_PUBLISHER_SERVICE_H_

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "components/data/common/notifier_metadata.h"

namespace kv_server {
class PublisherService {
 public:
  virtual ~PublisherService() = default;
  // Publish a message
  virtual absl::Status Publish(const std::string& message) = 0;
  static absl::StatusOr<std::unique_ptr<PublisherService>> Create(
      NotifierMetadata notifier_metadata);
};

}  // namespace kv_server

#endif  // COMPONENTS_TOOLS_PUBLISHER_SERVICE_H_
