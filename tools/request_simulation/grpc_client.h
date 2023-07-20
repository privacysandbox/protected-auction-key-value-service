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
#include "grpcpp/generic/generic_stub.h"
#include "grpcpp/grpcpp.h"
#include "src/google/protobuf/message.h"

namespace kv_server {

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
  explicit GrpcClient(std::shared_ptr<grpc::Channel> channel,
                      absl::Duration timeout)
      : timeout_(std::move(timeout)) {
    generic_stub_ =
        std::make_unique<grpc::TemplatedGenericStub<RequestT, ResponseT>>(
            channel);
  }
  // Sends message via grpc unary call. The request method is the
  // api name supported by the grpc service, an example method name is
  // "/PackageName.ExampleService/APIName".
  absl::StatusOr<ResponseT> SendMessage(const RequestT& request,
                                        const std::string& request_method) {
    absl::Notification notification;
    ResponseT response;
    grpc::ClientContext client_context;
    grpc::Status grpc_status;
    generic_stub_->UnaryCall(
        &client_context, request_method, grpc::StubOptions(), &request,
        &response, [&notification, &grpc_status](grpc::Status status) {
          grpc_status = std::move(status);
          notification.Notify();
        });
    if (!notification.WaitForNotificationWithTimeout(timeout_)) {
      return absl::DeadlineExceededError("Time out in gRPC unary call");
    }
    if (!grpc_status.ok()) {
      return absl::Status(absl::StatusCode(grpc_status.error_code()),
                          grpc_status.error_message());
    }
    return response;
  }

 private:
  absl::Duration timeout_;
  std::unique_ptr<grpc::TemplatedGenericStub<RequestT, ResponseT>>
      generic_stub_;
};

}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_GRPC_CLIENT_H_
