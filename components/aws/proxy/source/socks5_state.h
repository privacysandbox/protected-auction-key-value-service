/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMPONENTS_AWS_PROXY_SOURCE_SOCKS5_STATE
#define COMPONENTS_AWS_PROXY_SOURCE_SOCKS5_STATE

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

#include "components/aws/proxy/source/definitions.h"

// A state machine that processes the SOCKS5 handshake. Thread-safety: unsafe.
// User is required to add synchronization/locking mechanism to ensure safety.
class Socks5State : public std::enable_shared_from_this<Socks5State> {
 public:
  enum HandshakeState {
    kGreetingHeader,
    kGreetingMethods,
    kRequestHeader,
    kRequestAddrV4,
    kRequestAddrV6,
    kResponse,
    kForwarding,
    kFail
  };
  typedef uint8_t BufferUnitType;
  typedef BufferUnitType* BufferType;

  explicit Socks5State(SocketHandle client_sock)
      : state_(HandshakeState::kGreetingHeader),  // Start with client greeting
        internal_idx_(0),
        required_size_(2),  // Read byte 2 to reveal the length of the greeting.
        buffer_(nullptr),
        buffer_size_(0),
        buffer_idx_(0),
        dest_sock_(-1),
        client_sock_(client_sock),
        upstream_done_(false),
        downstream_done_(false) {}

  ~Socks5State();

  // Create a socks5 response with a dest host connection [sock].
  static std::vector<BufferUnitType> CreateResp(SocketHandle sock);

  // Set a non-owning buffer to use. This does not incur a copy, but only keeps
  // track of the buffer's pointer and size.
  void UseBuffer(BufferType buffer, size_t size);

  // Perform one state transition.
  // Return false on any error, otherwise return true.
  // sock: the sock to send potential error response to.
  bool Proceed();

  HandshakeState state() const { return state_; }

  BufferType RemainingBuffer() const { return buffer_ + buffer_idx_; }
  size_t RemainingLen() const { return buffer_size_ - buffer_idx_; }

  bool InsufficientBuffer() const;

  bool Failed() const {
    return state_ < Socks5State::kGreetingHeader || state_ > kForwarding;
  }

  SocketHandle GetDestSocket() const { return dest_sock_; }
  SocketHandle GetClientSocket() const { return client_sock_; }

  void CompleteBufferConsumption() { buffer_idx_ = buffer_size_; }

  bool DownstreamDone() const { return downstream_done_; }
  void SetDownstreamDone() { downstream_done_ = true; }
  bool UpstreamDone() const { return upstream_done_; }
  void SetUpstreamDone() { upstream_done_ = true; }

 private:
  // Helper functions for testing purposes.
  void SetState(HandshakeState state) { state_ = state; }
  void SetRequiredSize(size_t size) { required_size_ = size; }

  // Temporarily keep buffer content in [buf_internal_]. Return size of
  // [buf_internal_]
  size_t PreserveBuffer();

  // Copy out to [dst], use [buf_internal_] first as source, then [buffer]. The
  // caller should guarantee [buffer] has enough data, and [dst] has enough
  // space.
  void CopyOut(void* dst, size_t sz);

  // Create a socket and connect to addr. Return the connected socket, or -1 on
  // failure.
  static SocketHandle Connect(const struct sockaddr* addr, size_t addr_size);

  // The state of the socks5 handshake.
  HandshakeState state_;
  // In case of buffer having insufficient data, we preserve the buffer's
  // remaining content for next recv cycle.
  std::vector<char> buf_internal_;
  // The position in [buf_internal_] for next consumption.
  size_t internal_idx_;
  // Required minimum size of data to consume to complete current state.
  size_t required_size_;

  // The external buffer to consume.
  BufferType buffer_;
  // The size of the external buffer.
  size_t buffer_size_;
  // The next consumption index of the external buffer.
  size_t buffer_idx_;
  // The socket handle of the connection to dest host.
  SocketHandle dest_sock_;
  // The socket handle of the connection from client.
  SocketHandle client_sock_;
  std::atomic_bool upstream_done_;
  std::atomic_bool downstream_done_;
};

#endif  // COMPONENTS_AWS_PROXY_SOURCE_SOCKS5_STATE
