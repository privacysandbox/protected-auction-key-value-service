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

#ifndef TOOLS_REQUEST_SIMULATION_CLIENT_WORKER_H_
#define TOOLS_REQUEST_SIMULATION_CLIENT_WORKER_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "components/data/common/thread_manager.h"
#include "glog/logging.h"
#include "grpcpp/grpcpp.h"
#include "tools/request_simulation/grpc_client.h"
#include "tools/request_simulation/message_queue.h"
#include "tools/request_simulation/rate_limiter.h"

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

// Creates a grpc channel from server address and authentication mode.
std::shared_ptr<grpc::Channel> CreateGrpcChannel(
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

template <typename RequestT, typename ResponseT>
class ClientWorker {
 public:
  // Parameters passed in the constructor:
  //
  // Worker id to make the worker identifiable for logging/debugging purpose
  //
  // Server address to send requests to
  //
  // The api name supported by the service under test, an example method name
  // for grpc service can be "/PackageName.ExampleService/APIName"
  //
  // Grpc authentication mode
  //
  // Timeout duration to define a period of time that a unary call passes the
  // deadline
  //
  // Request converter function to attach the request body to the request
  // message
  //
  // Message queue to read and send requests from
  //
  // Rate limiter to control the rate of the requests sent.
  ClientWorker(int id, const std::string& server_address,
               std::string_view service_method,
               const GrpcAuthenticationMode& auth_mode,
               absl::Duration request_timeout,
               absl::AnyInvocable<RequestT(std::string)> request_converter,
               MessageQueue& message_queue, RateLimiter& rate_limiter) {
    ClientWorker(id, CreateGrpcChannel(server_address, auth_mode),
                 service_method, request_timeout, request_converter,
                 message_queue, rate_limiter);
  }
  // Starts the thread of sending requests.
  absl::Status Start();
  // Stops the thread of sending requests.
  absl::Status Stop();
  // Checks if the thread of sending requests is running.
  bool IsRunning() const;
  ~ClientWorker() = default;
  // ClientWorker is neither copyable nor movable.
  ClientWorker(const ClientWorker&) = delete;
  ClientWorker& operator=(const ClientWorker&) = delete;

 private:
  ClientWorker(int id, std::shared_ptr<grpc::Channel> channel,
               std::string_view service_method, absl::Duration request_timeout,
               absl::AnyInvocable<RequestT(std::string)> request_converter,
               MessageQueue& message_queue, RateLimiter& rate_limiter)
      : service_method_(service_method),
        message_queue_(message_queue),
        rate_limiter_(rate_limiter),
        request_converter_(std::move(request_converter)),
        thread_manager_(
            TheadManager::Create(absl::StrCat("Client worker ", id))) {
    grpc_client_ = std::make_unique<GrpcClient<RequestT, ResponseT>>(
        channel, request_timeout);
  }
  // The actual function that sends requests.
  void SendRequests();
  std::string service_method_;
  MessageQueue& message_queue_;
  RateLimiter& rate_limiter_;
  // Grpc client used to send requests.
  std::unique_ptr<GrpcClient<RequestT, ResponseT>> grpc_client_;
  absl::AnyInvocable<RequestT(std::string)> request_converter_;
  // Thread manager to start or stop the request sending thread.
  std::unique_ptr<TheadManager> thread_manager_;
  friend class ClientWorkerTestPeer;
};

template <typename RequestT, typename ResponseT>
absl::Status ClientWorker<RequestT, ResponseT>::Start() {
  return thread_manager_->Start([this]() { SendRequests(); });
}

template <typename RequestT, typename ResponseT>
absl::Status ClientWorker<RequestT, ResponseT>::Stop() {
  return thread_manager_->Stop();
}

template <typename RequestT, typename ResponseT>
bool ClientWorker<RequestT, ResponseT>::IsRunning() const {
  return thread_manager_->IsRunning();
}

template <typename RequestT, typename ResponseT>
void ClientWorker<RequestT, ResponseT>::SendRequests() {
  while (!thread_manager_->ShouldStop()) {
    if (rate_limiter_.Acquire().ok()) {
      const auto request_body = message_queue_.Pop();
      if (request_body.ok()) {
        // TODO(b/292268143) collect metrics
        auto response = grpc_client_->SendMessage(
            request_converter_(request_body.value()), service_method_);
        if (!response.ok()) {
          LOG(ERROR) << "Received error in response " << response.status();
        }
      }
    }
  }
}

}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_CLIENT_WORKER_H_