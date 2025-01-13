// Copyright 2025 Google LLC
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

#include <filesystem>
#include <fstream>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/log.h"
#include "google/protobuf/util/json_util.h"
#include "public/data_loading/filename_utils.h"
#include "src/util/status_macro/status_macros.h"
#include "src/util/status_macro/status_util.h"
#include "tools/bidding_auction_data_generator/data/custom_audience_data.pb.h"
#include "tools/bidding_auction_data_generator/delta_key_value_writer.h"
#include "tools/bidding_auction_data_generator/json_to_proto_util.h"

ABSL_FLAG(std::string, kv_byos_response_input_dir, "",
          "The input directory that contains BYOS key value server response "
          "json files");

ABSL_FLAG(std::string, kv_byos_response_input_file, "",
          "The input file path that contains BYOS key value server response "
          "json files");

ABSL_FLAG(std::string, output_dir, "",
          "Output directory to write delta file to");

ABSL_FLAG(int64_t, logical_commit_time, absl::ToUnixMicros(absl::Now()),
          "The logical commit time for the delta record, default is current "
          "unix time in macro seconds");

constexpr std::string_view kUsageMessage = R"(
Usage: This tool is used to prepare delta files for Trusted KV server for
bidding and auction testing purposes. The tool reads provided BYOS KV server
responses in json files, extracts key value pairs and writes those data updates
in delta files for data ingestion into trusted KV server.

The input of the tool can either be a directory of multiple json files or a
json file path, or both. The input files must be in json format.

The output of the tool is a single delta file that contains all key value pair
updates extracted from the input files.

Build Command:

<PROJECT_ROOT>/builders/tools/bazel-debian build //tools/bidding_auction_data_generator:generate_data_from_kv_response

Example Command:

<PROJECT_ROOT>/bazel-bin/tools/bidding_auction_data_generator/generate_data_from_kv_response \
--kv_byos_response_input_dir=<PROJECT_ROOT>/tools/bidding_auction_data_generator/data_test/seller_kv_byos_response \
--output_dir=<OUTPUT_DIR>

<PROJECT_ROOT>/bazel-bin/tools/bidding_auction_data_generator/generate_data_from_kv_response \
--kv_byos_response_input_file=<PROJECT_ROOT>/tools/bidding_auction_data_generator/data_test/buyer_kv_byos_response/response.json \
--output_dir=<OUTPUT_DIR>

)";

constexpr kv_server::KeyValueMutationType kMutationType =
    kv_server::KeyValueMutationType::Update;

// Gets the output delta file path from the provided output directory and
// logical commit time
absl::StatusOr<std::string> GetOutputDeltaFilePath(
    int64_t logical_commit_time) {
  // Get delta file name based on the logical commit time
  PS_ASSIGN_OR_RETURN(auto delta_file_name,
                      kv_server::ToDeltaFileName(logical_commit_time));
  const std::string output_dir = absl::GetFlag(FLAGS_output_dir);
  if (output_dir.empty()) {
    return absl::InvalidArgumentError("Empty output directory for delta files");
  }
  return absl::StrCat(output_dir, "/", delta_file_name);
}

// Gets the intput json file paths from the provided directory and
// json file path
absl::StatusOr<absl::flat_hash_set<std::string>> GetInputJsonFilePaths() {
  // Read the input key value server GetValues Response reponse json files
  // Get the file paths from directory
  const std::string input_file_dir =
      absl::GetFlag(FLAGS_kv_byos_response_input_dir);
  LOG(INFO) << "Reading GetValues byos response json files from directory "
            << input_file_dir;
  PS_ASSIGN_OR_RETURN(auto file_paths,
                      kv_server::GetInputJsonFilePaths(input_file_dir));
  // Add the file path if the json file path is defined
  if (const std::string input_file_path =
          absl::GetFlag(FLAGS_kv_byos_response_input_file);
      !input_file_path.empty()) {
    if (kv_server::IsJsonFile(input_file_path)) {
      file_paths.emplace(input_file_path);
    } else {
      return absl::InvalidArgumentError(absl::StrCat(
          "The specified file path is not json file: ", input_file_path));
    }
  }
  if (file_paths.empty()) {
    return absl::InvalidArgumentError("No input json files are found");
  }
  return file_paths;
}

absl::StatusOr<absl::flat_hash_map<std::string, std::string>>
GetKeyValuePairsFromInputJsonFiles(
    const absl::flat_hash_set<std::string>& input_json_files) {
  auto extract_keys_callback = kv_server::GetDataExtractorForKeys();
  auto extract_render_urls_callback =
      kv_server::GetDataExtractorForRenderUrls();
  auto extract_ad_component_render_urls_callback =
      kv_server::GetDataExtractorForAdComponentRenderUrls();
  absl::flat_hash_map<std::string, std::string> key_value_map;
  for (auto const& file : input_json_files) {
    PS_ASSIGN_OR_RETURN(auto byos_response_data,
                        kv_server::ReadGetValueResponseJsonFile(file));
    extract_keys_callback(byos_response_data, key_value_map);
    extract_render_urls_callback(byos_response_data, key_value_map);
    extract_ad_component_render_urls_callback(byos_response_data,
                                              key_value_map);
  }
  if (key_value_map.empty()) {
    return absl::DataLossError(
        "No output key value data is extracted from input files");
  }
  return key_value_map;
}

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage(absl::StrCat(kUsageMessage, "\n", argv[0]));
  absl::ParseCommandLine(argc, argv);

  // Parse the logical commit time
  int64_t logical_commit_time = absl::GetFlag(FLAGS_logical_commit_time);

  auto maybe_delta_file_path = GetOutputDeltaFilePath(logical_commit_time);
  if (!maybe_delta_file_path.ok()) {
    LOG(ERROR) << "Abort! Unable to get output delta file path: "
               << maybe_delta_file_path.status();
    return -1;
  }

  auto maybe_json_input_file_paths = GetInputJsonFilePaths();
  if (!maybe_json_input_file_paths.ok()) {
    LOG(ERROR) << "Abort! Error in getting input json file paths "
               << maybe_json_input_file_paths.status();
    return -1;
  }

  auto maybe_key_value_pairs =
      GetKeyValuePairsFromInputJsonFiles(*maybe_json_input_file_paths);
  if (!maybe_key_value_pairs.ok()) {
    LOG(ERROR) << "Abort! Error in extracting key value pairs from the input "
                  "json path "
               << maybe_key_value_pairs.status();
    return -1;
  }

  // write key value pairs to the delta file
  LOG(INFO) << "Writing " << maybe_key_value_pairs->size()
            << " key value pairs to delta file " << *maybe_delta_file_path;
  std::ofstream o_fstream(*maybe_delta_file_path);
  std::ostream* o_stream = &o_fstream;
  auto writer = kv_server::DeltaKeyValueWriter::Create(*o_stream);
  if (!writer.ok()) {
    LOG(ERROR) << "Unable to initialize delta writer: " << writer.status();
    return -1;
  }
  if (absl::Status status = writer.value()->Write(
          *maybe_key_value_pairs, logical_commit_time, kMutationType);
      !status.ok()) {
    LOG(ERROR) << "Error in writing delta file " << status;
    return -1;
  }
  LOG(INFO) << "Done writing delta file " << *maybe_delta_file_path;
  return 0;
}
