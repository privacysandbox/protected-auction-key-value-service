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
#include "absl/log/log.h"
#include "components/data/common/thread_manager.h"
#include "grpcpp/grpcpp.h"
#include "tools/request_simulation/grpc_client.h"
#include "tools/request_simulation/message_queue.h"
#include "tools/request_simulation/metrics_collector.h"
#include "tools/request_simulation/rate_limiter.h"

namespace kv_server {

template <typename RequestT, typename ResponseT>
class ClientWorker {
 public:
  // Parameters passed in the constructor:
  //
  // Worker id to make the worker identifiable for logging/debugging purpose
  //
  // Grpc channel connect to the service under test
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
  ClientWorker(int id, std::shared_ptr<grpc::Channel> channel,
               std::string_view service_method, absl::Duration request_timeout,
               absl::AnyInvocable<RequestT(std::string)> request_converter,
               MessageQueue& message_queue, RateLimiter& rate_limiter,
               MetricsCollector& metrics_collector,
               bool is_client_channel = true)
      : service_method_(service_method),
        message_queue_(message_queue),
        rate_limiter_(rate_limiter),
        metrics_collector_(metrics_collector),
        request_converter_(std::move(request_converter)),
        thread_manager_(
            ThreadManager::Create(absl::StrCat("Client worker ", id))) {
    grpc_client_ = std::make_unique<GrpcClient<RequestT, ResponseT>>(
        channel, request_timeout, is_client_channel);
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
  // The actual function that sends requests.
  void SendRequests();
  std::string service_method_;
  MessageQueue& message_queue_;
  RateLimiter& rate_limiter_;
  MetricsCollector& metrics_collector_;
  // Grpc client used to send requests.
  std::unique_ptr<GrpcClient<RequestT, ResponseT>> grpc_client_;
  absl::AnyInvocable<RequestT(std::string)> request_converter_;
  // Thread manager to start or stop the request sending thread.
  std::unique_ptr<ThreadManager> thread_manager_;
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
      VLOG(8) << "About to send message, current message queue size "
              << message_queue_.Size();
      if (request_body.ok()) {
        VLOG(8) << "Sending message " << request_body.value();
        metrics_collector_.IncrementRequestSentPerInterval();
        auto start = absl::Now();
        std::shared_ptr<ResponseT> response = std::make_shared<ResponseT>();
        std::shared_ptr<RequestT> request = std::make_shared<RequestT>(
            request_converter_(request_body.value()));
        auto status =
            grpc_client_->SendMessage(request, service_method_, response);
        metrics_collector_.IncrementServerResponseStatusEvent(status);
        if (!status.ok()) {
          VLOG(8) << "Received error in response " << status;
          metrics_collector_.IncrementRequestsWithErrorResponsePerInterval();
        } else {
          metrics_collector_.IncrementRequestsWithOkResponsePerInterval();
          metrics_collector_.AddLatencyToHistogram(absl::Now() - start);
          VLOG(9) << "Received ok response";
        }
      }
    } else {
      VLOG(8) << "Acquire timeout";
    }
  }
}

}  // namespace kv_server

#endif  // TOOLS_REQUEST_SIMULATION_CLIENT_WORKER_H_
