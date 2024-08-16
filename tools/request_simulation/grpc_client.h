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

#ifndef TOOLS_REQUEST_SIMULATION_GRPC_CLIENT_H_
#define TOOLS_REQUEST_SIMULATION_GRPC_CLIENT_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "components/data_server/request_handler/get_values_v2_handler.h"
#include "grpcpp/generic/generic_stub.h"
#include "grpcpp/grpcpp.h"
#include "src/google/protobuf/message.h"

namespace kv_server {

enum class GrpcAuthenticationMode {
  // Google token-based authentication
  // More information https://grpc.io/docs/guides/auth/
  kGoogleDefaultCredential = 0,
  // ALTS authentication for application running on GCP
  // More information https://grpc.io/docs/languages/cpp/alts/
  kALTS,
  // SSL/TLS authentication
  kSsl,
  // Plaintext authentication
  kPlainText,
};

// Overloads AbslParseFlag and AbslUnparseFlag to allow GrpcAuthenticationMode
// passed as enum flag. https://abseil.io/docs/cpp/guides/flags#custom
inline bool AbslParseFlag(absl::string_view text, GrpcAuthenticationMode* mode,
                          std::string* error) {
  if (text == "google_default") {
    *mode = GrpcAuthenticationMode::kGoogleDefaultCredential;
    return true;
  }
  if (text == "alts") {
    *mode = GrpcAuthenticationMode::kALTS;
    return true;
  }
  if (text == "ssl") {
    *mode = GrpcAuthenticationMode::kSsl;
    return true;
  }
  if (text == "plaintext") {
    *mode = GrpcAuthenticationMode::kPlainText;
    return true;
  }
  *error = "unknown value for enumeration";
  return false;
}

inline std::string AbslUnparseFlag(GrpcAuthenticationMode mode) {
  switch (mode) {
    case GrpcAuthenticationMode::kGoogleDefaultCredential:
      return "google_default";
    case GrpcAuthenticationMode::kALTS:
      return "alts";
    case GrpcAuthenticationMode::kSsl:
      return "ssl";
    case GrpcAuthenticationMode::kPlainText:
      return "plaintext";
    default:
      return absl::StrCat(mode);
  }
}

// Creates a grpc channel from server address and authentication mode.
inline std::shared_ptr<grpc::Channel> CreateGrpcChannel(
    const std::string& server_address,
    const GrpcAuthenticationMode& auth_mode) {
  switch (auth_mode) {
    case GrpcAuthenticationMode::kGoogleDefaultCredential:
      return grpc::CreateChannel(server_address,
                                 grpc::GoogleDefaultCredentials());
    case GrpcAuthenticationMode::kALTS:
      return grpc::CreateChannel(
          server_address, grpc::experimental::AltsCredentials(
                              grpc::experimental::AltsCredentialsOptions()));
    case GrpcAuthenticationMode::kSsl:
      return grpc::CreateChannel(
          server_address, grpc::SslCredentials(grpc::SslCredentialsOptions()));
    default:
      return grpc::CreateChannel(server_address,
                                 grpc::InsecureChannelCredentials());
  }
}

// A generic grpc client that sends request of given type and returns
// error message in status or response of given type.
// The Request and Response types can be grpc::ByteBuffer or protobuf Message
template <typename RequestT, typename ResponseT>
class GrpcClient {
 public:
  // Parameters passed in the constructor:
  //
  // A grpc channel to create a grpc stub in the grpc client
  //
  // Timeout duration to define a period of time that a unary call passes the
  // deadline
  //
  // Whether the grpc channel is a client channel rather than a in-process
  // channel
  explicit GrpcClient(std::shared_ptr<grpc::Channel> channel,
                      absl::Duration timeout, bool is_client_channel = true)
      : grpc_channel_(channel),
        is_client_channel_(is_client_channel),
        timeout_(std::move(timeout)) {
    generic_stub_ =
        std::make_unique<grpc::TemplatedGenericStub<RequestT, ResponseT>>(
            channel);
  }
  // Sends message via grpc unary call. The request method is the
  // api name supported by the grpc service, an example method name is
  // "/PackageName.ExampleService/APIName".
  absl::Status SendMessage(std::shared_ptr<RequestT> request,
                           const std::string& request_method,
                           std::shared_ptr<ResponseT> response) {
    if (is_client_channel_ &&
        grpc_channel_->GetState(true) != GRPC_CHANNEL_READY) {
      return absl::UnavailableError("GRPC channel is disconnected");
    }
    std::shared_ptr<absl::Notification> notification =
        std::make_shared<absl::Notification>();
    std::shared_ptr<grpc::ClientContext> client_context =
        std::make_shared<grpc::ClientContext>();
    client_context->AddMetadata(std::string(kContentTypeHeader),
                                std::string(kContentEncodingJsonHeaderValue));
    std::shared_ptr<absl::Status> grpc_status =
        std::make_shared<absl::Status>();
    generic_stub_->UnaryCall(
        client_context.get(), request_method, grpc::StubOptions(),
        request.get(), response.get(),
        [notification, grpc_status](grpc::Status status) {
          grpc_status->Update(absl::Status(
              absl::StatusCode(status.error_code()), status.error_message()));
          notification->Notify();
        });
    notification->WaitForNotificationWithTimeout(timeout_);
    if (!notification->HasBeenNotified()) {
      return absl::DeadlineExceededError("Time out in gRPC unary call");
    }
    return *grpc_status;
  }

 private:
  absl::Duration timeout_;
  std::shared_ptr<grpc::Channel> grpc_channel_;
  bool is_client_channel_;
  std::unique_ptr<grpc::TemplatedGenericStub<RequestT, ResponseT>>
      generic_stub_;
};

}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_GRPC_CLIENT_H_
