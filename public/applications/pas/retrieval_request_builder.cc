// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "public/applications/pas/retrieval_request_builder.h"

namespace kv_server::application_pas {

v2::GetValuesRequest GetRequest() {
  static const std::string* kClient = new std::string("Retrieval.20231018");
  v2::GetValuesRequest req;
  req.set_client_version(*kClient);
  (*(req.mutable_metadata()->mutable_fields()))["is_pas"].set_string_value(
      "true");
  return req;
}

v2::GetValuesRequest BuildRetrievalRequest(
    const privacy_sandbox::server_common::LogContext& log_context,
    const privacy_sandbox::server_common::ConsentedDebugConfiguration&
        consented_debug_config,
    std::string protected_signals,
    absl::flat_hash_map<std::string, std::string> device_metadata,
    std::string contextual_signals, std::vector<std::string> optional_ad_ids) {
  v2::GetValuesRequest req = GetRequest();
  v2::RequestPartition* partition = req.add_partitions();
  {
    auto* protected_signals_arg = partition->add_arguments();
    protected_signals_arg->mutable_data()->set_string_value(
        std::move(protected_signals));
  }
  {
    auto* device_metadata_arg = partition->add_arguments();
    for (auto&& [key, value] : std::move(device_metadata)) {
      (*device_metadata_arg->mutable_data()
            ->mutable_struct_value()
            ->mutable_fields())[std::move(key)]
          .set_string_value(value);
    }
  }
  {
    auto* contextual_signals_arg = partition->add_arguments();
    contextual_signals_arg->mutable_data()->set_string_value(
        std::move(contextual_signals));
  }
  {
    auto* ad_id_arg = partition->add_arguments();
    for (auto&& item : std::move(optional_ad_ids)) {
      ad_id_arg->mutable_data()
          ->mutable_list_value()
          ->add_values()
          ->set_string_value(std::move(item));
    }
  }
  { *req.mutable_consented_debug_config() = consented_debug_config; }
  { *req.mutable_log_context() = log_context; }
  return req;
}

v2::GetValuesRequest BuildLookupRequest(
    const privacy_sandbox::server_common::LogContext& log_context,
    const privacy_sandbox::server_common::ConsentedDebugConfiguration&
        consented_debug_config,
    std::vector<std::string> ad_ids) {
  v2::GetValuesRequest req = GetRequest();
  v2::RequestPartition* partition = req.add_partitions();
  auto* ad_id_arg = partition->add_arguments();
  for (auto&& item : std::move(ad_ids)) {
    ad_id_arg->mutable_data()
        ->mutable_list_value()
        ->add_values()
        ->set_string_value(std::move(item));
  }
  { *req.mutable_consented_debug_config() = consented_debug_config; }
  { *req.mutable_log_context() = log_context; }
  return req;
}

}  // namespace kv_server::application_pas
