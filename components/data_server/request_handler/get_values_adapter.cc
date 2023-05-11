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

#include "glog/logging.h"

namespace kv_server {
namespace {
using google::protobuf::RepeatedPtrField;

constexpr char kKeysTag[] = "keys";
constexpr char kRenderUrlsTag[] = "renderUrls";
constexpr char kAdComponentRenderUrlsTag[] = "adComponentRenderUrls";
constexpr char kKvInternalTag[] = "kvInternal";

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

  v2::GetValuesRequest v2_request;
  v2_request.mutable_raw_body()->set_data(get_values_v2.dump());
  return v2_request;
}

absl::Status ProcessV2ResponseJson(const nlohmann::json& v2_response_json,
                                   v1::GetValuesResponse& v1_response) {
  // Process the partitions in the response
  nlohmann::json partitions;
  if (const auto iter = v2_response_json.find("partitions");
      iter == v2_response_json.end()) {
    // V2 does not require partitions, so ignore missing partitions.
    return absl::OkStatus();
  } else {
    partitions = std::move(iter.value());
  }

  // TODO(b/278764114): Implement
  return absl::OkStatus();
}

absl::Status BuildV1Response(const google::api::HttpBody& v2_response,
                             v1::GetValuesResponse& v1_response) {
  nlohmann::json v2_response_json =
      nlohmann::json::parse(v2_response.data(), nullptr,
                            /*allow_exceptions=*/false,
                            /*ignore_comments=*/true);
  if (v2_response_json.is_discarded()) {
    return absl::InvalidArgumentError(
        "Error while parsing v2 GetValues response body.");
  }

  return ProcessV2ResponseJson(v2_response_json, v1_response);
}

}  // namespace

class GetValuesAdapterImpl : public GetValuesAdapter {
 public:
  explicit GetValuesAdapterImpl(const GetValuesV2Handler& v2_handler)
      : v2_handler_(v2_handler) {}

  grpc::Status CallV2Handler(const v1::GetValuesRequest& v1_request,
                             v1::GetValuesResponse& v1_response) const {
    v2::GetValuesRequest v2_request = BuildV2Request(v1_request);
    google::api::HttpBody v2_response;
    auto v2_response_status = v2_handler_.GetValues(v2_request, &v2_response);
    if (!v2_response_status.ok()) {
      return v2_response_status;
    }

    BuildV1Response(v2_response, v1_response);
    // TODO(b/278764114): process response status
    return grpc::Status::OK;
  }

 private:
  const GetValuesV2Handler& v2_handler_;
};

std::unique_ptr<GetValuesAdapter> GetValuesAdapter::Create(
    const GetValuesV2Handler& v2_handler) {
  return std::make_unique<GetValuesAdapterImpl>(v2_handler);
}

}  // namespace kv_server
