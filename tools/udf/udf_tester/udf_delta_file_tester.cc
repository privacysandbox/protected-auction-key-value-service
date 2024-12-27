// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <fstream>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/flags.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/substitute.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/key_value_cache.h"
#include "components/internal_server/local_lookup.h"
#include "components/tools/util/configure_telemetry_tools.h"
#include "components/udf/hooks/get_values_hook.h"
#include "components/udf/udf_client.h"
#include "components/udf/udf_config_builder.h"
#include "google/protobuf/util/json_util.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/readers/delta_record_stream_reader.h"
#include "public/query/v2/get_values_v2.pb.h"
#include "public/udf/constants.h"
#include "src/telemetry/telemetry_provider.h"
#include "src/util/status_macro/status_macros.h"

ABSL_FLAG(std::string, kv_delta_file_path, "",
          "Path to delta file with KV pairs.");
ABSL_FLAG(std::string, udf_delta_file_path, "", "Path to UDF delta file.");
ABSL_FLAG(std::string, input_arguments, "",
          "List of input arguments in JSON format. Each input argument should "
          "be equivalent to a UDFArgument.");

namespace kv_server {
namespace {
class UDFDeltaFileTestLogContext
    : public privacy_sandbox::server_common::log::SafePathContext {
 public:
  UDFDeltaFileTestLogContext() = default;
};
}  // namespace

using google::protobuf::util::JsonStringToMessage;

// If the arg is const&, the Span construction complains about converting const
// string_view to non-const string_view. Since this tool is for simple testing,
// the current solution is to pass by value.
absl::Status LoadCacheFromKVMutationRecord(
    UDFDeltaFileTestLogContext& log_context, KeyValueMutationRecordT record,
    Cache& cache) {
  switch (record.mutation_type) {
    case KeyValueMutationType::Update: {
      LOG(INFO) << "Updating cache with key " << record.key
                << ", logical commit time " << record.logical_commit_time;
      if (record.value.type == Value::StringValue) {
        cache.UpdateKeyValue(log_context, record.key,
                             record.value.AsStringValue()->value,
                             record.logical_commit_time);
        return absl::OkStatus();
      }
      if (record.value.type == Value::StringSet) {
        std::vector<std::string> values_str = record.value.AsStringSet()->value;
        std::vector<std::string_view> values(values_str.begin(),
                                             values_str.end());
        cache.UpdateKeyValueSet(log_context, record.key, absl::MakeSpan(values),
                                record.logical_commit_time);
        return absl::OkStatus();
      }
      if (record.value.type == Value::UInt32Set) {
        auto values = record.value.AsUInt32Set()->value;
        cache.UpdateKeyValueSet(log_context, record.key, absl::MakeSpan(values),
                                record.logical_commit_time);
        return absl::OkStatus();
      }
      if (record.value.type == Value::UInt64Set) {
        auto values = record.value.AsUInt64Set()->value;
        cache.UpdateKeyValueSet(log_context, record.key, absl::MakeSpan(values),
                                record.logical_commit_time);
        return absl::OkStatus();
      }
      return absl::InvalidArgumentError(
          absl::StrCat("Record with key: ", record.key,
                       " has unsupported value type: ", record.value.type));
    }
    case KeyValueMutationType::Delete: {
      cache.DeleteKey(log_context, record.key, record.logical_commit_time);
      break;
    }
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid mutation type: ", record.mutation_type));
  }
  return absl::OkStatus();
}

absl::Status LoadCacheFromFile(UDFDeltaFileTestLogContext& log_context,
                               std::string file_path, Cache& cache) {
  std::ifstream delta_file(file_path);
  DeltaRecordStreamReader record_reader(delta_file);
  absl::Status status = record_reader.ReadRecords(
      [&cache, &log_context](const DataRecord& data_record) {
        DataRecordT data_record_struct;
        data_record.UnPackTo(&data_record_struct);
        // Only load KVMutationRecords into cache.
        if (data_record_struct.record.type == Record::KeyValueMutationRecord) {
          return LoadCacheFromKVMutationRecord(
              log_context,
              *data_record_struct.record.AsKeyValueMutationRecord(), cache);
        }
        return absl::OkStatus();
      });
  return status;
}

void ReadCodeConfigFromUdfConfig(const UserDefinedFunctionsConfigT& udf_config,
                                 CodeConfig& code_config) {
  code_config.js = udf_config.code_snippet;
  code_config.logical_commit_time = udf_config.logical_commit_time;
  code_config.udf_handler_name = udf_config.handler_name;
  code_config.version = udf_config.version;
}

absl::Status ReadCodeConfigFromFile(std::string file_path,
                                    CodeConfig& code_config) {
  std::ifstream delta_file(file_path);
  DeltaRecordStreamReader record_reader(delta_file);
  return record_reader.ReadRecords([&code_config](
                                       const DataRecord& data_record) {
    DataRecordT data_record_struct;
    data_record.UnPackTo(&data_record_struct);
    if (data_record_struct.record.type == Record::UserDefinedFunctionsConfig) {
      ReadCodeConfigFromUdfConfig(
          *data_record_struct.record.AsUserDefinedFunctionsConfig(),
          code_config);
      return absl::OkStatus();
    }
    return absl::InvalidArgumentError("Invalid record type.");
  });
}

void ShutdownUdf(UdfClient& udf_client) {
  auto udf_client_stop = udf_client.Stop();
  if (!udf_client_stop.ok()) {
    LOG(ERROR) << "Error shutting down UDF execution engine: "
               << udf_client_stop;
  }
}

absl::Status TestUdf(const std::string& kv_delta_file_path,
                     const std::string& udf_delta_file_path,
                     const std::string& input_arguments) {
  ConfigureTelemetryForTools();
  LOG(INFO) << "Loading cache from delta file: " << kv_delta_file_path;
  std::unique_ptr<Cache> cache = KeyValueCache::Create();
  UDFDeltaFileTestLogContext log_context;
  PS_RETURN_IF_ERROR(LoadCacheFromFile(log_context, kv_delta_file_path, *cache))
      << "Error loading cache from file";

  LOG(INFO) << "Loading udf code config from delta file: "
            << udf_delta_file_path;
  CodeConfig code_config;
  PS_RETURN_IF_ERROR(ReadCodeConfigFromFile(udf_delta_file_path, code_config))
      << "Error loading UDF code from file";

  LOG(INFO) << "Starting UDF client";
  UdfConfigBuilder config_builder;
  auto string_get_values_hook =
      GetValuesHook::Create(GetValuesHook::OutputType::kString);
  string_get_values_hook->FinishInit(CreateLocalLookup(*cache));
  auto binary_get_values_hook =
      GetValuesHook::Create(GetValuesHook::OutputType::kBinary);
  binary_get_values_hook->FinishInit(CreateLocalLookup(*cache));
  auto run_set_query_string_hook = RunSetQueryStringHook::Create();
  run_set_query_string_hook->FinishInit(CreateLocalLookup(*cache));
  absl::StatusOr<std::unique_ptr<UdfClient>> udf_client =
      UdfClient::Create(std::move(
          config_builder.RegisterStringGetValuesHook(*string_get_values_hook)
              .RegisterBinaryGetValuesHook(*binary_get_values_hook)
              .RegisterRunSetQueryStringHook(*run_set_query_string_hook)
              .RegisterLoggingHook()
              .SetNumberOfWorkers(1)
              .Config()));
  PS_RETURN_IF_ERROR(udf_client.status())
      << "Error starting UDF execution engine";

  auto code_object_status =
      udf_client.value()->SetCodeObject(code_config, log_context);
  if (!code_object_status.ok()) {
    LOG(ERROR) << "Error setting UDF code object: " << code_object_status;
    ShutdownUdf(*udf_client.value());
    return code_object_status;
  }

  v2::RequestPartition req_partition;
  std::string req_partition_json =
      absl::StrCat("{arguments: ", input_arguments, "}");
  LOG(INFO) << "req_partition_json: " << req_partition_json;

  JsonStringToMessage(req_partition_json, &req_partition);

  LOG(INFO) << "Calling UDF for partition: " << req_partition.DebugString();
  auto request_context_factory = std::make_unique<RequestContextFactory>(
      privacy_sandbox::server_common::LogContext(),
      privacy_sandbox::server_common::ConsentedDebugConfiguration());
  ExecutionMetadata execution_metadata;
  auto udf_result = udf_client.value()->ExecuteCode(
      *request_context_factory, {}, req_partition.arguments(),
      execution_metadata);
  if (!udf_result.ok()) {
    LOG(ERROR) << "UDF execution failed: " << udf_result.status();
    ShutdownUdf(*udf_client.value());
    return udf_result.status();
  }
  ShutdownUdf(*udf_client.value());

  LOG(INFO) << "UDF execution result: " << udf_result.value();
  std::cout << "UDF execution result: " << udf_result.value() << std::endl;

  return absl::OkStatus();
}

}  // namespace kv_server

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  const std::string kv_delta_file_path =
      absl::GetFlag(FLAGS_kv_delta_file_path);
  const std::string udf_delta_file_path =
      absl::GetFlag(FLAGS_udf_delta_file_path);
  const std::string input_arguments = absl::GetFlag(FLAGS_input_arguments);

  auto status = kv_server::TestUdf(kv_delta_file_path, udf_delta_file_path,
                                   input_arguments);
  if (!status.ok()) {
    return -1;
  }
  return 0;
}
