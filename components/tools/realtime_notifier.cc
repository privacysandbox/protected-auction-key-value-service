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
#include "absl/strings/str_join.h"
#include "components/data/common/msg_svc.h"
#include "components/util/platform_initializer.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/filename_utils.h"
#include "public/data_loading/readers/riegeli_stream_io.h"
#include "public/data_loading/records_utils.h"
#include "src/cpp/telemetry/telemetry_provider.h"

ABSL_FLAG(std::string, gcp_project_id, "", "GCP project id");
ABSL_FLAG(std::string, gcp_topic_id, "", "GCP topic id");
ABSL_FLAG(std::string, aws_sns_arn, "", "AWS sns arn");
ABSL_FLAG(std::string, local_directory, "", "Local directory");

namespace kv_server {
namespace {
const char kQueuePrefix[] = "QueueNotifier_";

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
      StreamRecordReaderFactory<std::string_view>::Create();
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
          if (record.value_type() == Value::String) {
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

absl::StatusOr<NotifierMetadata> GetNotifierMetadata() {
  const std::string gcp_project_id = absl::GetFlag(FLAGS_gcp_project_id);
  const std::string gcp_topic_id = absl::GetFlag(FLAGS_gcp_topic_id);
  if (!gcp_project_id.empty() && !gcp_topic_id.empty()) {
    return GcpNotifierMetadata{.project_id = gcp_project_id,
                               .topic_id = gcp_topic_id,
                               .queue_prefix = kQueuePrefix};
  }
  const std::string sns_arn = absl::GetFlag(FLAGS_aws_sns_arn);
  if (!sns_arn.empty()) {
    auto metadata =
        AwsNotifierMetadata{.sns_arn = sns_arn, .queue_prefix = kQueuePrefix};

    auto maybe_queue_manager = MessageService::Create(metadata);
    if (!maybe_queue_manager.ok()) {
      return maybe_queue_manager.status();
    }
    queue_manager_ = std::move(*maybe_queue_manager);
    metadata.queue_manager = queue_manager_.get();
    return metadata;
  }
  const std::string local_directory = absl::GetFlag(FLAGS_local_directory);
  if (!local_directory.empty()) {
    return LocalNotifierMetadata{.local_directory = local_directory};
  }
  return absl::InvalidArgumentError(
      "Please specify a full set of parameters at least for one of the "
      "following platforms: local, GCP or AWS.");
}

absl::Status Run() {
  PlatformInitializer initializer;
  auto maybe_notifier_metadata = GetNotifierMetadata();
  if (!maybe_notifier_metadata.ok()) {
    return maybe_notifier_metadata.status();
  }
  auto noop_metrics_recorder =
      TelemetryProvider::GetInstance().CreateMetricsRecorder();
  auto realtime_notifier_maybe = RealtimeNotifier::Create(
      *noop_metrics_recorder, std::move(*maybe_notifier_metadata));
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
  google::InitGoogleLogging(argv[0]);
  const absl::Status status = kv_server::Run();
  if (!status.ok()) {
    LOG(FATAL) << "Failed to run: " << status;
  }
  return 0;
}
