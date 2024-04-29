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

#include "components/data/realtime/realtime_notifier.h"

#include <iostream>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/flags.h"
#include "absl/log/initialize.h"
#include "absl/strings/str_join.h"
#include "components/data/common/msg_svc.h"
#include "components/tools/publisher_service.h"
#include "components/tools/util/configure_telemetry_tools.h"
#include "components/util/platform_initializer.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/filename_utils.h"
#include "public/data_loading/readers/riegeli_stream_record_reader_factory.h"
#include "public/data_loading/records_utils.h"
#include "src/telemetry/telemetry_provider.h"

ABSL_FLAG(std::string, local_directory, "", "Local directory");

namespace kv_server {
namespace {
using kv_server::DataRecord;
using kv_server::DeserializeDataRecord;
using kv_server::GetRecordValue;
using kv_server::KeyValueMutationRecord;
using kv_server::Record;
using kv_server::Value;
using privacy_sandbox::server_common::TelemetryProvider;

std::unique_ptr<kv_server::MessageService> queue_manager_;

void Print(std::string string_decoded) {
  std::istringstream is(string_decoded);
  auto delta_stream_reader_factory =
      std::make_unique<kv_server::RiegeliStreamRecordReaderFactory>();
  auto record_reader = delta_stream_reader_factory->CreateReader(is);
  auto result = record_reader->ReadStreamRecords([](std::string_view raw) {
    return DeserializeDataRecord(raw, [](const DataRecord& data_record) {
      if (data_record.record_type() == Record::KeyValueMutationRecord) {
        const auto* record = data_record.record_as_KeyValueMutationRecord();
        auto update_type = "update";
        switch (record->mutation_type()) {
          case KeyValueMutationType::Delete: {
            update_type = "delete";
            break;
          }
        }
        auto format_value_func =
            [](const KeyValueMutationRecord& record) -> std::string {
          if (record.value_type() == Value::StringValue) {
            return std::string(GetRecordValue<std::string_view>(record));
          }
          if (record.value_type() == Value::StringSet) {
            return absl::StrJoin(
                GetRecordValue<std::vector<std::string_view>>(record), ",");
          }
          return "";
        };
        LOG(INFO) << "key: " << record->key()->string_view();
        LOG(INFO) << "value: " << format_value_func(*record);
        LOG(INFO) << "logical_commit_time: " << record->logical_commit_time();
        LOG(INFO) << "update_type: " << update_type;
      }
      return absl::OkStatus();
    });
    return absl::OkStatus();
  });
}

absl::Status Run() {
  PlatformInitializer initializer;
  NotifierMetadata metadata;
  kv_server::ConfigureTelemetryForTools();
// TODO(b/299623229): Remove CLOUD_PLATFORM_LOCAL macro and extract to
// publisher_service.
#if defined(CLOUD_PLATFORM_LOCAL)
  const std::string local_directory = absl::GetFlag(FLAGS_local_directory);
  if (!local_directory.empty()) {
    metadata = LocalNotifierMetadata{.local_directory = local_directory};
  }
  return absl::InvalidArgumentError(
      "Please specify a full set of parameters for the local platform.");
#else
  auto maybe_notifier_metadata = PublisherService::GetNotifierMetadata();
  if (!maybe_notifier_metadata.ok()) {
    return maybe_notifier_metadata.status();
  }
  metadata = std::move(*maybe_notifier_metadata);
#endif

  auto realtime_notifier_maybe = RealtimeNotifier::Create(metadata);
  if (!realtime_notifier_maybe.ok()) {
    return realtime_notifier_maybe.status();
  }
  auto realtime_notifier = std::move(*realtime_notifier_maybe);
  realtime_notifier->Start([](const std::string& message) {
    Print(message);
    DataLoadingStats stats;
    return stats;
  });
  LOG(INFO) << "Listening ....";
  absl::SleepFor(absl::InfiniteDuration());
  return absl::OkStatus();
}

}  // namespace
}  // namespace kv_server

int main(int argc, char* argv[]) {
  const std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  const absl::Status status = kv_server::Run();
  if (!status.ok()) {
    LOG(FATAL) << "Failed to run: " << status;
  }
  return 0;
}
