// Copyright 2022 Google LLC
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
#include <iostream>
#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "components/data/blob_storage/delta_file_notifier.h"
#include "components/data_server/cache/cache.h"
#include "components/data_server/cache/key_value_cache.h"
#include "components/data_server/data_loading/data_orchestrator.h"
#include "components/tools/util/configure_telemetry_tools.h"
#include "components/udf/noop_udf_client.h"
#include "components/util/platform_initializer.h"
#include "public/base_types.pb.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/readers/riegeli_stream_io.h"
#include "public/data_loading/readers/riegeli_stream_record_reader_factory.h"
#include "public/sharding/key_sharder.h"

ABSL_FLAG(std::vector<std::string>, operations,
          std::vector<std::string>({"PASS_THROUGH", "READ_ONLY", "CACHE"}),
          "operations to test");
ABSL_FLAG(std::string, bucket, "performance-test-data-bucket",
          "Bucket to read files from");

namespace kv_server {
namespace {

class DataLoadingAnalyzerLogContext
    : public privacy_sandbox::server_common::log::SafePathContext {
 public:
  DataLoadingAnalyzerLogContext() = default;
};

class NoopBlobStorageChangeNotifier : public BlobStorageChangeNotifier {
 public:
  absl::StatusOr<std::vector<std::string>> GetNotifications(
      absl::Duration max_wait,
      const std::function<bool()>& should_stop_callback) override {
    return std::vector<std::string>();
  }
};

class NoopRealtimeThreadPoolManager : public RealtimeThreadPoolManager {
 public:
  absl::Status Start(
      std::function<absl::StatusOr<DataLoadingStats>(const std::string& key)>
          callback) override {
    return absl::OkStatus();
  }
  absl::Status Stop() override { return absl::OkStatus(); }
};

class NoopReader : public StreamRecordReader {
  absl::StatusOr<KVFileMetadata> GetKVFileMetadata() override {
    return KVFileMetadata();
  }
  absl::Status ReadStreamRecords(
      const std::function<absl::Status(const std::string_view&)>& callback)
      override {
    return absl::OkStatus();
  }
};

// Reader that only reads the stream.
// Stateless and thread-safe.
class PassThroughStreamReaderFactory : public StreamRecordReaderFactory {
 public:
  std::unique_ptr<StreamRecordReader> CreateReader(
      std::istream& data_input) const override {
    std::ofstream devnull("/dev/null");
    devnull << data_input.rdbuf();
    devnull.close();
    return std::make_unique<NoopReader>();
  }
  std::unique_ptr<StreamRecordReader> CreateConcurrentReader(
      std::function<std::unique_ptr<RecordStream>()> stream_factory)
      const override {
    return nullptr;
  }
};

class ReadonlyStreamReaderFactory : public StreamRecordReaderFactory {
 public:
  std::unique_ptr<StreamRecordReader> CreateReader(
      std::istream& data_input) const override {
    auto reader = riegeli::RecordReader(riegeli::IStreamReader(&data_input));
    absl::Cleanup reader_closer([&reader] { reader.Close(); });
    std::string_view raw;
    while (reader.ReadRecord(raw)) {
      auto record = flatbuffers::GetRoot<KeyValueMutationRecord>(raw.data());
      if (record->logical_commit_time() == 0) {
        LOG(INFO) << "This is a dummy log line (that should not be called) in "
                     "order to read the record. A logical commit time of 0 is "
                     "not expected.";
      }
    }
    return std::make_unique<NoopReader>();
  }
  std::unique_ptr<StreamRecordReader> CreateConcurrentReader(
      std::function<std::unique_ptr<RecordStream>()> stream_factory)
      const override {
    return nullptr;
  }
};

enum class Operation {
  kPassThrough = 0,
  kReadOnly,
  kCache,
};

std::vector<Operation> OperationsFromFlag() {
  const std::vector<std::string> operations = absl::GetFlag(FLAGS_operations);
  std::vector<Operation> results;
  for (const auto& op : operations) {
    if (op == "PASS_THROUGH") {
      results.push_back(Operation::kPassThrough);
    }
    if (op == "READ_ONLY") {
      results.push_back(Operation::kReadOnly);
    }
    if (op == "CACHE") {
      results.push_back(Operation::kCache);
    }
  }
  return results;
}

absl::Status InitOnce(Operation operation) {
  DataLoadingAnalyzerLogContext log_context;
  std::unique_ptr<UdfClient> noop_udf_client = NewNoopUdfClient();
  ConfigureTelemetryForTools();
  std::unique_ptr<Cache> cache = KeyValueCache::Create();

  std::unique_ptr<BlobStorageClientFactory> blob_storage_client_factory =
      BlobStorageClientFactory::Create();
  std::unique_ptr<BlobStorageClient> blob_client =
      blob_storage_client_factory->CreateBlobStorageClient();
  std::unique_ptr<DeltaFileNotifier> notifier =
      DeltaFileNotifier::Create(*blob_client);
  std::unique_ptr<StreamRecordReaderFactory> delta_stream_reader_factory;
  switch (operation) {
    case Operation::kPassThrough:
      LOG(INFO) << "Initializing by passing through the stream";
      delta_stream_reader_factory =
          std::make_unique<PassThroughStreamReaderFactory>();
      break;
    case Operation::kReadOnly:
      LOG(INFO)
          << "Initializing by building records but not processing the records";
      delta_stream_reader_factory =
          std::make_unique<ReadonlyStreamReaderFactory>();
      break;
    case Operation::kCache:
    default:
      LOG(INFO) << "Initializing fully";
      delta_stream_reader_factory =
          std::make_unique<RiegeliStreamRecordReaderFactory>();
      break;
  }
  NoopBlobStorageChangeNotifier change_notifier;
  // Blocks until cache is initialized
  absl::StatusOr<std::unique_ptr<DataOrchestrator>> maybe_data_orchestrator;
  absl::Time start_time = absl::Now();
  NoopRealtimeThreadPoolManager realtime_thread_pool_manager;
  maybe_data_orchestrator = DataOrchestrator::TryCreate({
      .data_bucket = absl::GetFlag(FLAGS_bucket),
      .cache = *cache,
      .blob_client = *blob_client,
      .delta_notifier = *notifier,
      .change_notifier = change_notifier,
      .delta_stream_reader_factory = *delta_stream_reader_factory,
      .realtime_thread_pool_manager = realtime_thread_pool_manager,
      .udf_client = *noop_udf_client,
      .key_sharder = KeySharder(ShardingFunction{/*seed=*/""}),
      .log_context = log_context,
  });
  absl::Time end_time = absl::Now();
  LOG(INFO) << "Init used " << (end_time - start_time);
  return maybe_data_orchestrator.status();
}
}  // namespace
absl::Status Run() {
  kv_server::PlatformInitializer initializer;

  const std::vector<Operation> operations = OperationsFromFlag();
  LOG(INFO) << "Performing " << operations.size() << " operations";
  for (const auto op : operations) {
    if (const auto s = InitOnce(op); !s.ok()) {
      return s;
    }
  }
  return absl::OkStatus();
}
}  // namespace kv_server
int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  const absl::Status status = kv_server::Run();
  if (!status.ok()) {
    LOG(FATAL) << "Failed to run: " << status;
  }
  return 0;
}
