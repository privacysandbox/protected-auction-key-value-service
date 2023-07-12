/*
 * Copyright 2023 Google LLC
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

#include "components/data_server/request_handler/get_values_adapter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/data_server/request_handler/v2_response_data.pb.h"
#include "glog/logging.h"
#include "google/protobuf/util/json_util.h"

namespace kv_server {
namespace {
using google::protobuf::RepeatedPtrField;
using google::protobuf::Struct;
using google::protobuf::Value;
using google::protobuf::util::JsonStringToMessage;

constexpr char kKeysTag[] = "keys";
constexpr char kRenderUrlsTag[] = "renderUrls";
constexpr char kAdComponentRenderUrlsTag[] = "adComponentRenderUrls";
constexpr char kKvInternalTag[] = "kvInternal";
constexpr char kCustomTag[] = "custom";

constexpr int kUdfInputApiVersion = 1;

nlohmann::json BuildKeyGroup(const RepeatedPtrField<std::string>& keys,
                             std::string namespace_tag) {
  nlohmann::json keyGroup;
  keyGroup["tags"] =
      nlohmann::json::array({"custom", std::move(namespace_tag)});
  nlohmann::json keyList = nlohmann::json::array();
  for (auto&& key : keys) {
    keyList.push_back(key);
  }
  keyGroup["keyList"] = keyList;
  return keyGroup;
}

v2::GetValuesRequest BuildV2Request(const v1::GetValuesRequest& v1_request) {
  nlohmann::json get_values_v2;

  get_values_v2["context"]["subkey"] = v1_request.subkey();

  nlohmann::json keyGroups = nlohmann::json::array();
  if (v1_request.keys_size() > 0) {
    keyGroups.push_back(BuildKeyGroup(v1_request.keys(), kKeysTag));
  }
  if (v1_request.render_urls_size() > 0) {
    keyGroups.push_back(
        BuildKeyGroup(v1_request.render_urls(), kRenderUrlsTag));
  }
  if (v1_request.ad_component_render_urls_size() > 0) {
    keyGroups.push_back(BuildKeyGroup(v1_request.ad_component_render_urls(),
                                      kAdComponentRenderUrlsTag));
  }
  if (v1_request.kv_internal_size() > 0) {
    keyGroups.push_back(
        BuildKeyGroup(v1_request.kv_internal(), kKvInternalTag));
  }

  // We only need one partition, since a V1 request does not aggregate keys.
  nlohmann::json partition;
  partition["id"] = 0;
  partition["compressionGroup"] = 0;
  partition["keyGroups"] = keyGroups;

  get_values_v2["partitions"] = nlohmann::json::array({partition});
  get_values_v2["udfInputApiVersion"] = kUdfInputApiVersion;

  v2::GetValuesRequest v2_request;
  v2_request.mutable_raw_body()->set_data(get_values_v2.dump());
  return v2_request;
}

// Add key value pairs to the result struct
void ProcessKeyValues(KeyGroupOutput key_group_output, Struct& result_struct) {
  for (auto&& [k, v] : std::move(key_group_output.key_values())) {
    if (v.value().has_string_value()) {
      Value value_proto;
      absl::Status status =
          JsonStringToMessage(v.value().string_value(), &value_proto);
      if (status.ok()) {
        (*result_struct.mutable_fields())[std::move(k)] = value_proto;
      }
    }
    (*result_struct.mutable_fields())[std::move(k)] = v.value();
  }
}

// Find the namespace tag that is paired with the "custom" tag.
absl::StatusOr<std::string> FindNamespace(RepeatedPtrField<std::string> tags) {
  if (tags.size() != 2) {
    return absl::InvalidArgumentError(
        absl::StrCat("Expected 2 tags, found ", tags.size()));
  }

  bool has_custom_tag = false;
  std::string maybe_namespace_tag;
  for (auto&& tag : std::move(tags)) {
    if (tag == kCustomTag) {
      has_custom_tag = true;
    } else {
      maybe_namespace_tag = std::move(tag);
    }
  }

  if (has_custom_tag) {
    return maybe_namespace_tag;
  }
  return absl::InvalidArgumentError("No namespace tags found");
}

absl::Status ProcessKeyGroupOutput(KeyGroupOutput key_group_output,
                                   v1::GetValuesResponse& v1_response) {
  // Ignore if no valid namespace tag that is paired with a 'custom' tag
  auto tag_namespace_status_or =
      FindNamespace(std::move(key_group_output.tags()));
  if (!tag_namespace_status_or.ok()) {
    return tag_namespace_status_or.status();
  }
  if (tag_namespace_status_or.value() == kKeysTag) {
    ProcessKeyValues(std::move(key_group_output), *v1_response.mutable_keys());
  }
  if (tag_namespace_status_or.value() == kRenderUrlsTag) {
    ProcessKeyValues(std::move(key_group_output),
                     *v1_response.mutable_render_urls());
  }
  if (tag_namespace_status_or.value() == kAdComponentRenderUrlsTag) {
    ProcessKeyValues(std::move(key_group_output),
                     *v1_response.mutable_ad_component_render_urls());
  }
  if (tag_namespace_status_or.value() == kKvInternalTag) {
    ProcessKeyValues(std::move(key_group_output),
                     *v1_response.mutable_kv_internal());
  }
  return absl::OkStatus();
}

// Process a V2 response object. The response JSON consists of an array of
// compression groups, each of which is a group of partition outputs.
absl::Status BuildV1Response(const nlohmann::json& v2_response_json,
                             v1::GetValuesResponse& v1_response) {
  if (v2_response_json.is_null()) {
    return absl::InternalError("v2 GetValues response is null");
  }
  if (!v2_response_json.is_array()) {
    return absl::InvalidArgumentError(
        "Response should be an array of compression groups.");
  }

  for (const auto& compression_group_json : v2_response_json) {
    V2CompressionGroup compression_group_proto;
    auto status = JsonStringToMessage(compression_group_json.dump(),
                                      &compression_group_proto);
    if (!status.ok()) {
      return absl::InternalError(
          absl::StrCat("Could not convert compression group json to proto: ",
                       status.message()));
    }
    for (auto&& partition_proto : compression_group_proto.partitions()) {
      for (auto&& key_group_output_proto :
           partition_proto.key_group_outputs()) {
        const auto key_group_output_status = ProcessKeyGroupOutput(
            std::move(key_group_output_proto), v1_response);
        if (!key_group_output_status.ok()) {
          // Skip and log failed key group outputs
          LOG(ERROR) << "Error processing key group output: "
                     << key_group_output_status;
        }
      }
    }
  }
  return absl::OkStatus();
}

}  // namespace

class GetValuesAdapterImpl : public GetValuesAdapter {
 public:
  explicit GetValuesAdapterImpl(std::unique_ptr<GetValuesV2Handler> v2_handler)
      : v2_handler_(std::move(v2_handler)) {}

  grpc::Status CallV2Handler(const v1::GetValuesRequest& v1_request,
                             v1::GetValuesResponse& v1_response) const {
    v2::GetValuesRequest v2_request = BuildV2Request(v1_request);
    auto maybe_v2_response_json =
        v2_handler_->GetValuesJsonResponse(v2_request);
    if (!maybe_v2_response_json.ok()) {
      return grpc::Status(
          grpc::StatusCode::INTERNAL,
          std::string(maybe_v2_response_json.status().message()));
    }

    auto build_response_status =
        BuildV1Response(maybe_v2_response_json.value(), v1_response);
    if (!build_response_status.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(build_response_status.message()));
    }
    return grpc::Status::OK;
  }

 private:
  std::unique_ptr<GetValuesV2Handler> v2_handler_;
};

std::unique_ptr<GetValuesAdapter> GetValuesAdapter::Create(
    std::unique_ptr<GetValuesV2Handler> v2_handler) {
  return std::make_unique<GetValuesAdapterImpl>(std::move(v2_handler));
}

}  // namespace kv_server
