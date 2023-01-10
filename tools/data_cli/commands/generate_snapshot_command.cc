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
#include "public/constants.h"
#include "public/data_loading/filename_utils.h"
#include "public/data_loading/readers/delta_record_stream_reader.h"
#include "public/data_loading/riegeli_metadata.pb.h"

namespace kv_server {
namespace {

constexpr std::string_view kStdioSymbol = "-";
constexpr std::string_view kCloudDirectoryPrefix = "cloud://";

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

class FileBlobStorageClient : public BlobStorageClient {
 public:
  ~FileBlobStorageClient() = default;
  static std::unique_ptr<BlobStorageClient> Create() {
    return std::make_unique<FileBlobStorageClient>();
  }
  std::unique_ptr<BlobReader> GetBlobReader(DataLocation location) override {
    return std::make_unique<FileBlobReader>(GetFullPath(location));
  }
  absl::Status PutBlob(BlobReader& blob_reader,
                       DataLocation location) override {
    std::filesystem::remove(GetFullPath(location));
    std::ofstream blob_ostream(GetFullPath(location));
    blob_ostream << blob_reader.Stream().rdbuf();
    blob_ostream.close();
    return absl::OkStatus();
  }
  absl::Status DeleteBlob(DataLocation location) override {
    auto fullpath = GetFullPath(location);
    if (std::filesystem::remove(fullpath)) {
      return absl::OkStatus();
    }
    return absl::InternalError(
        absl::StrCat("Failed to delete blob: ", fullpath.c_str()));
  }
  absl::StatusOr<std::vector<std::string>> ListBlobs(
      DataLocation location, ListOptions options) override {
    std::vector<std::string> blob_names;
    for (const auto& dir_entry :
         std::filesystem::directory_iterator(location.bucket)) {
      if (dir_entry.is_directory()) {
        continue;
      }
      auto blob_name = dir_entry.path().filename();
      if (!absl::StartsWith(blob_name.c_str(), options.prefix) ||
          blob_name <= options.start_after) {
        continue;
      }
      blob_names.push_back(blob_name);
    }
    std::sort(blob_names.begin(), blob_names.end());
    return blob_names;
  }

 private:
  std::filesystem::path GetFullPath(DataLocation location) {
    return std::filesystem::path(location.bucket) / location.key;
  }
};

absl::Status ValidateRequiredParams(
    const GenerateSnapshotCommand::Params& params) {
  if (params.key_namespace.empty()) {
    return absl::InvalidArgumentError("Key namespace is required.");
  }
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
  if (!absl::StartsWith(params.data_dir, kCloudDirectoryPrefix) &&
      !std::filesystem::is_directory(params.data_dir)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Data directory must be a valid local directory or an S3 "
                     "bucket. Data directory: ",
                     params.data_dir));
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

bool IsCloudDataDir(std::string_view data_dir) {
  return absl::StartsWith(data_dir, kCloudDirectoryPrefix);
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

absl::StatusOr<KeyNamespace::Enum> GetKeyNamespace(
    std::string_view key_namespace) {
  KeyNamespace::Enum key_ns;
  if (KeyNamespace::Enum_Parse(key_namespace.data(), &key_ns)) {
    return key_ns;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Key namespace: ", key_namespace, " is not valid."));
}

absl::StatusOr<KVFileMetadata> CreateSnapshotMetadata(
    const GenerateSnapshotCommand::Params& params) {
  KVFileMetadata metadata;
  auto key_ns = GetKeyNamespace(params.key_namespace);
  if (!key_ns.ok()) {
    return key_ns.status();
  }
  metadata.set_key_namespace(*key_ns);
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
    KeyNamespace::Enum key_namespace, BlobStorageClient& blob_client,
    SnapshotStreamWriter<std::ostream>& snapshot_writer) {
  LOG(INFO) << "Compacting base snapshot file: " << params.starting_file;
  auto blob_reader = blob_client.GetBlobReader(
      {.bucket = params.data_dir.data(), .key = params.starting_file.data()});
  DeltaRecordStreamReader record_reader(blob_reader->Stream());
  auto metadata = record_reader.ReadMetadata();
  if (!metadata.ok()) {
    return metadata.status();
  }
  if (!metadata->has_key_namespace() ||
      metadata->key_namespace() != key_namespace) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Base snapshot file has different key namespace than "
        "target snapshot. Base key namespace: ",
        metadata->key_namespace(), ", target key namespace: ", key_namespace));
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
    KeyNamespace::Enum key_namespace, BlobStorageClient& blob_client,
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
    if (!metadata->has_key_namespace() ||
        metadata->key_namespace() != key_namespace) {
      LOG(INFO) << "Skipping delta file: " << delta_file
                << " because of key namespace mismatch. "
                << " Delta file namespace: " << metadata->key_namespace()
                << ", Target snapshot namespace: " << key_namespace;
      continue;
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
  auto blob_client = IsCloudDataDir(params.data_dir)
                         ? BlobStorageClient::Create()
                         : FileBlobStorageClient::Create();
  params.data_dir = IsCloudDataDir(params.data_dir)
                        ? params.data_dir.substr(kCloudDirectoryPrefix.size())
                        : params.data_dir;
  return absl::WrapUnique(
      new GenerateSnapshotCommand(std::move(params), std::move(blob_client)));
}

absl::Status GenerateSnapshotCommand::Execute() {
  auto key_namespace = GetKeyNamespace(params_.key_namespace);
  if (!key_namespace.ok()) {
    return key_namespace.status();
  }
  auto snapshot_metadata = CreateSnapshotMetadata(params_);
  if (!snapshot_metadata.ok()) {
    return snapshot_metadata.status();
  }
  std::ofstream snapshot_ofstream(GetTempSnapshotFile(params_));
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
    auto snapshot_end_file = WriteBaseSnapshotData(
        params_, *key_namespace, *blob_client_, **snapshot_writer);
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
  if (auto status =
          WriteDeltaFilesToSnapshot(*delta_files, params_, *key_namespace,
                                    *blob_client_, **snapshot_writer);
      !status.ok()) {
    return status;
  }
  if (auto status = (*snapshot_writer)->Finalize(); !status.ok()) {
    return status;
  }
  snapshot_ofstream.close();
  auto blob_reader = FileBlobStorageClient::Create()->GetBlobReader(
      {.bucket = params_.working_dir, .key = params_.snapshot_file});
  LOG(INFO) << "Writing snapshot file: " << params_.data_dir << "/"
            << params_.snapshot_file;
  if (auto status = blob_client_->PutBlob(
          *blob_reader,
          {.bucket = params_.data_dir, .key = params_.snapshot_file});
      !status.ok()) {
    return status;
  }
  LOG(INFO) << "Successfully wrote snapshot file: " << params_.data_dir << "/"
            << params_.snapshot_file;
  return absl::OkStatus();
}

}  // namespace kv_server
