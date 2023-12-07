/*
 * Copyright 2023 Google LLC
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

#ifndef COMPONENTS_TELEMETRY_ERROR_CODE_H_
#define COMPONENTS_TELEMETRY_ERROR_CODE_H_

#include "absl/strings/string_view.h"

namespace kv_server {

// AWS change notifier queue setup failure
inline constexpr absl::string_view kAwsChangeNotifierQueueSetupFailure =
    "AwsChangeNotifierQueueSetupFailure";
// AWS change notifier failure in tagging the queue
inline constexpr absl::string_view kAwsChangeNotifierTagFailure =
    "AwsChangeNotifierTagFailure";
// AWS change notifier failure in receiving messages
inline constexpr absl::string_view kAwsChangeNotifierMessagesReceivingFailure =
    "AwsChangeNotifierMessagesReceivingFailure";
// AWS change notifier data loss from messages
inline constexpr absl::string_view kAwsChangeNotifierMessagesDataLoss =
    "AwsChangeNotifierMessagesDataLoss";
// AWS change notifier failure in deleting message from queue
inline constexpr absl::string_view kAwsChangeNotifierMessagesDeletionFailure =
    "AwsChangeNotifierMessagesDeletionFailure";
// Failure in parsing record from delta file record change notification
inline constexpr absl::string_view
    kDeltaFileRecordChangeNotifierParsingFailure =
        "DeltaFileRecordChangeNotifierParsingFailure";
// Error in parsing json from AWS S3 notification
inline constexpr absl::string_view kAwsJsonParseError = "AwsJsonParseError";

// Strings must be sorted, this is required by the API of partitioned metrics
inline constexpr absl::string_view kChangeNotifierErrorCode[] = {
    kAwsChangeNotifierMessagesDataLoss,
    kAwsChangeNotifierMessagesDeletionFailure,
    kAwsChangeNotifierMessagesReceivingFailure,
    kAwsChangeNotifierQueueSetupFailure,
    kAwsChangeNotifierTagFailure,
    kAwsJsonParseError,
    kDeltaFileRecordChangeNotifierParsingFailure,
};

}  // namespace kv_server

#endif  // COMPONENTS_TELEMETRY_ERROR_CODE_H_
