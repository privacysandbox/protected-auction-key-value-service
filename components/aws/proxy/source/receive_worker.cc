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

#include "components/aws/proxy/source/receive_worker.h"

#include <netdb.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <thread>

#include "components/aws/proxy/source/send.h"
#include "components/aws/proxy/source/socks5_state.h"

// NOLINTNEXTLINE(readability/todo)
// TODO: Refactor the threading logic, use non-blocking io multiplexing
// and get rid of this file completely.

static constexpr int64_t kSocketTimoutSec = 5;

void Socks5Worker(SocketHandle client_sock, size_t buffer_size) {
  // Create a Socks5State to track all the state and sockets.
  auto state = std::make_shared<Socks5State>(client_sock);
  std::unique_ptr<Socks5State::BufferUnitType[]> buffer;
  try {
    buffer = std::make_unique<Socks5State::BufferUnitType[]>(buffer_size);
  } catch (std::bad_alloc& e) {
    std::cerr << "ERROR: Out of memory allocating client buffer\n";
    return;
  }
  timeval timeout{.tv_sec = kSocketTimoutSec, .tv_usec = 0};
  if (setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                 sizeof(timeout))) {
    std::cerr << "Client setsockopt failed, errno=" << errno << "\n";
    return;
  }
  while (true) {
    ssize_t bytes_recv = recv(client_sock, buffer.get(), buffer_size, 0);
    if (bytes_recv < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        // This is the timeout case. Check the status here. If we haven't
        // completed handshake, or the other side is gone, then we close.
        if (state->state() != Socks5State::kForwarding) {
          std::cerr << "Client connection " << client_sock
                    << " handshake timeout.\n";
          break;
        }
        if (state->DownstreamDone()) {
          std::cerr << "Closing client connection " << client_sock
                    << ", as the other side is gone.\n";
          break;
        }
        // Otherwise keep receiving.
        continue;
      }
      std::cerr << "Client connection " << client_sock << " read failed."
                << " errno=" << errno << "\n";
      break;
    }  // if (bytes_recv < 0)
    if (bytes_recv == 0) {
      std::cerr << "Client connection " << client_sock << " closed by peer.\n";
      break;
    }
    state->UseBuffer(buffer.get(), bytes_recv);
    // As long as we have enough buffer to do next work, keep calling Proceed().
    while (!state->InsufficientBuffer()) {
      if (!state->Proceed()) {
        break;
      }
    }
  }
  state->SetUpstreamDone();
}

void DestToClientForwarder(const std::shared_ptr<Socks5State>& socks5state,
                           size_t buffer_size) {
  // Make a copy so that we keep a ref count.
  auto state = socks5state;
  auto buffer = std::make_unique<char[]>(buffer_size);
  auto client_sock = state->GetClientSocket();
  auto dest_sock = state->GetDestSocket();
  timeval timeout{.tv_sec = kSocketTimoutSec, .tv_usec = 0};
  if (setsockopt(dest_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                 sizeof(timeout))) {
    std::cerr << "Client setsockopt failed, errno=" << errno << "\n";
    return;
  }
  while (true) {
    ssize_t bytes_recv = recv(dest_sock, buffer.get(), buffer_size, 0);
    if (bytes_recv < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        // This is the timeout case. Check the status here.
        if (state->UpstreamDone()) {
          std::cerr << "Closing dest connection " << dest_sock
                    << ", as the other side is gone.\n";
          break;
        } else {
          // Otherwise keep receiving.
          continue;
        }
      }
      std::cerr << "Dest Connection " << dest_sock << " ERROR: #" << errno
                << ", closing connection\n";
      break;
    }
    if (bytes_recv == 0) {
      std::cerr << "Dest Connection " << dest_sock << " closed by peer.\n";
      break;
    }
    if (send(client_sock, buffer.get(), bytes_recv, 0) != bytes_recv) {
      std::cerr << "Client connection " << client_sock << " write failed. "
                << "errno=" << errno << "\n";
      break;
    }
  }
  state->SetDownstreamDone();
}
