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
#include "scp/cc/core/common/uuid/src/uuid.h"
#include "src/cpp/metric/context_map.h"
#include "src/cpp/util/duration.h"
#include "src/cpp/util/read_system.h"

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
constexpr int kCounterDPLowerBound = 1;
constexpr int kCounterDPUpperBound = 10;

constexpr int kErrorCounterDPLowerBound = 0;
constexpr int kErrorCounterDPUpperBound = 1;
constexpr int kErrorMaxPartitionsContributed = 1;
constexpr double kErrorMinNoiseToOutput = 0.99;

constexpr int kMicroSecondsLowerBound = 1;
constexpr int kMicroSecondsUpperBound = 2'000'000'000;

inline constexpr double kLatencyInMicroSecondsBoundaries[] = {
    160,     220,       280,       320,       640,       1'200,         2'500,
    5'000,   10'000,    20'000,    40'000,    80'000,    160'000,       320'000,
    640'000, 1'000'000, 1'300'000, 2'600'000, 5'000'000, 10'000'000'000};

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
    privacy_total_budget{/*epsilon*/ 5};

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

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kHistogram>
    kShardedLookupRunQueryLatencyInMicros(
        "ShardedLookupRunQueryLatencyInMicros",
        "Latency in executing run query in the sharded lookup",
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
    privacy_sandbox::server_common::metrics::Instrument::kUpDownCounter>
    kRealtimeTotalRowsUpdated("RealtimeTotalRowsUpdated",
                              "Realtime total rows updated count");

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
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kTotalRowsDroppedInDataLoading(
        "TotalRowsDroppedInDataLoading",
        "Total rows dropped during data loading", "file_name",
        privacy_sandbox::server_common::metrics::kEmptyPublicPartition);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kTotalRowsUpdatedInDataLoading(
        "TotalRowsUpdatedInDataLoading",
        "Total rows updated during data loading", "file_name",
        privacy_sandbox::server_common::metrics::kEmptyPublicPartition);

inline constexpr privacy_sandbox::server_common::metrics::Definition<
    double, privacy_sandbox::server_common::metrics::Privacy::kNonImpacting,
    privacy_sandbox::server_common::metrics::Instrument::kPartitionedCounter>
    kTotalRowsDeletedInDataLoading(
        "TotalRowsDeletedInDataLoading",
        "Total rows deleted during data loading", "file_name",
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

// KV server metrics list contains contains non request related safe metrics
// and request metrics collected before stage of internal lookups
inline constexpr const privacy_sandbox::server_common::metrics::DefinitionName*
    kKVServerMetricList[] = {
        // Unsafe metrics
        &kKVUdfRequestError, &kShardedLookupRunQueryLatencyInMicros,
        &kRemoteLookupGetValuesLatencyInMicros,
        // Safe metrics
        &kKVServerError,
        &privacy_sandbox::server_common::metrics::kTotalRequestCount,
        &privacy_sandbox::server_common::metrics::kServerTotalTimeMs,
        &privacy_sandbox::server_common::metrics::kRequestByte,
        &privacy_sandbox::server_common::metrics::kResponseByte,
        &kRequestFailedCountByStatus, &kGetParameterStatus,
        &kCompleteLifecycleStatus, &kCreateDataOrchestratorStatus,
        &kStartDataOrchestratorStatus, &kLoadNewFilesStatus,
        &kGetShardManagerStatus, &kDescribeInstanceGroupInstancesStatus,
        &kDescribeInstancesStatus, &kRealtimeTotalRowsUpdated,
        &kReceivedLowLatencyNotificationsE2ECloudProvided,
        &kReceivedLowLatencyNotificationsE2E, &kReceivedLowLatencyNotifications,
        &kAwsSqsReceiveMessageLatency, &kSeekingInputStreambufSeekoffLatency,
        &kSeekingInputStreambufSizeLatency,
        &kSeekingInputStreambufUnderflowLatency,
        &kTotalRowsDroppedInDataLoading, &kTotalRowsUpdatedInDataLoading,
        &kTotalRowsDeletedInDataLoading,
        &kConcurrentStreamRecordReaderReadShardRecordsLatency,
        &kConcurrentStreamRecordReaderReadStreamRecordsLatency,
        &kConcurrentStreamRecordReaderReadByteRangeLatency,
        &kUpdateKeyValueLatency, &kUpdateKeyValueSetLatency, &kDeleteKeyLatency,
        &kDeleteValuesInSetLatency, &kRemoveDeletedKeyLatency,
        &kCleanUpKeyValueMapLatency, &kCleanUpKeyValueSetMapLatency};

// Internal lookup service metrics list contains metrics collected in the
// internal lookup server. This separation from KV metrics list allows all
// lookup requests (local and request from remote KV server) to contribute to
// the same set of metrics, so that the noise of unsafe metric won't be skewed
// for particular batch of requests, e.g server request that requires only
// remote lookups
inline constexpr const privacy_sandbox::server_common::metrics::DefinitionName*
    kInternalLookupServiceMetricsList[] = {
        // Unsafe metrics
        &kInternalLookupRequestError,
        &kInternalRunQueryLatencyInMicros,
        &kInternalGetKeyValuesLatencyInMicros,
        &kInternalGetKeyValueSetLatencyInMicros,
        &kInternalSecureLookupLatencyInMicros,
        &kGetValuePairsLatencyInMicros,
        &kGetKeyValueSetLatencyInMicros,
        &kCacheAccessEventCount};

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

// Logs common safe request metrics
template <typename RequestT, typename ResponseT>
inline void LogRequestCommonSafeMetrics(
    const RequestT* request, const ResponseT* response,
    const grpc::Status& grpc_request_status,
    const absl::Time& request_received_time) {
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
  LogIfError(KVServerContextMap()
                 ->SafeMetric()
                 .template LogHistogram<
                     privacy_sandbox::server_common::metrics::kRequestByte>(
                     (int)request->ByteSizeLong()));
  LogIfError(KVServerContextMap()
                 ->SafeMetric()
                 .template LogHistogram<
                     privacy_sandbox::server_common::metrics::kResponseByte>(
                     (int)response->ByteSizeLong()));
  int duration_ms =
      (absl::Now() - request_received_time) / absl::Milliseconds(1);
  LogIfError(
      KVServerContextMap()
          ->SafeMetric()
          .LogHistogram<
              privacy_sandbox::server_common::metrics::kServerTotalTimeMs>(
              duration_ms));
}

// ScopeMetricsContext provides metrics context ties to the request and
// should have the same lifetime of the request.
// The purpose of this class is to avoid explicit creating and deleting metrics
// context from context map. The metrics context associated with the request
// will be destroyed after ScopeMetricsContext goes out of scope.
class ScopeMetricsContext {
 public:
  explicit ScopeMetricsContext(
      std::string request_id = google::scp::core::common::ToString(
          google::scp::core::common::Uuid::GenerateUuid()))
      : request_id_(std::move(request_id)) {
    // Create a metrics context in the context map and
    // associated it with request id
    KVServerContextMap()->Get(&request_id_);
    CHECK_OK([this]() {
      // Remove the metrics context for request_id to transfer the ownership
      // of metrics context to the ScopeMetricsContext. This is to ensure that
      // metrics context has the same lifetime with RequestContext and be
      // destroyed when ScopeMetricsContext goes out of scope.
      PS_ASSIGN_OR_RETURN(udf_request_metrics_context_,
                          KVServerContextMap()->Remove(&request_id_));
      return absl::OkStatus();
    }()) << "Udf request metrics context is not initialized";
    InternalLookupServerContextMap()->Get(&request_id_);
    CHECK_OK([this]() {
      // Remove the metrics context for request_id to transfer the ownership
      // of metrics context to the ScopeMetricsContext. This is to ensure that
      // metrics context has the same lifetime with RequestContext and be
      // destroyed when ScopeMetricsContext goes out of scope.
      PS_ASSIGN_OR_RETURN(
          internal_lookup_metrics_context_,
          InternalLookupServerContextMap()->Remove(&request_id_));
      return absl::OkStatus();
    }()) << "Internal lookup metrics context is not initialized";
  }
  UdfRequestMetricsContext& GetUdfRequestMetricsContext() const {
    return *udf_request_metrics_context_;
  }
  InternalLookupMetricsContext& GetInternalLookupMetricsContext() const {
    return *internal_lookup_metrics_context_;
  }

 private:
  const std::string request_id_;
  // Metrics context has the same lifetime of server request context
  std::unique_ptr<UdfRequestMetricsContext> udf_request_metrics_context_;
  std::unique_ptr<InternalLookupMetricsContext>
      internal_lookup_metrics_context_;
};

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
