// Copyright 2022 Google LLC
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

#ifndef INFRASTRUCTURE_COMMUNICATION_BHTTP_UTILS_H_
#define INFRASTRUCTURE_COMMUNICATION_BHTTP_UTILS_H_

#include <string>
#include <string_view>
#include <utility>

#include "absl/status/statusor.h"
#include "glog/logging.h"
#include "google/protobuf/util/json_util.h"
#include "quiche/binary_http/binary_http_message.h"

namespace fledge {
namespace internal {

// Returns error if the message has error outside the Http body.
template <typename BHttpMessage>
absl::Status ExtractBHttpError(const BHttpMessage& message) {
  return absl::OkStatus();
}

absl::Status ExtractBHttpError(const quiche::BinaryHttpResponse& response) {
  // TODO: Pass more error information
  if (response.status_code() != 200) {
    return absl::InternalError(absl::StrCat(
        "Binary Http response contains error code: ", response.status_code()));
  }
  return absl::OkStatus();
}

}  // namespace internal

// Converts serialized Binary HTTP messages with json encoded proto body to
// proto.
//
// The binary HTTP message should contain a JSON representation of the proto in
// the message body. BHttpMessage is either BinaryHttpRequest or
// BinaryHttpResponse. In the case it is a BinaryHttpResponse, the status_code
// is checked, and only when successful (200) will the body be parsed from json
// to proto.
//
// Error will be returned if the message is a Binary HTTP response with an
// error.
template <typename BHttpMessage, typename ProtoMessage>
absl::StatusOr<ProtoMessage> DeserializeBHttpToProto(
    std::string_view serialized_bhttp_msg) {
  static_assert(std::is_base_of<google::protobuf::Message, ProtoMessage>::value,
                "DeserializeBHttpToProto only decodes to protobuf messages.");
  static_assert(std::is_base_of<quiche::BinaryHttpMessage, BHttpMessage>::value,
                "DeserializeBHttpToProto decodes from binary Http messages.");

  const absl::StatusOr<BHttpMessage> maybe_deserialized_msg =
      BHttpMessage::Create(serialized_bhttp_msg);
  if (!maybe_deserialized_msg.ok()) {
    // Deserialization error
    return maybe_deserialized_msg.status();
  }
  if (const auto s = internal::ExtractBHttpError(*maybe_deserialized_msg);
      !s.ok()) {
    LOG(WARNING) << "Received error response " << s
                 << ", body: " << maybe_deserialized_msg->body();
    return s;
  }

  ProtoMessage result;
  google::protobuf::util::JsonParseOptions options;
  options.ignore_unknown_fields = true;
  if (const auto s = google::protobuf::util::JsonStringToMessage(
          maybe_deserialized_msg->body(), &result, options);
      !s.ok()) {
    // protobuf begins to use absl::Status since
    // https://github.com/protocolbuffers/protobuf/commit/a3c8e2deb05186334b7ee8c1174f44802e38b43d
    // Once that is available we can simply return the status.
    // So we should not spend too much time dealing with the conversion now.
    return absl::InternalError(s.message().ToString());
  }

  return result;
}

// Converts a proto to a binary Http message and serializes it.
//
// The proto will be converted to JSON and stored in the message body.
// `metadata` is used to construct the message. Depending on the message type,
// the metadata type is different. For BinaryHttpRequest, it is
// BinaryHttpRequest::ControlData, for BinaryHttpResponse, it is the status
// code.
template <typename BHttpMessage, typename ProtoMessage>
absl::StatusOr<std::string> SerializeProtoToBHttp(
    const ProtoMessage& proto,
    typename std::conditional<
        std::is_same<BHttpMessage, quiche::BinaryHttpResponse>::value, uint16_t,
        quiche::BinaryHttpRequest::ControlData>::type metadata) {
  static_assert(std::is_base_of<google::protobuf::Message, ProtoMessage>::value,
                "SerializeProtoToBHttp only encodes from protobuf messages.");
  static_assert(std::is_base_of<quiche::BinaryHttpMessage, BHttpMessage>::value,
                "SerializeProtoToBHttp encodes to binary Http messages.");

  std::string body;
  google::protobuf::util::JsonOptions options;
  options.add_whitespace = false;
  if (const auto s =
          google::protobuf::util::MessageToJsonString(proto, &body, options);
      !s.ok()) {
    // protobuf begins to use absl::Status since
    // https://github.com/protocolbuffers/protobuf/commit/a3c8e2deb05186334b7ee8c1174f44802e38b43d
    // Once that is available we can simply return the status.
    // So we should not spend too much time dealing with the conversion now.
    return absl::InternalError(s.message().ToString());
  }
  BHttpMessage bhttp_msg(std::move(metadata));
  bhttp_msg.set_body(std::move(body));
  return bhttp_msg.Serialize();
}

}  // namespace fledge

#endif  // INFRASTRUCTURE_COMMUNICATION_BHTTP_UTILS_H_
