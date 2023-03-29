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

#include <fstream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "glog/logging.h"
#include "public/data_loading/filename_utils.h"

#include "custom_audience_data_parser.h"
#include "delta_key_value_writer.h"
#include "http_value_retriever.h"

ABSL_FLAG(std::string, buyer_kv_base_url, "",
          "The buyer key value server base url to query against");

ABSL_FLAG(std::string, seller_kv_base_url, "",
          "The seller key value server to query against");

ABSL_FLAG(std::string, custom_audience_input_file, "",
          "The input custom audience json file");

ABSL_FLAG(std::string, buyer_output_file_dir, "",
          "Output directory to write buyer delta file to");

ABSL_FLAG(std::string, seller_output_file_dir, "",
          "Output directory to write seller delta file to");

ABSL_FLAG(int64_t, logical_commit_time, absl::ToUnixMicros(absl::Now()),
          "The logical commit time for the delta record, default is current "
          "unix time in macro seconds");

ABSL_FLAG(int, num_keys_per_batch, 50,
          "The number of keys in one batch of http request");

constexpr kv_server::DeltaMutationType kMutationType =
    kv_server::DeltaMutationType::Update;

using kv_server::HttpValueRetriever;
using Output = std::vector<absl::flat_hash_map<std::string, std::string>>;
using SideLoadData =
    kv_server::tools::bidding_auction_data_generator::SideLoadData;

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage(absl::StrCat("Usage of the tool:\n", argv[0]));
  absl::ParseCommandLine(argc, argv);
  const std::string buyer_kv_base_url = absl::GetFlag(FLAGS_buyer_kv_base_url);
  const std::string seller_kv_base_url =
      absl::GetFlag(FLAGS_seller_kv_base_url);

  // Parse the logical commit time
  int64_t logical_commit_time = absl::GetFlag(FLAGS_logical_commit_time);

  // Get delta file name based on the logical commit time
  const absl::StatusOr<std::string> delta_file_name =
      kv_server::ToDeltaFileName(logical_commit_time);
  if (!delta_file_name.ok()) {
    LOG(ERROR) << "Abort! Unable to generate valid delta file name from "
                  "logical commit time "
               << logical_commit_time << ", " << delta_file_name.status();
  }

  // read the json file
  LOG(INFO) << "Reading custom audience json file";
  const std::string& custom_audience_json_path =
      absl::GetFlag(FLAGS_custom_audience_input_file);
  // todo: process the file as file stream
  const absl::StatusOr<SideLoadData>& custom_audience_data =
      kv_server::ReadCustomAudienceData(custom_audience_json_path);
  if (!custom_audience_data.ok()) {
    LOG(ERROR) << "Abort! Unable to read custom audience input json file: "
               << custom_audience_data.status() << ", "
               << custom_audience_json_path;
    return -1;
  }

  // extract data from json file
  absl::flat_hash_set<std::string> custom_audience_names;
  absl::flat_hash_set<std::string> render_urls;
  kv_server::ParseAudienceData(custom_audience_data.value(),
                               custom_audience_names, render_urls);

  kv_server::HttpUrlFetchClient http_url_fetch_client;
  std::unique_ptr<HttpValueRetriever> http_value_retriever =
      HttpValueRetriever::Create(http_url_fetch_client);

  const int num_keys_per_batch = absl::GetFlag(FLAGS_num_keys_per_batch);

  // retrieve buyer values from buyer keys
  LOG(INFO) << "Retrieving values for buyer keys of size "
            << custom_audience_names.size();
  const absl::StatusOr<Output>& buyer_output =
      http_value_retriever->RetrieveValues(
          custom_audience_names, buyer_kv_base_url, "keys",
          kv_server::GetDataExtractorForKeys(), false, num_keys_per_batch);
  if (!buyer_output.ok()) {
    LOG(ERROR) << "Unable to retrieve values for buyer keys. "
               << buyer_output.status();

  } else {
    // write buyer key value to the delta file
    const std::string buyer_delta_file_path =
        absl::StrCat(absl::GetFlag(FLAGS_buyer_output_file_dir), "/",
                     delta_file_name.value());

    std::ofstream o_fstream(buyer_delta_file_path);
    std::ostream* o_stream = &o_fstream;
    auto buyer_writer = kv_server::DeltaKeyValueWriter::Create(*o_stream);
    if (!buyer_writer.ok()) {
      LOG(ERROR) << "Unable to initialize delta writer for buyer. "
                 << buyer_writer.status();
    } else {
      for (const auto& key_value_map : buyer_output.value()) {
        if (absl::Status status = buyer_writer.value()->Write(
                key_value_map, logical_commit_time, kMutationType);
            !status.ok()) {
          LOG(ERROR) << "Error in writing buyer delta file " << status;
        }
      }
      LOG(INFO) << "Done writing buyer delta file with "
                << buyer_output.value().size() << " key value maps";
    }
  }
  LOG(INFO) << "Retrieving values for seller keys of size "
            << render_urls.size();
  const absl::StatusOr<Output>& seller_output =
      http_value_retriever->RetrieveValues(
          render_urls, seller_kv_base_url, "renderUrls",
          kv_server::GetDataExtractorForRenderUrls(), true, num_keys_per_batch);
  if (!seller_output.ok()) {
    LOG(ERROR) << "Unable to retrieve values for seller keys. "
               << seller_output.status();
  } else {
    // write seller key value to the delta file
    const std::string seller_delta_file_path =
        absl::StrCat(absl::GetFlag(FLAGS_seller_output_file_dir), "/",
                     delta_file_name.value());
    std::ofstream o_fstream(seller_delta_file_path);
    std::ostream* o_stream = &o_fstream;
    auto seller_writer = kv_server::DeltaKeyValueWriter::Create(*o_stream);
    if (!seller_writer.ok()) {
      LOG(ERROR) << "Unable to initialize delta writer for seller. "
                 << seller_writer.status();
    } else {
      for (const auto& key_value_map : seller_output.value()) {
        if (absl::Status status = seller_writer.value()->Write(
                key_value_map, logical_commit_time, kMutationType);
            !status.ok()) {
          LOG(ERROR) << "Error in writing seller delta file " << status;
        }
      }
      LOG(INFO) << "Done writing seller delta file with "
                << seller_output.value().size() << " key value maps";
    }
  }
  return 0;
}
