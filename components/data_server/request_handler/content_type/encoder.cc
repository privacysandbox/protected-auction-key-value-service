// Copyright 2024 Google LLC
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

#include "components/data_server/request_handler/content_type/encoder.h"

#include <map>

#include "components/data_server/request_handler/content_type/cbor_encoder.h"
#include "components/data_server/request_handler/content_type/json_encoder.h"
#include "components/data_server/request_handler/content_type/proto_encoder.h"
#include "public/constants.h"

namespace kv_server {

std::unique_ptr<V2EncoderDecoder> V2EncoderDecoder::Create(
    const V2EncoderDecoder::ContentType& content_type) {
  switch (content_type) {
    case V2EncoderDecoder::ContentType::kCbor: {
      return std::make_unique<CborV2EncoderDecoder>();
    }
    case V2EncoderDecoder::ContentType::kJson: {
      return std::make_unique<JsonV2EncoderDecoder>();
    }
    case V2EncoderDecoder::ContentType::kProto: {
      return std::make_unique<ProtoV2EncoderDecoder>();
    }
  }
}

V2EncoderDecoder::ContentType V2EncoderDecoder::GetContentType(
    const std::multimap<grpc::string_ref, grpc::string_ref>& headers,
    V2EncoderDecoder::ContentType default_content_type) {
  for (const auto& [header_name, header_value] : headers) {
    if (absl::AsciiStrToLower(std::string_view(
            header_name.data(), header_name.size())) == kKVContentTypeHeader) {
      if (absl::AsciiStrToLower(
              std::string_view(header_value.data(), header_value.size())) ==
          kContentEncodingProtoHeaderValue) {
        return ContentType::kProto;
      } else if (absl::AsciiStrToLower(std::string_view(header_value.data(),
                                                        header_value.size())) ==
                 kContentEncodingJsonHeaderValue) {
        return ContentType::kJson;
      } else if (absl::AsciiStrToLower(std::string_view(header_value.data(),
                                                        header_value.size())) ==
                 kContentEncodingCborHeaderValue) {
        return ContentType::kCbor;
      }
    }
  }
  return default_content_type;
}

}  // namespace kv_server
