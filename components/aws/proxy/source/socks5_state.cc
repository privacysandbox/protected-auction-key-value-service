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

#include "components/aws/proxy/source/socks5_state.h"

#include <netinet/ip.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>

#include "components/aws/proxy/source/protocol.h"
#include "components/aws/proxy/source/receive_worker.h"
#include "components/aws/proxy/source/send.h"

static const size_t kBufferSize = 65536;

std::vector<Socks5State::BufferUnitType> Socks5State::CreateResp(
    SocketHandle sock) {
  struct sockaddr_storage addr_storage;
  socklen_t addr_len = sizeof(addr_storage);
  // Per rfc1928 the response is in this format:
  //  +----+-----+-------+------+----------+----------+
  //  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
  //  +----+-----+-------+------+----------+----------+
  //  | 1  |  1  | X'00' |  1   | Variable |    2     |
  //  +----+-----+-------+------+----------+----------+
  // The size of a response with IPv4/IPv6 addresses are 10(v4), 22(v6),
  // respectively. So 32 should be large enough to contain them.
  static constexpr size_t kRespBufSize = 32;
  BufferUnitType resp_storage[kRespBufSize] = {0x05, 0x00, 0x00};
  size_t resp_size = 3;  // First 3 bytes are fixed as defined above.
  sockaddr* addr = reinterpret_cast<sockaddr*>(&addr_storage);
  if (sock > 0 && !getsockname(sock, addr, &addr_len)) {
    // Successful. Return response.
    size_t addr_sz = FillAddrPort(&resp_storage[resp_size], addr);
    resp_size += addr_sz;
    std::vector<BufferUnitType> resp(resp_storage, resp_storage + resp_size);
    return resp;
  }
  // Otherwise, we have an error.
  std::cerr << "Socket " << sock
            << " failed to get local address. errno=" << errno << '\n';
  // A template of error response, with REP = 0x01, and all address and port
  // bytes set to 0x00.
  static const BufferUnitType err_resp_template[] = {
      0x05, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  return std::vector<BufferUnitType>(
      err_resp_template, err_resp_template + sizeof(err_resp_template));
}

Socks5State::~Socks5State() {
  close(client_sock_);
  close(dest_sock_);
}

void Socks5State::UseBuffer(BufferType buffer, size_t size) {
  // We must have consumed the previous buffer at time of calling this.
  assert(buffer_idx_ == buffer_size_);
  buffer_ = buffer;
  buffer_size_ = size;
  buffer_idx_ = 0;
}

bool Socks5State::Proceed() {
  // TODO: Socks5State class should only handles buffers, not socket or threads.
  // The logic for socket operations and thread creation should be refactored
  // out.
  if (InsufficientBuffer()) {
    // Insufficient data for handshake to complete, copy remaining bytes.
    PreserveBuffer();
    return true;
  }
  // TODO: refactor this long switch case to improve readability.
  switch (state_) {
    case Socks5State::kGreetingHeader: {
      // Per RFC1928 https://datatracker.ietf.org/doc/html/rfc1928
      //   +----+----------+----------+
      //   |VER | NMETHODS | METHODS  |
      //   +----+----------+----------+
      //   | 1  |    1     | 1 to 255 |
      //   +----+----------+----------+
      // Since we don't support authentication, we expect to read NMETHODS ==
      // 1, and METHODS = 0x00 (no auth required).
      uint8_t greeting[2];
      CopyOut(greeting, sizeof(greeting));
      if (greeting[0] != 0x05 || greeting[1] == 0) {
        std::cerr << "Malformed client greeting: ";
        char s[32] = {0};
        snprintf(s, sizeof(s), "%#04x %#04x\n", greeting[0], greeting[1]);
        std::cerr << s;
        state_ = Socks5State::kFail;
        break;
      }
      required_size_ = greeting[1];
      state_ = Socks5State::kGreetingMethods;
      break;
    }
    case Socks5State::kGreetingMethods: {
      size_t n_methods = required_size_;
      // TODO: handle bad_alloc
      std::unique_ptr<uint8_t[]> methods(new uint8_t[n_methods]);
      CopyOut(methods.get(), n_methods);
      // We only support "no auth" here, which is represented by 0x00.
      if (memchr(methods.get(), 0x00, n_methods) == nullptr) {
        std::cerr << "Unsupported auth methods.\n";
        state_ = Socks5State::kFail;
        break;
      }
      static const uint8_t kGreetingResp[2] = {0x05, 0x00};
      if (client_sock_ > 0 &&
          send(client_sock_, kGreetingResp, sizeof(kGreetingResp), 0) !=
              sizeof(kGreetingResp)) {
        std::cerr << "Handshake failure with client.\n";
        state_ = Socks5State::kFail;
        break;
      }
      state_ = Socks5State::kRequestHeader;
      required_size_ = 4;
      break;
    }
    case Socks5State::kRequestHeader: {
      // The request is defined by RFC1928 as:
      //   +----+-----+-------+------+----------+----------+
      //   |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
      //   +----+-----+-------+------+----------+----------+
      //   | 1  |  1  | X'00' |  1   | Variable |    2     |
      //   +----+-----+-------+------+----------+----------+
      uint8_t header[4];
      CopyOut(header, sizeof(header));
      // only support CONNECT (0x01) at this time.
      // TODO: extend the CMD type support here.
      if (header[0] != 0x05 || header[1] != 0x01 || header[2] != 0x00) {
        std::cerr << "Malformed client request.\n";
        // TODO: return meaningful response to client.
        state_ = Socks5State::kFail;
        break;
      }
      uint8_t atyp = header[3];
      if (atyp == 0x01) {
        state_ = Socks5State::kRequestAddrV4;
        required_size_ = 6;  // 4-byte IPv4 address, 2-byte port
      } else if (atyp == 0x03) {
        std::cerr << "Unsupported ATYP: 0x03(fqdn)\n";
        state_ = Socks5State::kFail;
        break;
      } else if (atyp == 0x04) {
        state_ = Socks5State::kRequestAddrV6;
        required_size_ = 16 + 2;  // 16-byte IPv6 address, 2-byte port
      } else {
        std::cerr << "Malformed client request. ATYP = " << atyp << "\n";
        state_ = Socks5State::kFail;
        break;
      }
      break;
    }
    case Socks5State::kRequestAddrV4: {
      struct sockaddr_in addr;
      memset(&addr, 0, sizeof(addr));
      addr.sin_family = AF_INET;
      // The addr and port are already in network byte order, so just copy
      CopyOut(&addr.sin_addr, sizeof(addr.sin_addr));
      CopyOut(&addr.sin_port, sizeof(addr.sin_port));
      dest_sock_ = Connect(reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
      if (dest_sock_ < 0) {
        required_size_ = 0;
        state_ = Socks5State::kFail;
        break;
      }
      // No more data to read, send response and we are done.
      required_size_ = 0;
      state_ = Socks5State::kResponse;
      break;
    }
    case Socks5State::kRequestAddrV6: {
      // Similar to kRequestAddrV4, except that it is a v6 address
      struct sockaddr_in6 addr;
      memset(&addr, 0, sizeof(addr));
      addr.sin6_family = AF_INET6;
      // The addr and port are already in network byte order, so just copy
      CopyOut(&addr.sin6_addr, sizeof(addr.sin6_addr));
      CopyOut(&addr.sin6_port, sizeof(addr.sin6_port));
      dest_sock_ = Connect(reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
      if (dest_sock_ < 0) {
        required_size_ = 0;
        state_ = Socks5State::kFail;
        break;
      }
      // No more data to read, send response and we are done.
      required_size_ = 0;
      state_ = Socks5State::kResponse;
      break;
    }
    case Socks5State::kResponse: {
      auto resp = Socks5State::CreateResp(dest_sock_);
      // TODO: consolidate the types and remove reinterpret_cast
      bool send_success = ::Send(
          client_sock_, reinterpret_cast<char*>(resp.data()), resp.size());
      if (!send_success || dest_sock_ < 0) {
        state_ = Socks5State::kFail;
        break;
      } else {
        std::thread t(DestToClientForwarder, shared_from_this(), kBufferSize);
        t.detach();
        state_ = Socks5State::kForwarding;
        buf_internal_.clear();
        internal_idx_ = 0;
        required_size_ = 1;
      }
      break;
    }
    case Socks5State::kForwarding: {
      if (!::Send(dest_sock_, RemainingBuffer(), RemainingLen())) {
        state_ = Socks5State::kFail;
        break;
      }
      // Mark buffer consumption is done.
      CompleteBufferConsumption();
      break;
    }
    case Socks5State::kFail: {
      return false;
    }
    default: {
      return false;
    }
  }  // switch
  return state_ != Socks5State::kFail;
}

bool Socks5State::InsufficientBuffer() const {
  size_t internal_remaining = buf_internal_.size() - internal_idx_;
  size_t external_remaining = buffer_size_ - buffer_idx_;
  return internal_remaining + external_remaining < required_size_;
}

size_t Socks5State::PreserveBuffer() {
  size_t remaining_size = buffer_size_ - buffer_idx_;
  buf_internal_.reserve(buf_internal_.size() + remaining_size);
  BufferType buf = buffer_ + buffer_idx_;
  std::copy(buf, buf + remaining_size, std::back_inserter(buf_internal_));
  buffer_idx_ = buffer_size_;
  return buf_internal_.size();
}

void Socks5State::CopyOut(void* dst, size_t sz) {
  char* ptr = static_cast<char*>(dst);
  if (!buf_internal_.empty()) {
    size_t buf_remain_size = buf_internal_.size() - internal_idx_;
    size_t to_copy = buf_remain_size >= sz ? sz : buf_remain_size;
    memcpy(ptr, &buf_internal_[internal_idx_], to_copy);
    internal_idx_ += to_copy;
    sz -= to_copy;
    ptr += to_copy;
  }
  if (sz > 0) {
    memcpy(ptr, buffer_ + buffer_idx_, sz);
    buffer_idx_ += sz;
  }
  if (internal_idx_ == buf_internal_.size()) {
    buf_internal_.clear();
    internal_idx_ = 0;
  }
}

SocketHandle Socks5State::Connect(const struct sockaddr* addr,
                                  size_t addr_size) {
  SocketHandle sock = socket(addr->sa_family, SOCK_STREAM, 0);
  if (sock != -1 && connect(sock, addr, addr_size) == 0) {
    return sock;
  }
  close(sock);
  return -1;
}
