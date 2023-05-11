/*
 * Copyright 2022 Google LLC
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

#include "tools/data_cli/commands/generate_snapshot_command.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "public/constants.h"
#include "public/data_loading/filename_utils.h"
#include "public/data_loading/readers/delta_record_stream_reader.h"
#include "public/data_loading/riegeli_metadata.pb.h"
#include "src/cpp/telemetry/telemetry_provider.h"

namespace kv_server {
namespace {

using privacy_sandbox::server_common::TelemetryProvider;

// The working_dir is always on local disk but the BlobStorageClient that this
// CLI is compiled with may support either S3 or local, not both.  So we need a
// BlobReader that can read the local temp data for writing to the Client.
class FileBlobReader : public BlobReader {
 public:
  explicit FileBlobReader(const std::string& filename)
      : file_stream_(filename) {}
  ~FileBlobReader() = default;
  std::istream& Stream() override { return file_stream_; }
  bool CanSeek() const override { return true; }

 private:
  std::ifstream file_stream_;
};

constexpr std::string_view kStdioSymbol = "-";

absl::Status ValidateRequiredParams(GenerateSnapshotCommand::Params& params) {
  if (params.working_dir.empty()) {
    return absl::InvalidArgumentError("Working directory is required.");
  }
  if (!std::filesystem::exists(params.working_dir)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Working directory: ", params.working_dir, " does not exist."));
  }
  if (params.data_dir.empty()) {
    return absl::InvalidArgumentError("Data directory is required.");
  }
  if (params.starting_file.empty() ||
      (!IsDeltaFilename(params.starting_file) &&
       !IsSnapshotFilename(params.starting_file))) {
    return absl::InvalidArgumentError(
        "Starting file must be a valid delta or snapshot filename.");
  }
  if (params.ending_delta_file.empty() ||
      !IsDeltaFilename(params.ending_delta_file)) {
    return absl::InvalidArgumentError("Ending delta file is not valid.");
  }
  return absl::OkStatus();
}

std::filesystem::path GetTempAggregatorDbFile(
    const GenerateSnapshotCommand::Params& params) {
  auto filename = std::filesystem::path(params.working_dir) /
                  absl::StrFormat("RecordAggregator.%d.db", std::rand());
  // Try to remove the file in case it already exists.
  std::filesystem::remove(filename);
  return filename;
}

std::filesystem::path GetTempSnapshotFile(
    const GenerateSnapshotCommand::Params& params) {
  if (params.snapshot_file == kStdioSymbol) {
    return "";
  }
  auto file_path =
      std::filesystem::path(params.working_dir) / params.snapshot_file;
  std::filesystem::remove(file_path);
  return file_path;
}

absl::StatusOr<KVFileMetadata> CreateSnapshotMetadata(
    const GenerateSnapshotCommand::Params& params) {
  KVFileMetadata metadata;
  auto snapshot_metadata = metadata.mutable_snapshot();
  *snapshot_metadata->mutable_starting_file() = params.starting_file;
  *snapshot_metadata->mutable_ending_delta_file() = params.ending_delta_file;
  return metadata;
}

void ResetInputStream(std::istream& istream) {
  istream.clear();
  istream.seekg(0, std::ios::beg);
}

absl::StatusOr<std::string> WriteBaseSnapshotData(
    const GenerateSnapshotCommand::Params& params,
    BlobStorageClient& blob_client,
    SnapshotStreamWriter<std::ostream>& snapshot_writer) {
  LOG(INFO) << "Compacting base snapshot file: " << params.starting_file;
  auto blob_reader = blob_client.GetBlobReader(
      {.bucket = params.data_dir.data(), .key = params.starting_file.data()});
  DeltaRecordStreamReader record_reader(blob_reader->Stream());
  auto metadata = record_reader.ReadMetadata();
  if (!metadata.ok()) {
    return metadata.status();
  }
  if (blob_reader->CanSeek()) {
    ResetInputStream(blob_reader->Stream());
  } else {
    blob_reader = blob_client.GetBlobReader(
        {.bucket = params.data_dir.data(), .key = params.starting_file.data()});
  }
  if (auto status = snapshot_writer.WriteRecordStream(blob_reader->Stream());
      !status.ok()) {
    return status;
  }
  LOG(INFO) << "Successfully compacted base snapshot file: "
            << params.starting_file;
  return metadata->snapshot().ending_delta_file();
}

absl::Status WriteDeltaFilesToSnapshot(
    const std::vector<std::string>& delta_files,
    const GenerateSnapshotCommand::Params& params,
    BlobStorageClient& blob_client,
    SnapshotStreamWriter<std::ostream>& snapshot_writer) {
  for (const auto& delta_file : delta_files) {
    LOG(INFO) << "Compacting delta file: " << delta_file;
    if (!IsDeltaFilename(delta_file)) {
      LOG(INFO) << "Skipping invalid delta filename: " << delta_file;
      continue;
    }
    if (params.ending_delta_file < delta_file) {
      LOG(INFO) << "Delta file " << delta_file
                << "is out of range. So we are done processing, skippping it.";
      break;
    }
    auto blob_reader = blob_client.GetBlobReader(
        {.bucket = params.data_dir.data(), .key = delta_file});
    DeltaRecordStreamReader record_reader(blob_reader->Stream());
    auto metadata = record_reader.ReadMetadata();
    if (!metadata.ok()) {
      return metadata.status();
    }
    if (blob_reader->CanSeek()) {
      ResetInputStream(blob_reader->Stream());
    } else {
      blob_reader = blob_client.GetBlobReader(
          {.bucket = params.data_dir.data(), .key = delta_file});
    }
    if (auto status = snapshot_writer.WriteRecordStream(blob_reader->Stream());
        !status.ok()) {
      return status;
    }
    LOG(INFO) << "Successfully compacted delta file: " << delta_file;
  }
  return absl::OkStatus();
}
}  // namespace

GenerateSnapshotCommand::GenerateSnapshotCommand(
    GenerateSnapshotCommand::Params params,
    std::unique_ptr<BlobStorageClient> blob_client)
    : params_(std::move(params)), blob_client_(std::move(blob_client)) {}

GenerateSnapshotCommand::~GenerateSnapshotCommand() {
  std::filesystem::remove(GetTempSnapshotFile(params_));
  if (!params_.in_memory_compaction) {
    std::filesystem::remove(GetTempAggregatorDbFile(params_));
  }
}

absl::StatusOr<std::unique_ptr<GenerateSnapshotCommand>>
GenerateSnapshotCommand::Create(Params params) {
  if (absl::Status status = ValidateRequiredParams(params); !status.ok()) {
    return status;
  }
  auto noop_metrics_recorder =
      TelemetryProvider::GetInstance().CreateMetricsRecorder();
  auto blob_client = BlobStorageClient::Create(*noop_metrics_recorder);
  return absl::WrapUnique(
      new GenerateSnapshotCommand(std::move(params), std::move(blob_client)));
}

absl::Status GenerateSnapshotCommand::Execute() {
  auto snapshot_metadata = CreateSnapshotMetadata(params_);
  if (!snapshot_metadata.ok()) {
    return snapshot_metadata.status();
  }
  const std::filesystem::path temp_snapshot(GetTempSnapshotFile(params_));
  std::ofstream snapshot_ofstream(temp_snapshot);
  std::ostream* snapshot_ostream =
      params_.snapshot_file == kStdioSymbol ? &std::cout : &snapshot_ofstream;
  auto snapshot_writer = SnapshotStreamWriter<std::ostream>::Create(
      {.metadata = *snapshot_metadata,
       .temp_data_file = params_.in_memory_compaction
                             ? ""
                             : GetTempAggregatorDbFile(params_)},
      *snapshot_ostream);
  if (!snapshot_writer.ok()) {
    return snapshot_writer.status();
  }
  std::string_view start_after_delta_file = params_.starting_file;
  if (IsSnapshotFilename(params_.starting_file)) {
    auto snapshot_end_file =
        WriteBaseSnapshotData(params_, *blob_client_, **snapshot_writer);
    if (!snapshot_end_file.ok()) {
      return snapshot_end_file.status();
    }
    start_after_delta_file = *snapshot_end_file;
  }
  auto delta_files =
      blob_client_->ListBlobs({.bucket = params_.data_dir},
                              {.prefix = FilePrefix<FileType::DELTA>().data(),
                               .start_after = start_after_delta_file.data()});
  if (!delta_files.ok()) {
    return delta_files.status();
  }
  if (IsDeltaFilename(params_.starting_file)) {
    delta_files->insert(delta_files->begin(), start_after_delta_file.data());
  }
  if (auto status = WriteDeltaFilesToSnapshot(*delta_files, params_,
                                              *blob_client_, **snapshot_writer);
      !status.ok()) {
    return status;
  }
  if (auto status = (*snapshot_writer)->Finalize(); !status.ok()) {
    return status;
  }
  snapshot_ofstream.close();
  FileBlobReader file_blob_reader(temp_snapshot);
  LOG(INFO) << "Writing snapshot file: " << params_.data_dir << "/"
            << params_.snapshot_file;
  if (auto status = blob_client_->PutBlob(
          file_blob_reader,
          {.bucket = params_.data_dir, .key = params_.snapshot_file});
      !status.ok()) {
    return status;
  }
  LOG(INFO) << "Successfully wrote snapshot file: " << params_.data_dir << "/"
            << params_.snapshot_file;
  return absl::OkStatus();
}

}  // namespace kv_server
