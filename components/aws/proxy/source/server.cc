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

#include "components/aws/proxy/source/server.h"

#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
// clang-format off
#include <linux/vm_sockets.h>
// clang-format on

#include <chrono>
#include <cstring>
#include <iostream>
#include <utility>

#include "components/aws/proxy/source/receive_worker.h"
#include "components/aws/proxy/source/send.h"

bool Server::Start(uint16_t port) {
  if (port > 0) {
    port_ = port;
  }
  if (listener_status_ == ConnectionState::Connected) {
    std::cerr << "WARNING: Only one listener thread allowed per server\n";
    return false;
  }
  listener_status_ = ConnectionState::Connecting;

  // Start listener thread and return with state
  std::string type = vsock_ ? "virtual sockets" : "TCP";
  std::cout << "Starting server listening thread using " << type << " on port "
            << port_ << std::endl;
  listener_thread_ = std::thread(&Server::ListenerWorker, this);

  // Wait for listener to be active
  while (listener_status_ == ConnectionState::Connecting) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return listener_status_ == ConnectionState::Connected;
}

void Server::Stop() {
  // Stop listener
  if (listener_status_ == ConnectionState::Connected) {
    std::cout << "Shutting down listener thread...\n";
    shutdown(listener_handle_, SHUT_RDWR);
    close(listener_handle_);
    listener_handle_ = -1;
    listener_thread_.join();
    std::cout << "Listener shutdown completed.\n";
  }

  listener_status_ = ConnectionState::Disconnected;
}

bool Server::IsListening() {
  return listener_status_ == ConnectionState::Connected;
}

void Server::ListenerWorker() {
  if (vsock_) {
    listener_handle_ = socket(AF_VSOCK, SOCK_STREAM, 0);
  } else {
    listener_handle_ = socket(AF_INET, SOCK_STREAM, 0);
  }
  if (listener_handle_ == -1) {
    std::cerr << "ERROR: Cannot create server listener socket\n";
    listener_status_ = ConnectionState::Disconnected;
    return;
  }
  int reuse = 1;
  if (setsockopt(listener_handle_, SOL_SOCKET, SO_REUSEADDR,
                 (const char*)&reuse, sizeof(reuse)) < 0) {
    std::cerr << "ERROR: Cannot set reuse address flag\n";
    listener_status_ = ConnectionState::Disconnected;
    return;
  }

  // Bind listener
  struct sockaddr* listener_address;
  size_t address_size;

  if (vsock_) {
    struct sockaddr_vm* vsock_socket_address = new struct sockaddr_vm;
    memset(vsock_socket_address, 0, sizeof(struct sockaddr_vm));

    vsock_socket_address->svm_family = AF_VSOCK;
    vsock_socket_address->svm_cid = VMADDR_CID_ANY;
    vsock_socket_address->svm_port = port_;

    listener_address = (struct sockaddr*)vsock_socket_address;
    address_size = sizeof(*vsock_socket_address);
  } else {
    struct sockaddr_in* tcp_socket_address = new struct sockaddr_in;
    memset(tcp_socket_address, 0, sizeof(struct sockaddr_in));

    tcp_socket_address->sin_family = AF_INET;
    tcp_socket_address->sin_addr.s_addr = htonl(INADDR_ANY);
    tcp_socket_address->sin_port = htons(port_);

    listener_address = (struct sockaddr*)tcp_socket_address;
    address_size = sizeof(*tcp_socket_address);
  }

  if (bind(listener_handle_, listener_address, address_size) < 0) {
    std::cerr << "ERROR: Cannot bind server listener socket\n";
    close(listener_handle_);
    listener_status_ = ConnectionState::Disconnected;
    return;
  }

  delete listener_address;

  // Listen to and accept incoming connections forever
  listener_status_ = ConnectionState::Connected;
  while (true) {
    // Listen for new connections
    if (listen(listener_handle_, 5) < 0) {
      std::cerr << "ERROR: #" << errno << ", cannot listen on socket\n";
      break;
    }

    // Accept incoming connections, spawning one thread per connection
    if (vsock_) {
      AcceptVSocketConnection();
    } else {
      AcceptSocketConnection();
    }
  }
  std::cerr << "WARNING: Exiting listener thread for " << listener_handle_
            << "\n";
  listener_handle_ = -1;
  listener_status_ = ConnectionState::Disconnected;
}

void Server::SpawnReceiveWorker(SocketHandle client_handle) {
  if (client_handle > 0) {
    // Spawn new connection on worker thread
    std::thread t(Socks5Worker, client_handle, default_size_);
    t.detach();
  } else {
    // Handle errors
    std::cerr << "Bad client socket " << client_handle << "\n";
  }
}

void Server::AcceptVSocketConnection() {
  sockaddr_vm client_address;
  socklen_t client_address_size = sizeof(sockaddr_vm);
  SocketHandle client_handle = accept(
      listener_handle_, (sockaddr*)&client_address, &client_address_size);
  std::cout << "Accepted incoming virtual socket client " << client_handle
            << " on port " << port_ << std::endl;
  SpawnReceiveWorker(client_handle);
}

void Server::AcceptSocketConnection() {
  sockaddr client_address;
  socklen_t client_address_size = sizeof(sockaddr);
  int client_handle = accept(listener_handle_, (sockaddr*)&client_address,
                             &client_address_size);
  std::cout << "Accepted incoming socket client " << client_handle
            << " on port " << port_ << std::endl;
  SpawnReceiveWorker(client_handle);
}

void Server::Serve() { listener_thread_.join(); }
