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

#ifndef COMPONENTS_TELEMETRY_SERVER_DEFINITION_H_
#define COMPONENTS_TELEMETRY_SERVER_DEFINITION_H_

#include <memory>
#include <string>
#include <utility>

#include "src/cpp/metric/context_map.h"

namespace kv_server {

// When server is running in debug mode, all unsafe metrics will be logged
// safely without DP noise applied. Therefore for now it is okay to set DP
// upper and lower bounds for all unsafe metrics to a default value. But
// need to revisit and tune the upper and lower bound parameters once we have
// metric monitoring set up.
// TODO(b/307362951): Tune the upper bound and lower bound for different
// unsafe metrics.
constexpr int kCounterDPLowerBound = 1;
constexpr int kCounterDPUpperBound = 10;

constexpr int kMicroSecondsLowerBound = 1;
constexpr int kMicroSecondsUpperBound = 2'000'000'000;

inline constexpr double kLatencyInMicroSecondsBoundaries[] = {
    160,     220,       280,       320,       640,       1'200,         2'500,
    5'000,   10'000,    20'000,    40'000,    80'000,    160'000,       320'000,
    640'000, 1'000'000, 1'300'000, 2'600'000, 5'000'000, 10'000'000'000};

// String literals for absl status, the string list and literals match those
// implemented in the absl::StatusCodeToString method
// https://github.com/abseil/abseil-cpp/blob/1a03fb9dd1c533e42b6d7d1ebea85b448a07e793/absl/status/status.cc#L47
inline constexpr absl::string_view kAbslStatusStrings[] = {
    "OK",
    "CANCELLED",
    "UNKNOWN",
    "INVALID_ARGUMENT",
    "DEADLINE_EXCEEDED",
    "NOT_FOUND",
    "ALREADY_EXISTS",
    "PERMISSION_DENIED",
    "UNAUTHENTICATED",
    "RESOURCE_EXHAUSTED",
    "FAILED_PRECONDITION",
    "ABORTED",
    "OUT_OF_RANGE",
    "UNIMPLEMENTED",
    "INTERNAL",
    "UNAVAILABLE",
    "DATA_LOSS",
    ""};

// Metric definitions for request level metrics that are privacy impacting
// and should be logged unsafe with DP(differential privacy) noises.
inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kInternalRunQueryKeySetRetrievalFailure(
        "InternalRunQueryKeySetRetrievalFailure",
        "Number of key set internal retrieval failures during internal"
        "run query processing",
        kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kKeysNotFoundInKeySetsInShardedLookup(
        "KeysNotFoundInKeySetsInShardedLookup",
        "Number of keys not found in the result key set in the sharded lookup",
        kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kKeysNotFoundInKeySetsInLocalLookup(
        "KeysNotFoundInKeySetsInLocalLookup",
        "Number of keys not found in the result key set in the local lookup",
        kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kInternalRunQueryEmtpyQuery("InternalRunQueryEmtpyQuery",
                                "Number of empty queries encountered during "
                                "internal run query processing",
                                kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kInternalRunQueryMissingKeySet(
        "InternalRunQueryMissingKeySet",
        "Number of missing keys not found in the key set "
        "during internal run query processing",
        kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kInternalRunQueryParsingFailure(
        "InternalRunQueryParsingFailure",
        "Number of failures in parsing query during "
        "internal run query processing",
        kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kLookupClientMissing(
        "LookupClientMissing",
        "Number of missing internal lookup clients encountered during "
        "sharded lookup",
        kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kShardedLookupServerRequestFailed(
        "ShardedLookupServerRequestFailed",
        "Number of failed server requests in the sharded lookup",
        kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kShardedLookupServerKeyCollisionOnCollection(
        "ShardedLookupServerKeyCollisionOnCollection",
        "Number of key collisions when collecting results from shards",
        kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kLookupFuturesCreationFailure(
        "LookupFuturesCreationFailure",
        "Number of failures in creating lookup futures in the sharded lookup",
        kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kShardedLookupFailure("ShardedLookupFailure",
                          "Number of lookup failures in the sharded lookup",
                          kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kRemoteClientEncryptionFailure(
        "RemoteClientEncryptionFailure",
        "Number of request encryption failures in the remote lookup client",
        kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kRemoteClientSecureLookupFailure(
        "RemoteClientSecureLookupFailure",
        "Number of secure lookup failures in the remote lookup client",
        kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kRemoteClientDecryptionFailure(
        "RemoteClientDecryptionFailure",
        "Number of response decryption failures in the remote lookup client",
        kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kInternalClientDecryptionFailure(
        "InternalClientEncryptionFailure",
        "Number of request decryption failures in the internal lookup client",
        kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kInternalClientUnpaddingRequestError(
        "InternalClientUnpaddingRequestError",
        "Number of unpadding errors in the request deserialization in the "
        "internal lookup client",
        kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kShardedLookupRunQueryLatencyInMicros(
        "ShardedLookupRunQueryLatencyInMicros",
        "Latency in executing run query in the sharded lookup",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kRemoteLookupGetValuesLatencyInMicros(
        "RemoteLookupGetValuesLatencyInMicros",
        "Latency in get values in the remote lookup",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kInternalSecureLookupLatencyInMicros("InternalSecureLookupLatencyInMicros",
                                         "Latency in internal secure lookup",
                                         kLatencyInMicroSecondsBoundaries,
                                         kMicroSecondsUpperBound,
                                         kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kGetValuePairsLatencyInMicros("GetValuePairsLatencyInMicros",
                                  "Latency in executing GetValuePairs in cache",
                                  kLatencyInMicroSecondsBoundaries,
                                  kMicroSecondsUpperBound,
                                  kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kGetKeyValueSetLatencyInMicros(
        "GetKeyValueSetLatencyInMicros",
        "Latency in executing GetKeyValueSet in cache",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

// Metric definitions for safe metrics that are not privacy impacting
inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kRealtimeGetNotificationsFailure(
        "RealtimeGetNotificationsFailure",
        "Number of failures in getting realtime notifications");

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kRealtimeSleepFailure(
        "RealtimeSleepFailure",
        "Number of sleep failures in getting realtime notifications");

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kRealtimeTotalRowsUpdated("RealtimeTotalRowsUpdated",
                              "Realtime total rows updated status count",
                              "status", kAbslStatusStrings);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kReceivedLowLatencyNotificationsE2ECloudProvided(
        "ReceivedLowLatencyNotificationsE2ECloudProvided",
        "Time between cloud topic publisher inserting message and realtime "
        "notifier receiving the message",
        kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kReceivedLowLatencyNotificationsE2E(
        "ReceivedLowLatencyNotificationsE2E",
        "Time between producer producing the message and realtime notifier "
        "receiving the message",
        kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kReceivedLowLatencyNotifications(
        "ReceivedLowLatencyNotifications",
        "Latency in realtime notifier processing the received batch of "
        "notification messages",
        kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kRealtimeDecodeMessageFailure(
        "RealtimeDecodeMessageFailure",
        "Number of failures in decoding realtime message");

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kRealtimeMessageApplicationFailure(
        "RealtimeMessageApplicationFailure",
        "Number of realtime message application failures");

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kAwsChangeNotifierQueueSetupFailure(
        "AwsChangeNotifierQueueSetupFailure",
        "Number of failures in setting up AWS change notifier queue");

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kAwsSqsReceiveMessageLatency("AwsSqsReceiveMessageLatency",
                                 "AWS SQS receive message latency",
                                 kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kAwsChangeNotifierTagFailure("AwsChangeNotifierTagFailure",
                                 "Number of failures in tagging AWS SQS queue");

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kAwsChangeNotifierMessagesReceivingFailure(
        "AwsChangeNotifierMessagesReceivingFailure",
        "Number of failures in receiving message from AWS SQS");

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kAwsChangeNotifierMessagesDataLossFailure(
        "AwsChangeNotifierMessagesDataLossFailure",
        "Number of failures in extracting data from AWS SQS messages");

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kAwsChangeNotifierMessagesDeletionFailure(
        "AwsChangeNotifierMessagesDeletionFailure",
        "Number of failures in deleting messages from AWS SQS");

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kAwsJsonParseError(
        "AwsJsonParseError",
        "Number of errors in parsing Json from AWS S3 notification");

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kDeltaFileRecordChangeNotifierParsingFailure(
        "DeltaFileRecordChangeNotifierParsingFailure",
        "Number of errors in parsing Json from delta file record change "
        "notification");

inline constexpr const privacy_sandbox::server_common::metrics::DefinitionName*
    kKVServerMetricList[] = {
        // Unsafe metrics
        &kInternalRunQueryKeySetRetrievalFailure,
        &kKeysNotFoundInKeySetsInShardedLookup,
        &kKeysNotFoundInKeySetsInLocalLookup, &kInternalRunQueryEmtpyQuery,
        &kInternalRunQueryMissingKeySet, &kInternalRunQueryParsingFailure,
        &kLookupClientMissing, &kShardedLookupServerRequestFailed,
        &kShardedLookupServerKeyCollisionOnCollection,
        &kLookupFuturesCreationFailure, &kShardedLookupFailure,
        &kRemoteClientEncryptionFailure, &kRemoteClientSecureLookupFailure,
        &kRemoteClientDecryptionFailure, &kInternalClientDecryptionFailure,
        &kInternalClientUnpaddingRequestError,
        &kShardedLookupRunQueryLatencyInMicros,
        &kRemoteLookupGetValuesLatencyInMicros,
        &kInternalSecureLookupLatencyInMicros, &kGetValuePairsLatencyInMicros,
        &kGetKeyValueSetLatencyInMicros,
        // Safe metrics
        &privacy_sandbox::server_common::metrics::kServerTotalTimeMs,
        &kRealtimeGetNotificationsFailure, &kRealtimeSleepFailure,
        &kRealtimeTotalRowsUpdated,
        &kReceivedLowLatencyNotificationsE2ECloudProvided,
        &kReceivedLowLatencyNotificationsE2E, &kReceivedLowLatencyNotifications,
        &kRealtimeDecodeMessageFailure, &kRealtimeMessageApplicationFailure,
        &kAwsChangeNotifierQueueSetupFailure, &kAwsSqsReceiveMessageLatency,
        &kAwsChangeNotifierTagFailure,
        &kAwsChangeNotifierMessagesReceivingFailure,
        &kAwsChangeNotifierMessagesDataLossFailure,
        &kAwsChangeNotifierMessagesDeletionFailure, &kAwsJsonParseError,
        &kDeltaFileRecordChangeNotifierParsingFailure};

}  // namespace kv_server

#endif  // COMPONENTS_TELEMETRY_SERVER_DEFINITION_H_
