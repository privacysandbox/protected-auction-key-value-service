/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PUBLIC_TESTING_FAKE_KEY_VALUE_SERVICE_IMPL_H_
#define PUBLIC_TESTING_FAKE_KEY_VALUE_SERVICE_IMPL_H_

#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "grpcpp/grpcpp.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"

namespace kv_server {

// Class that implements the fake key value grpc service
class FakeKeyValueServiceImpl final : public v2::KeyValueService::Service {
 public:
  explicit FakeKeyValueServiceImpl(
      absl::flat_hash_map<std::string, std::string> data) {
    data_ = std::move(data);
  }
  grpc::Status GetValuesHttp(grpc::ServerContext* context,
                             const v2::GetValuesHttpRequest* request,
                             google::api::HttpBody* response) override {
    auto key = request->raw_body().data();
    auto itr = data_.find(key);
    if (itr == data_.end()) {
      return {grpc::StatusCode::NOT_FOUND, "value not found"};
    }
    response->set_data(itr->second);
    return grpc::Status::OK;
  }

 private:
  absl::flat_hash_map<std::string, std::string> data_;
};

}  // namespace kv_server

#endif  // PUBLIC_TESTING_FAKE_KEY_VALUE_SERVICE_IMPL_H_
