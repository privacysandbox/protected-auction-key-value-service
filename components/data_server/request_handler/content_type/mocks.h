/*
 * Copyright 2025 Google LLC
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

#ifndef COMPONENTS_DATA_SERVER_REQUEST_HANDLER_CONTENT_TYPE_MOCKS_H_
#define COMPONENTS_DATA_SERVER_REQUEST_HANDLER_CONTENT_TYPE_MOCKS_H_

#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"

#include "encoder.h"

namespace kv_server {

class MockV2EncoderDecoder : public V2EncoderDecoder {
 public:
  MOCK_METHOD(absl::StatusOr<std::string>, EncodeV2GetValuesResponse,
              (v2::GetValuesResponse&), (const, override));

  MOCK_METHOD(absl::StatusOr<std::string>, EncodePartitionOutputs,
              ((std::vector<std::pair<int32_t, std::string>>&),
               const RequestContextFactory&),
              (const, override));

  MOCK_METHOD(absl::StatusOr<v2::GetValuesRequest>,
              DecodeToV2GetValuesRequestProto, (std::string_view),
              (const, override));
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_REQUEST_HANDLER_CONTENT_TYPE_MOCKS_H_
