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

#ifndef COMPONENTS_DATA_REALTIME_NOTIFIER_METADATA_H_
#define COMPONENTS_DATA_REALTIME_NOTIFIER_METADATA_H_

#include <memory>
#include <string>

#include "components/data/common/notifier_metadata.h"
#include "components/util/sleepfor.h"

namespace google {
namespace cloud {
namespace pubsub {
// GCP uses inline namespaces for versioning. If we update GCP SDK and
// decide to bump the version, we'd need to update it here too.
inline namespace v2_17 {
// This is a forward declare so that we don't need to import the
//  pubsub Subscriber when we're running on non-GCP platforms.
class Subscriber;
}  // namespace v2_17
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

namespace kv_server {
// This is a forward declare so that we don't need to import the
//  DeltaFileRecordChangeNotifier when we're running on non-AWS platforms.
class DeltaFileRecordChangeNotifier;
struct AwsRealtimeNotifierMetadata {
  // Used for tests only.
  // This can't be a unique pointer, since this will be an incomplete type
  // for certain platforms.
  // Realtime notifier takes ownership of this if set.
  DeltaFileRecordChangeNotifier* change_notifier_for_unit_testing = nullptr;
  // Used for tests only.
  std::unique_ptr<SleepFor> maybe_sleep_for;
};

struct GcpRealtimeNotifierMetadata {
  // Used for tests only.
  // This can't be a unique pointer, since this will be an incomplete type
  // for certain platforms.
  // Realtime notifier takes ownership of this if set.
  ::google::cloud::pubsub::v2_17::Subscriber* gcp_subscriber_for_unit_testing =
      nullptr;
  // Used for tests only.
  std::unique_ptr<SleepFor> maybe_sleep_for;
};

struct LocalRealtimeNotifierMetadata {};

// An overall purpose of this type and this whole file is to allow
// injecting objects for unit testing.
using RealtimeNotifierMetadata =
    std::variant<AwsRealtimeNotifierMetadata, GcpRealtimeNotifierMetadata,
                 LocalRealtimeNotifierMetadata>;

}  // namespace kv_server
#endif  // COMPONENTS_DATA_REALTIME_NOTIFIER_METADATA_H_
