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

#include "absl/time/time.h"
#include "components/telemetry/error_code.h"
#include "opentelemetry/metrics/provider.h"
#include "src/core/common/uuid/uuid.h"
#include "src/metric/context_map.h"
#include "src/util/duration.h"
#include "src/util/read_system.h"

namespace kv_server {

constexpr std::string_view kKVServerServiceName = "KVServer";
constexpr std::string_view kInternalLookupServiceName = "InternalLookupServer";

// When server is running in debug mode, all unsafe metrics will be logged
// safely without DP noise applied. Therefore for now it is okay to set DP
// upper and lower bounds for all unsafe metrics to a default value. But
// need to revisit and tune the upper and lower bound parameters once we have
// metric monitoring set up.
// TODO(b/307362951): Tune the upper bound and lower bound for different
// unsafe metrics.
inline constexpr int kCounterDPLowerBound = 1;
inline constexpr int kCounterDPUpperBound = 10;

inline constexpr int kErrorCounterDPLowerBound = 0;
inline constexpr int kErrorCounterDPUpperBound = 1;
inline constexpr int kErrorMaxPartitionsContributed = 1;
inline constexpr double kErrorMinNoiseToOutput = 0.99;

inline constexpr int kMicroSecondsLowerBound = 1;
// 2 seconds
inline constexpr int kMicroSecondsUpperBound = 2'000'000'000;

inline constexpr double kLatencyInMicroSecondsBoundaries[] = {
    160,       220,       280,       320,       640,
    1'200,     2'500,     5'000,     10'000,    20'000,
    40'000,    80'000,    160'000,   320'000,   640'000,
    1'000'000, 1'300'000, 2'600'000, 5'000'000, 10'000'000'000,
};

// String literals for absl status partition, the string list and literals match
// those implemented in the absl::StatusCodeToString method
// https://github.com/abseil/abseil-cpp/blob/1a03fb9dd1c533e42b6d7d1ebea85b448a07e793/absl/status/status.cc#L47
// Strings in the partition are required to be sorted
inline constexpr absl::string_view kAbslStatusStrings[] = {
    "",
    "ABORTED",
    "ALREADY_EXISTS",
    "CANCELLED",
    "DATA_LOSS",
    "DEADLINE_EXCEEDED",
    "FAILED_PRECONDITION",
    "INTERNAL",
    "INVALID_ARGUMENT",
    "NOT_FOUND",
    "OK",
    "OUT_OF_RANGE",
    "PERMISSION_DENIED",
    "RESOURCE_EXHAUSTED",
    "UNAUTHENTICATED",
    "UNAVAILABLE",
    "UNIMPLEMENTED",
    "UNKNOWN"};

inline constexpr std::string_view kKeyValueCacheHit = "KeyValueCacheHit";
inline constexpr std::string_view kKeyValueCacheMiss = "KeyValueCacheMiss";
inline constexpr std::string_view kKeyValueSetCacheHit = "KeyValueSetCacheHit";
inline constexpr std::string_view kKeyValueSetCacheMiss =
    "KeyValueSetCacheMiss";
inline constexpr std::string_view kCacheAccessEvents[] = {
    kKeyValueCacheHit, kKeyValueCacheMiss, kKeyValueSetCacheHit,
    kKeyValueSetCacheMiss};

inline constexpr privacy_sandbox::server_common::metrics::PrivacyBudget
    privacy_total_budget = {
        .epsilon = 5,
};

// Metric definitions for request level metrics that are privacy impacting
// and should be logged unsafe with DP(differential privacy) noises.
inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kKVUdfRequestError("KVUdfRequestError",
                       "Errors in processing KV server V2 request",
                       "error_code", kErrorMaxPartitionsContributed,
                       kKVUdfRequestErrorCode, kErrorCounterDPUpperBound,
                       kErrorCounterDPLowerBound, kErrorMinNoiseToOutput);

// Metric definitions for request level metrics that are privacy impacting
// and should be logged unsafe with DP(differential privacy) noises.
inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kShardedLookupKeyCountByShard(
        "ShardedLookupKeyCountByShard", "Keys count by shard number",
        "key_shard_num", 1 /*max_partitions_contributed*/,
        privacy_sandbox::server_common::metrics::kEmptyPublicPartition,
        kCounterDPUpperBound, kCounterDPLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kShardedLookupGetKeyValuesLatencyInMicros(
        "ShardedLookupGetKeyValuesLatencyInMicros",
        "Latency in executing GetKeyValues in the sharded lookup",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kShardedLookupGetKeyValueSetLatencyInMicros(
        "ShardedLookupGetKeyValueSetLatencyInMicros",
        "Latency in executing GetKeyValueSet in the sharded lookup",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kShardedLookupGetUInt32ValueSetLatencyInMicros(
        "ShardedLookupGetUInt32ValueSetLatencyInMicros",
        "Latency in executing GetUInt32ValueSet in the sharded lookup",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kShardedLookupGetUInt64ValueSetLatencyInMicros(
        "ShardedLookupGetUInt64ValueSetLatencyInMicros",
        "Latency in executing GetUInt64ValueSet in the sharded lookup",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kShardedLookupRunQueryLatencyInMicros(
        "ShardedLookupRunQueryLatencyInMicros",
        "Latency in executing RunQuery in the sharded lookup",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kShardedLookupRunSetQueryUInt32LatencyInMicros(
        "ShardedLookupRunSetQueryUInt32LatencyInMicros",
        "Latency in executing RunSetQueryUInt32 in the sharded lookup",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kShardedLookupRunSetQueryUInt64LatencyInMicros(
        "ShardedLookupRunSetQueryUInt64LatencyInMicros",
        "Latency in executing RunSetQueryUInt64 in the sharded lookup",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kRemoteLookupGetValuesLatencyInMicros(
        "RemoteLookupGetValuesLatencyInMicros",
        "Latency in get values in the remote lookup",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kInternalRunQueryLatencyInMicros("InternalRunQueryLatencyInMicros",
                                     "Latency in internal run query call",
                                     kLatencyInMicroSecondsBoundaries,
                                     kMicroSecondsUpperBound,
                                     kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kInternalGetKeyValuesLatencyInMicros(
        "InternalGetKeyValuesLatencyInMicros",
        "Latency in internal get key values call",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kInternalGetKeyValueSetLatencyInMicros(
        "InternalGetKeyValueSetLatencyInMicros",
        "Latency in internal get key value set call",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kInternalGetUInt32ValueSetLatencyInMicros(
        "InternalGetUInt32ValueSetLatencyInMicros",
        "Latency in internal get uint32 value set call",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kInternalGetUInt64ValueSetLatencyInMicros(
        "InternalGetUInt64ValueSetLatencyInMicros",
        "Latency in internal get uint64 value set call",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kInternalLookupRequestError("InternalLookupRequestError",
                                "Errors in processing internal lookup request",
                                "error_code", kErrorMaxPartitionsContributed,
                                kInternalLookupRequestErrorCode,
                                kErrorCounterDPUpperBound,
                                kErrorCounterDPLowerBound,
                                kErrorMinNoiseToOutput);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kInternalSecureLookupLatencyInMicros("InternalSecureLookupLatencyInMicros",
                                         "Latency in internal secure lookup",
                                         kLatencyInMicroSecondsBoundaries,
                                         kMicroSecondsUpperBound,
                                         kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kGetValuePairsLatencyInMicros("GetValuePairsLatencyInMicros",
                                  "Latency in executing GetValuePairs in cache",
                                  kLatencyInMicroSecondsBoundaries,
                                  kMicroSecondsUpperBound,
                                  kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kGetKeyValueSetLatencyInMicros(
        "GetKeyValueSetLatencyInMicros",
        "Latency in executing GetKeyValueSet in cache",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kGetUInt32ValueSetLatencyInMicros(
        "GetUInt32ValueSetLatencyInMicros",
        "Latency in executing GetUInt32ValueSet in cache",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kGetUInt64ValueSetLatencyInMicros(
        "GetUInt64ValueSetLatencyInMicros",
        "Latency in executing GetUInt64ValueSet in cache",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kCacheAccessEventCount("CacheAccessEventCount",
                           "Count of cache hit or miss events by request",
                           "cache_access", 4 /*max_partitions_contributed*/,
                           kCacheAccessEvents, kCounterDPUpperBound,
                           kCounterDPLowerBound);

// Metric definitions for safe metrics that are not privacy impacting
inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kRequestFailedCountByStatus(
        "request.failed_count_by_status",
        "Total number of requests that resulted in failure partitioned by "
        "Error Code",
        "error_code", kAbslStatusStrings);
inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kGetParameterStatus("GetParameterStatus", "Get parameter status", "status",
                        kAbslStatusStrings);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kCompleteLifecycleStatus("CompleteLifecycleStatus",
                             "Server complete life cycle status", "status",
                             kAbslStatusStrings);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kCreateDataOrchestratorStatus("CreateDataOrchestratorStatus",
                                  "Data orchestrator creation status", "status",
                                  kAbslStatusStrings);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kStartDataOrchestratorStatus("StartDataOrchestratorStatus",
                                 "Data orchestrator start status count",
                                 "status", kAbslStatusStrings);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kLoadNewFilesStatus("LoadNewFilesStatus", "Load new file status", "status",
                        kAbslStatusStrings);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kGetShardManagerStatus("GetShardManagerStatus", "Get shard manager status",
                           "status", kAbslStatusStrings);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kDescribeInstanceGroupInstancesStatus(
        "DescribeInstanceGroupInstancesStatus",
        "Describe instance group instances status", "status",
        kAbslStatusStrings);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kDescribeInstancesStatus("DescribeInstancesStatus",
                             "Describe instances status", "status",
                             kAbslStatusStrings);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kReceivedLowLatencyNotificationsE2ECloudProvided(
        "ReceivedLowLatencyNotificationsE2ECloudProvided",
        "Time between cloud topic publisher inserting message and realtime "
        "notifier receiving the message",
        kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kReceivedLowLatencyNotificationsE2E(
        "ReceivedLowLatencyNotificationsE2E",
        "Time between producer producing the message and realtime notifier "
        "receiving the message",
        kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kReceivedLowLatencyNotifications(
        "ReceivedLowLatencyNotifications",
        "Latency in realtime notifier processing the received batch of "
        "notification messages",
        kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kReceivedLowLatencyNotificationsBytes(
        "ReceivedLowLatencyNotificationsBytes",
        "Size of messages received through pub/sub",
        privacy_sandbox::server_common::metrics::kSizeHistogram);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kReceivedLowLatencyNotificationsCount(
        "ReceivedLowLatencyNotificationsCount",
        "Count of messages received through pub/sub");

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kAwsSqsReceiveMessageLatency("AwsSqsReceiveMessageLatency",
                                 "AWS SQS receive message latency",
                                 kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kKVServerError("KVServerError", "Non request related server errors",
                   "error_code", kKVServerErrorCode);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kSeekingInputStreambufSizeLatency("SeekingInputStreambufSizeLatency",
                                      "Latency in seeking input streambuf size",
                                      kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kSeekingInputStreambufUnderflowLatency(
        "SeekingInputStreambufUnderflowLatency",
        "Latency in seeking input streambuf underflow",
        kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kSeekingInputStreambufSeekoffLatency(
        "SeekingInputStreambufSeekoffLatency",
        "Latency in seeking input streambuf seekoff",
        kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kBlobStorageReadBytes(
        "BlobStorageReadBytes", "Size of data read from blob storage in bytes",
        privacy_sandbox::server_common::metrics::kSizeHistogram);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kTotalRowsDroppedInDataLoading(
        "TotalRowsDroppedInDataLoading",
        "Total rows dropped during data loading from data source,"
        "data source can be a data file or realtime",
        "data_source",
        privacy_sandbox::server_common::metrics::kEmptyPublicPartition);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kTotalRowsUpdatedInDataLoading(
        "TotalRowsUpdatedInDataLoading",
        "Total rows updated during data loading from data source,"
        "data source can be a data file or realtime ",
        "data_source",
        privacy_sandbox::server_common::metrics::kEmptyPublicPartition);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kTotalRowsDeletedInDataLoading(
        "TotalRowsDeletedInDataLoading",
        "Total rows deleted during data loading from data source,"
        "data source can be a data file or realtime",
        "data_source",
        privacy_sandbox::server_common::metrics::kEmptyPublicPartition);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kConcurrentStreamRecordReaderReadShardRecordsLatency(
        "ConcurrentStreamRecordReaderReadShardRecordsLatency",
        "Latency in ConcurrentStreamRecordReader reading shard records",
        kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kConcurrentStreamRecordReaderReadStreamRecordsLatency(
        "ConcurrentStreamRecordReaderReadStreamRecordsLatency",
        "Latency in ConcurrentStreamRecordReader reading stream records",
        kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kConcurrentStreamRecordReaderReadByteRangeLatency(
        "ConcurrentStreamRecordReaderReadByteRangeLatency",
        "Latency in ConcurrentStreamRecordReader reading byte range",
        kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kUpdateKeyValueLatency("UpdateKeyValueLatency",
                           "Latency in key value update",
                           kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kUpdateKeyValueSetLatency("UpdateKeyValueSetLatency",
                              "Latency in key value set update",
                              kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kUpdateUInt32ValueSetLatency("UpdateUInt32ValueSetLatency",
                                 "Latency in uint32 key value set update",
                                 kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kUpdateUInt64ValueSetLatency("UpdateUInt64ValueSetLatency",
                                 "Latency in uint64 key value set update",
                                 kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kDeleteKeyLatency("DeleteKeyLatency", "Latency in deleting key",
                      kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kDeleteValuesInSetLatency("DeleteValuesInSetLatency",
                              "Latency in deleting values in set",
                              kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kDeleteUInt32ValueSetLatency("DeleteUInt32ValueSetLatency",
                                 "Latency in deleting values in an uint32 set",
                                 kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kDeleteUInt64ValueSetLatency("DeleteUInt64ValueSetLatency",
                                 "Latency in deleting values in an uint64 set",
                                 kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kRemoveDeletedKeyLatency(
        "RemoveDeletedKeyLatency",
        "Latency in removing deleted keys in the clean up process",
        kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kCleanUpKeyValueMapLatency("CleanUpKeyValueMapLatency",
                               "Latency in cleaning up key value map",
                               kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kCleanUpKeyValueSetMapLatency("CleanUpKeyValueSetMapLatency",
                                  "Latency in cleaning up key value set map",
                                  kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kCleanUpUIntSetMapLatency("CleanUpUIntSetMapMapLatency",
                              "Latency in cleaning up key value uint set maps",
                              kLatencyInMicroSecondsBoundaries);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kSecureLookupRequestCount(
        "SecureLookupRequestCount",
        "Number of secure lookup requests received from remote server");

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kTotalV2LatencyWithoutCustomCode(
        "TotalV2LatencyWithoutCustomCode",
        "Latency for running V2 request without UDF execution time",
        kLatencyInMicroSecondsBoundaries, kMicroSecondsUpperBound,
        kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kUDFExecutionLatencyInMicros("UDFExecutionLatencyInMicros",
                                 "UDF execution time",
                                 kLatencyInMicroSecondsBoundaries,
                                 kMicroSecondsUpperBound,
                                 kMicroSecondsLowerBound);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kTotalRequestCountV1("request.v1.count",
                         "Total number of V1 requests received by the server");

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kRequestFailedCountByStatusV1(
        "request.v1.failed_count_by_status",
        "Total number of V1 requests that resulted in failure partitioned by "
        "Error Code",
        "error_code", kAbslStatusStrings);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kServerTotalTimeMsV1(
        "request.v1.duration_ms",
        "Total time taken by the server to execute the request",
        privacy_sandbox::server_common::metrics::kTimeHistogram);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kResponseByteV1("response.v1.size_bytes", "V1 response size in bytes",
                    privacy_sandbox::server_common::metrics::kSizeHistogram);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kRequestByteV1("request.v1.size_bytes", "V1 request size in bytes",
                   privacy_sandbox::server_common::metrics::kSizeHistogram);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    int, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kGetValuesAdapterLatency(
        "GetValuesAdapterLatencyMs",
        "GetValues adapter latency in milliseconds",
        privacy_sandbox::server_common::metrics::kTimeHistogram);

// KV server metrics list contains non request related safe metrics
// and request metrics collected before stage of internal lookups
inline constexpr const privacy_sandbox::server_common::metrics::DefinitionName*
    kKVServerMetricList[] = {
        // Unsafe metrics
        &kKVUdfRequestError, &kShardedLookupKeyCountByShard,
        &kShardedLookupGetKeyValuesLatencyInMicros,
        &kShardedLookupGetKeyValueSetLatencyInMicros,
        &kShardedLookupRunQueryLatencyInMicros,
        &kShardedLookupRunSetQueryUInt32LatencyInMicros,
        &kRemoteLookupGetValuesLatencyInMicros,
        &kTotalV2LatencyWithoutCustomCode, &kUDFExecutionLatencyInMicros,
        &kShardedLookupGetUInt32ValueSetLatencyInMicros,
        &kShardedLookupGetUInt64ValueSetLatencyInMicros,
        &kShardedLookupRunSetQueryUInt64LatencyInMicros,
        // Safe metrics
        &kKVServerError,
        &privacy_sandbox::server_common::metrics::kTotalRequestCount,
        &privacy_sandbox::server_common::metrics::kServerTotalTimeMs,
        &privacy_sandbox::server_common::metrics::kRequestByte,
        &privacy_sandbox::server_common::metrics::kResponseByte,
        &kTotalRequestCountV1, &kServerTotalTimeMsV1, &kRequestByteV1,
        &kResponseByteV1, &kRequestFailedCountByStatusV1,
        &kRequestFailedCountByStatus, &kGetValuesAdapterLatency,
        &kGetParameterStatus, &kCompleteLifecycleStatus,
        &kCreateDataOrchestratorStatus, &kStartDataOrchestratorStatus,
        &kLoadNewFilesStatus, &kGetShardManagerStatus,
        &kDescribeInstanceGroupInstancesStatus, &kDescribeInstancesStatus,
        &kReceivedLowLatencyNotificationsE2ECloudProvided,
        &kReceivedLowLatencyNotificationsE2E, &kReceivedLowLatencyNotifications,
        &kReceivedLowLatencyNotificationsBytes,
        &kReceivedLowLatencyNotificationsCount, &kAwsSqsReceiveMessageLatency,
        &kSeekingInputStreambufSeekoffLatency,
        &kSeekingInputStreambufSizeLatency,
        &kSeekingInputStreambufUnderflowLatency,
        &kTotalRowsDroppedInDataLoading, &kTotalRowsUpdatedInDataLoading,
        &kTotalRowsDeletedInDataLoading,
        &kConcurrentStreamRecordReaderReadShardRecordsLatency,
        &kConcurrentStreamRecordReaderReadStreamRecordsLatency,
        &kConcurrentStreamRecordReaderReadByteRangeLatency,
        &kUpdateKeyValueLatency, &kUpdateKeyValueSetLatency,
        &kUpdateUInt32ValueSetLatency, &kDeleteKeyLatency,
        &kDeleteValuesInSetLatency, &kDeleteUInt32ValueSetLatency,
        &kRemoveDeletedKeyLatency, &kCleanUpKeyValueMapLatency,
        &kCleanUpKeyValueSetMapLatency, &kCleanUpUIntSetMapLatency,
        &kBlobStorageReadBytes, &kUpdateUInt64ValueSetLatency,
        &kDeleteUInt64ValueSetLatency};

// Internal lookup service metrics list contains metrics collected in the
// internal lookup server. This separation from KV metrics list allows all
// lookup requests (local and request from remote KV server) to contribute to
// the same set of metrics, so that the noise of unsafe metric won't be skewed
// for particular batch of requests, e.g server request that requires only
// remote lookups
inline constexpr const privacy_sandbox::server_common::metrics::DefinitionName*
    kInternalLookupServiceMetricsList[] = {
        // Safe metrics
        &kSecureLookupRequestCount,
        // Unsafe metrics
        &kInternalLookupRequestError, &kInternalRunQueryLatencyInMicros,
        &kInternalGetKeyValuesLatencyInMicros,
        &kInternalGetKeyValueSetLatencyInMicros,
        &kInternalSecureLookupLatencyInMicros, &kGetValuePairsLatencyInMicros,
        &kGetKeyValueSetLatencyInMicros, &kGetUInt32ValueSetLatencyInMicros,
        &kCacheAccessEventCount, &kGetUInt64ValueSetLatencyInMicros,
        &kInternalGetUInt32ValueSetLatencyInMicros,
        &kInternalGetUInt64ValueSetLatencyInMicros};

inline constexpr absl::Span<
    const privacy_sandbox::server_common::metrics::DefinitionName* const>
    kKVServerMetricSpan = kKVServerMetricList;

inline constexpr absl::Span<
    const privacy_sandbox::server_common::metrics::DefinitionName* const>
    kInternalLookupServiceMetricsSpan = kInternalLookupServiceMetricsList;

inline auto* KVServerContextMap(
    std::optional<
        privacy_sandbox::server_common::telemetry::BuildDependentConfig>
        config = std::nullopt,
    std::unique_ptr<opentelemetry::metrics::MeterProvider> provider = nullptr,
    absl::string_view service = kKVServerServiceName,
    absl::string_view version = "") {
  return privacy_sandbox::server_common::metrics::GetContextMap<
      const std::string, kKVServerMetricSpan>(std::move(config),
                                              std::move(provider), service,
                                              version, privacy_total_budget);
}

inline auto* InternalLookupServerContextMap(
    std::optional<
        privacy_sandbox::server_common::telemetry::BuildDependentConfig>
        config = std::nullopt,
    std::unique_ptr<opentelemetry::metrics::MeterProvider> provider = nullptr,
    absl::string_view service = kInternalLookupServiceName,
    absl::string_view version = "") {
  return privacy_sandbox::server_common::metrics::GetContextMap<
      const std::string, kInternalLookupServiceMetricsSpan>(
      std::move(config), std::move(provider), service, version,
      privacy_total_budget);
}

template <typename T>
inline void AddSystemMetric(T* context_map) {
  context_map->AddObserverable(
      privacy_sandbox::server_common::metrics::kCpuPercent,
      privacy_sandbox::server_common::GetCpu);
  context_map->AddObserverable(
      privacy_sandbox::server_common::metrics::kMemoryKB,
      privacy_sandbox::server_common::GetMemory);
}

inline void LogIfError(const absl::Status& s,
                       absl::string_view message = "when logging metric",
                       privacy_sandbox::server_common::SourceLocation location
                           PS_LOC_CURRENT_DEFAULT_ARG) {
  if (s.ok()) return;
  ABSL_LOG_EVERY_N_SEC(WARNING, 60)
          .AtLocation(location.file_name(), location.line())
      << message << ": " << s;
}

template <typename T>
inline void LogIfError(const absl::StatusOr<T>& s, std::string_view message,
                       privacy_sandbox::server_common::SourceLocation location
                           PS_LOC_CURRENT_DEFAULT_ARG) {
  if (s.ok()) return;
  ABSL_LOG_EVERY_N_SEC(WARNING, 60)
          .AtLocation(location.file_name(), location.line())
      << message << ": " << s.status();
}

template <const auto& definition>
inline absl::AnyInvocable<void(const absl::Status&, int) const>
LogStatusSafeMetricsFn() {
  return [](const absl::Status& status, int count) {
    LogIfError(KVServerContextMap()->SafeMetric().LogUpDownCounter<definition>(
        {{absl::StatusCodeToString(status.code()), count}}));
  };
}

inline absl::AnyInvocable<void(const absl::Status&, int) const>
LogMetricsNoOpCallback() {
  return [](const absl::Status& status, int count) {};
}

// Initialize metrics context map
// This is the minimum requirement to initialize the noop telemetry
inline void InitMetricsContextMap() {
  privacy_sandbox::server_common::telemetry::TelemetryConfig config_proto;
  config_proto.set_mode(
      privacy_sandbox::server_common::telemetry::TelemetryConfig::PROD);
  kv_server::KVServerContextMap(
      privacy_sandbox::server_common::telemetry::BuildDependentConfig(
          config_proto));
  kv_server::InternalLookupServerContextMap(
      privacy_sandbox::server_common::telemetry::BuildDependentConfig(
          config_proto));
}

using UdfRequestMetricsContext =
    privacy_sandbox::server_common::metrics::ServerContext<kKVServerMetricSpan>;
using InternalLookupMetricsContext =
    privacy_sandbox::server_common::metrics::ServerContext<
        kInternalLookupServiceMetricsSpan>;
using ServerSafeMetricsContext =
    privacy_sandbox::server_common::metrics::ServerSafeContext<
        kKVServerMetricSpan>;

inline void LogUdfRequestErrorMetric(UdfRequestMetricsContext& metrics_context,
                                     std::string_view error_code) {
  LogIfError(
      metrics_context.AccumulateMetric<kKVUdfRequestError>(1, error_code));
}

inline void LogInternalLookupRequestErrorMetric(
    InternalLookupMetricsContext& metrics_context,
    std::string_view error_code) {
  LogIfError(metrics_context.AccumulateMetric<kInternalLookupRequestError>(
      1, error_code));
}

// Logs non-request related error metrics
inline void LogServerErrorMetric(std::string_view error_code) {
  LogIfError(
      KVServerContextMap()->SafeMetric().LogUpDownCounter<kKVServerError>(
          {{std::string(error_code), 1}}));
}

// Logs common safe request metrics for V2 request path
template <typename RequestT, typename ResponseT>
inline void LogRequestCommonSafeMetrics(
    const RequestT* request, const ResponseT* response,
    const grpc::Status& grpc_request_status,
    const privacy_sandbox::server_common::Stopwatch& stopwatch) {
  LogIfError(
      KVServerContextMap()
          ->SafeMetric()
          .LogUpDownCounter<
              privacy_sandbox::server_common::metrics::kTotalRequestCount>(1));
  if (auto request_status =
          privacy_sandbox::server_common::ToAbslStatus(grpc_request_status);
      !request_status.ok()) {
    LogIfError(KVServerContextMap()
                   ->SafeMetric()
                   .LogUpDownCounter<kRequestFailedCountByStatus>(
                       {{absl::StatusCodeToString(request_status.code()), 1}}));
  }
  LogIfError(
      KVServerContextMap()
          ->SafeMetric()
          .LogHistogram<privacy_sandbox::server_common::metrics::kRequestByte>(
              static_cast<int>(request->ByteSizeLong())));
  LogIfError(
      KVServerContextMap()
          ->SafeMetric()
          .LogHistogram<privacy_sandbox::server_common::metrics::kResponseByte>(
              static_cast<int>(response->ByteSizeLong())));
  int duration_ms =
      static_cast<int>(absl::ToInt64Milliseconds(stopwatch.GetElapsedTime()));
  LogIfError(
      KVServerContextMap()
          ->SafeMetric()
          .LogHistogram<
              privacy_sandbox::server_common::metrics::kServerTotalTimeMs>(
              duration_ms));
}

// Logs safe V1 request metrics
template <typename RequestT, typename ResponseT>
inline void LogV1RequestCommonSafeMetrics(
    const RequestT* request, const ResponseT* response,
    const grpc::Status& grpc_request_status,
    const privacy_sandbox::server_common::Stopwatch& stopwatch) {
  LogIfError(
      KVServerContextMap()->SafeMetric().LogUpDownCounter<kTotalRequestCountV1>(
          1));
  if (auto request_status =
          privacy_sandbox::server_common::ToAbslStatus(grpc_request_status);
      !request_status.ok()) {
    LogIfError(KVServerContextMap()
                   ->SafeMetric()
                   .LogUpDownCounter<kRequestFailedCountByStatusV1>(
                       {{absl::StatusCodeToString(request_status.code()), 1}}));
  }
  LogIfError(KVServerContextMap()->SafeMetric().LogHistogram<kRequestByteV1>(
      static_cast<int>(request->ByteSizeLong())));
  LogIfError(KVServerContextMap()->SafeMetric().LogHistogram<kResponseByteV1>(
      static_cast<int>(response->ByteSizeLong())));
  int duration_ms =
      static_cast<int>(absl::ToInt64Milliseconds(stopwatch.GetElapsedTime()));
  LogIfError(
      KVServerContextMap()->SafeMetric().LogHistogram<kServerTotalTimeMsV1>(
          duration_ms));
}

// Measures the latency of a block of code. The latency is recorded in
// microseconds as histogram metrics when the object of this class goes
// out of scope. The metric can be either safe or unsafe metric.
template <typename ContextT, const auto& definition>
class ScopeLatencyMetricsRecorder {
 public:
  explicit ScopeLatencyMetricsRecorder<ContextT, definition>(
      ContextT& metrics_context,
      std::unique_ptr<privacy_sandbox::server_common::Stopwatch> stopwatch =
          std::make_unique<privacy_sandbox::server_common::Stopwatch>())
      : metrics_context_(metrics_context) {
    stopwatch_ = std::move(stopwatch);
  }
  ~ScopeLatencyMetricsRecorder<ContextT, definition>() {
    LogIfError(metrics_context_.template LogHistogram<definition>(
        absl::ToDoubleMicroseconds(stopwatch_->GetElapsedTime())));
  }
  // Returns the latency so far
  absl::Duration GetLatency() { return stopwatch_->GetElapsedTime(); }

 private:
  ContextT& metrics_context_;
  std::unique_ptr<privacy_sandbox::server_common::Stopwatch> stopwatch_;
};

}  // namespace kv_server

#endif  // COMPONENTS_TELEMETRY_SERVER_DEFINITION_H_
