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

// Realtime message application failure
inline constexpr absl::string_view kRealtimeMessageApplicationFailure =
    "RealtimeMessageApplicationFailure";
// Failure in getting realtime notifications
inline constexpr absl::string_view kRealtimeGetNotificationsFailure =
    "RealtimeGetNotificationsFailure";
// Sleep failure in getting realtime notifications
inline constexpr absl::string_view kRealtimeSleepFailure =
    "RealtimeSleepFailure";
// Failure in decoding realtime message
inline constexpr absl::string_view kRealtimeDecodeMessageFailure =
    "RealtimeDecodeMessageFailure";

// Failure in decrypting request in the internal secure lookup
inline constexpr absl::string_view kRequestDecryptionFailure =
    "RequestDecryptionFailure";
// Error in unpadding request in the internal secure lookup
inline constexpr absl::string_view kRequestUnpaddingError =
    "RequestUnpaddingError";
// Failure in encrypting response in internal secure lookup
inline constexpr absl::string_view kResponseEncryptionFailure =
    "ResponseEncryptionFailure";
// Internal run query request failure
inline constexpr std::string_view kInternalRunQueryRequestFailure =
    "InternalRunQueryRequestFailure";
// Failure in executing run query in local lookup
inline constexpr std::string_view kLocalRunQueryFailure =
    "LocalRunQueryFailure";
// Keyset not found error in the GetKeyValueSet in local lookup
inline constexpr std::string_view kLocalGetKeyValueSetKeySetNotFound =
    "LocalGetKeyValueSetKeySetNotFound";
// Missing keyset in the run query in local lookup
inline constexpr std::string_view kLocalRunQueryMissingKeySet =
    "LocalRunQueryMissingKeySet";
// Query parsing failure in the run query in local lookup
inline constexpr std::string_view kLocalRunQueryParsingFailure =
    "LocalRunQueryParsingFailure";

// Lookup client missing in the sharded lookup
inline constexpr std::string_view kLookupClientMissing = "LookupClientMissing";
// Failure in creating lookup futures
inline constexpr std::string_view kLookupFuturesCreationFailure =
    "LookupFuturesCreationFailure";
inline constexpr std::string_view kRemoteRequestEncryptionFailure =
    "RemoteRequestEncryptionFailure";
inline constexpr std::string_view kRemoteResponseDecryptionFailure =
    "RemoteResponseDecryptionFailure";
inline constexpr std::string_view kRemoteSecureLookupFailure =
    "RemoteSecureLookupFailure";
// Sharded GetKeyValues request failure
inline constexpr std::string_view kShardedKeyValueRequestFailure =
    "ShardedKeyValueRequestFailure";
// Sharded GetKeyValueSet request failure
inline constexpr std::string_view kShardedKeyValueSetRequestFailure =
    "ShardedKeyValueSetRequestFailure";
// Key collisions in collecting results from sharded GetKeyValueSet requests
inline constexpr std::string_view kShardedKeyCollisionOnKeySetCollection =
    "ShardedKeyCollisionOnKeySetCollection";
// Empty query encountered in the sharded lookup
inline constexpr std::string_view kShardedRunQueryEmptyQuery =
    "ShardedRunQueryEmptyQuery";
// Failure in running query in sharded lookup
inline constexpr std::string_view kShardedRunQueryFailure =
    "ShardedRunQueryFailure";
// Key set not found error in the GetValueKeySet in sharded lookup
inline constexpr std::string_view kShardedGetKeyValueSetKeySetNotFound =
    "ShardedGetKeyValueSetKeySetNotFound";
// Key set retrieval failure in the GetValueKeySet in sharded lookup
inline constexpr std::string_view kShardedGetKeyValueSetKeySetRetrievalFailure =
    "ShardedGetKeyValueSetKeySetRetrievalFailure";
// Failure in key set retrieval in the run query in sharded lookup
inline constexpr std::string_view kShardedRunQueryKeySetRetrievalFailure =
    "ShardedRunQueryKeySetRetrievalFailure";
// Missing keyset in the run query in sharded lookup
inline constexpr std::string_view kShardedRunQueryMissingKeySet =
    "ShardedRunQueryMissingKeySet";
// Query parsing failure in the run query in sharded lookup
inline constexpr std::string_view kShardedRunQueryParsingFailure =
    "ShardedRunQueryParsingFailure";

// TODO(b/304311724): Revisit the error codes and provide utililities to make
//  it easier to log error metrics
// Strings must be sorted, this is required by the API of partitioned metrics
inline constexpr absl::string_view kKVUdfRequestErrorCode[] = {
    kLookupClientMissing,
    kLookupFuturesCreationFailure,
    kRemoteRequestEncryptionFailure,
    kRemoteResponseDecryptionFailure,
    kRemoteSecureLookupFailure,
    kShardedGetKeyValueSetKeySetNotFound,
    kShardedGetKeyValueSetKeySetRetrievalFailure,
    kShardedKeyCollisionOnKeySetCollection,
    kShardedKeyValueRequestFailure,
    kShardedKeyValueSetRequestFailure,
    kShardedRunQueryEmptyQuery,
    kShardedRunQueryFailure,
    kShardedRunQueryKeySetRetrievalFailure,
    kShardedRunQueryMissingKeySet,
    kShardedRunQueryParsingFailure,
};

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

// Strings must be sorted, this is required by the API of partitioned metrics
inline constexpr absl::string_view kRealtimeErrorCode[] = {
    kRealtimeDecodeMessageFailure,
    kRealtimeGetNotificationsFailure,
    kRealtimeMessageApplicationFailure,
    kRealtimeSleepFailure,
};

// Strings must be sorted, this is required by the API of partitioned metrics
inline constexpr absl::string_view kInternalLookupRequestErrorCode[] = {
    kInternalRunQueryRequestFailure, kLocalGetKeyValueSetKeySetNotFound,
    kLocalRunQueryFailure,           kLocalRunQueryMissingKeySet,
    kLocalRunQueryParsingFailure,    kRequestDecryptionFailure,
    kRequestUnpaddingError,          kResponseEncryptionFailure,
};

}  // namespace kv_server

#endif  // COMPONENTS_TELEMETRY_ERROR_CODE_H_
