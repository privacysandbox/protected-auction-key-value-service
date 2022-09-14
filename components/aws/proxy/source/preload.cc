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

#include <arpa/nameser.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <resolv.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>
// clang-format off
#include <linux/vm_sockets.h>
// clang-format on
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "components/aws/proxy/source/protocol.h"

// Define possible interfaces with C linkage so that all the signatures and
// interfaces are consistent with libc.
extern "C" {
// This is called when the whole preload library is loaded to the app program.
static void preload_init(void) __attribute__((constructor));

// Pointer to libc functions
static int (*libc_connect)(int sockfd, const struct sockaddr* addr,
                           socklen_t addrlen);
static int (*libc_close)(int fd);
static int (*libc_res_ninit)(res_state statep);
static int (*libc_res_init)(void);
static int (*libc_setsockopt)(int sockfd, int level, int optname,
                              const void* optval, socklen_t optlen);
static int (*libc_getsockopt)(int sockfd, int level, int optname,
                              void* __restrict optval,
                              socklen_t* __restrict optlen);
// clang-format off
static int (*libc_ioctl)(int fd, unsigned long request, ...);  // NOLINT(*)
// clang-format on
// The ioctl() syscall signature contains variadic arguments for historical
// reasons (i.e. allowing different types without forced casting). However, a
// real syscall cannot have variadic arguments at all. The real internal
// signature is really just an argument with char* or void* type.
// ref: https://static.lwn.net/images/pdf/LDD3/ch06.pdf
int ioctl(int fd, unsigned long request, void* argp);  // NOLINT(runtime/int)
}

static const char kParentCidEnv[] = "PROXY_PARENT_CID";
static const char kParentPortEnv[] = "PROXY_PARENT_PORT";
static const unsigned int kDefaultParentCid = 3;
static const unsigned int kDefaultParentPort = 8888;

static int socks5_client_connect(int sockfd, const struct sockaddr* addr);

namespace {
class AutoCloseFd {
 public:
  explicit AutoCloseFd(int fd) : fd_(fd) {}
  ~AutoCloseFd() {
    if (libc_close) {
      libc_close(fd_);
    } else {
      close(fd_);
    }
  }
  int get() const { return fd_; }

 private:
  int fd_;
};
}  // namespace

void preload_init(void) {
// Some functions may be actually a macro. Here these two macros will stringize
// the argument, so that we can use STR(func_name) to refer the real symbol name
// as string.
#define _STR(s) #s
#define STR(s) _STR(s)
  libc_connect =
      reinterpret_cast<decltype(libc_connect)>(dlsym(RTLD_NEXT, STR(connect)));
  libc_close =
      reinterpret_cast<decltype(libc_close)>(dlsym(RTLD_NEXT, STR(close)));
  libc_res_init = reinterpret_cast<decltype(libc_res_init)>(
      dlsym(RTLD_NEXT, STR(res_init)));
  libc_res_ninit = reinterpret_cast<decltype(libc_res_ninit)>(
      dlsym(RTLD_NEXT, STR(res_ninit)));
  libc_setsockopt = reinterpret_cast<decltype(libc_setsockopt)>(
      dlsym(RTLD_NEXT, STR(setsockopt)));
  libc_getsockopt = reinterpret_cast<decltype(libc_getsockopt)>(
      dlsym(RTLD_NEXT, STR(getsockopt)));
  libc_ioctl =
      reinterpret_cast<decltype(libc_ioctl)>(dlsym(RTLD_NEXT, STR(ioctl)));
#undef _STR
#undef STR
}

#define EXPORT __attribute__((visibility("default")))

EXPORT int res_init(void) {
  if (libc_res_init == nullptr) {
    return -1;
  }
  int r = libc_res_init();
  // Force DNS lookups to use TCP, as we don't support UDP-based socks5 proxying
  // yet.
  _res.options |= RES_USEVC;
  return r;
}

EXPORT int res_ninit(res_state statep) {
  int r = libc_res_ninit(statep);
  statep->options |= RES_USEVC;
  return r;
}

// Get value from environment and convert to unsigned int. val is overwritten if
// everything succeeds, otherwise untouched.
static void EnvGetUint(const char* env_name, unsigned int& val) {
  if (env_name == nullptr) {
    return;
  }
  char* val_str = getenv(env_name);
  if (val_str == nullptr) {
    return;
  }
  errno = 0;
  char* end;
  unsigned int v = strtoul(val_str, &end, 10);
  if (errno == 0 && v < UINT_MAX) {
    val = static_cast<unsigned int>(v);
  }
}

// Construct a sockaddr of the parent instance by looking and env variables, or
// default if env not set.
static sockaddr_vm get_vsock_addr() {
  unsigned int cid = kDefaultParentCid;
  unsigned int port = kDefaultParentPort;
  EnvGetUint(kParentCidEnv, cid);
  EnvGetUint(kParentPortEnv, port);

  sockaddr_vm addr;
  memset(&addr, 0, sizeof(addr));
  addr.svm_family = AF_VSOCK;
  addr.svm_port = port;
  addr.svm_cid = cid;
  return addr;
}

EXPORT int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
  // First of all, we only care about TCP sockets over IPv4 or IPv6. That is,
  // SOCK_STREAM type over AF_INET or AF_INET6. If any condition doesn't match
  // or even the getsockopt call fails, we fallback to libc_connect() so that
  // any error is handled by the prestine libc_connect().
  int sock_type = 0;
  socklen_t sock_type_len = sizeof(sock_type);
  int ret = libc_getsockopt(sockfd, SOL_SOCKET, SO_TYPE,
                            static_cast<void*>(&sock_type), &sock_type_len);
  int sock_domain = 0;
  socklen_t sock_domain_len = sizeof(sock_domain);
  // We need two libc_getsockopt() calls, one for SO_TYPE, one for SO_DOMAIN. If
  // either fails, fallback to libc_connect.
  if (ret != 0 || libc_getsockopt(sockfd, SOL_SOCKET, SO_DOMAIN,
                                  static_cast<void*>(&sock_domain),
                                  &sock_domain_len) != 0) {
    return libc_connect(sockfd, addr, addrlen);
  }
  // If the socket is not TCP over IPv4/IPv6, fallback to libc_connect.
  if (sock_type != SOCK_STREAM ||
      (sock_domain != AF_INET && sock_domain != AF_INET6)) {
    return libc_connect(sockfd, addr, addrlen);
  }
  // Here we know this is a TCP socket over IPv4 or IPv6. We should convert this
  // into a vsock socket.
  int vsock_fd = socket(AF_VSOCK, SOCK_STREAM, 0);
  if (vsock_fd < 0) {
    return -1;
  }
  // Auto close vsock_fd after this scope. We'll either fail some operations or
  // successfully make a dup into sockfd. In both cases, it will need to be
  // closed.
  AutoCloseFd autoclose(vsock_fd);
  // Preserve the file modes (esp. blocking/non-blocking) so that we can apply
  // the same modes later.
  int fl = fcntl(sockfd, F_GETFL);
  // "Atomically" close sockfd and duplicate the vsock_fd into sockfd. So that
  // the application can use the same fd value. This essentially changes the
  // sockfd family from AF_INET(6) to AF_VSOCK
  if (dup2(vsock_fd, sockfd) < 0) {
    return -1;
  }
  sockaddr_vm vsock_addr = get_vsock_addr();
  if (libc_connect(sockfd, reinterpret_cast<sockaddr*>(&vsock_addr),
                   sizeof(vsock_addr)) < 0) {
    fcntl(sockfd, F_SETFL, fl);
    return -1;
  }
  // Here this is a blocking call. This potentially hurts performance on many,
  // frequent, short non-blocking connections. However, without a blocking call
  // here we'd have to hijack select/poll/epoll all together as well, which is
  // far more complicated. They may be added later if needed.
  ret = socks5_client_connect(sockfd, addr);
  // Apply file modes again.
  fcntl(sockfd, F_SETFL, fl);
  return ret;
}

EXPORT int setsockopt(int sockfd, int level, int optname, const void* optval,
                      socklen_t optlen) {
  // Application may still have the illusion that the socket is a TCP socket and
  // wants to set some TCP-level opts, e.g. TCP_NODELAY. Here we simply return 0
  // (success) in these scenarios, to avoid unnecessary failures.
  int sock_domain = 0;
  socklen_t sock_domain_len = sizeof(sock_domain);
  if ((level == SOL_TCP || level == IPPROTO_TCP) &&
      !libc_getsockopt(sockfd, SOL_SOCKET, SO_DOMAIN,
                       static_cast<void*>(&sock_domain), &sock_domain_len) &&
      sock_domain == AF_VSOCK) {
    return 0;
  }
  return libc_setsockopt(sockfd, level, optname, optval, optlen);
}

EXPORT int getsockopt(int sockfd, int level, int optname,
                      void* __restrict optval, socklen_t* __restrict optlen) {
  int sock_domain = 0;
  socklen_t sock_domain_len = sizeof(sock_domain);
  if ((level == SOL_TCP || level == IPPROTO_TCP) &&
      !libc_getsockopt(sockfd, SOL_SOCKET, SO_DOMAIN,
                       static_cast<void*>(&sock_domain), &sock_domain_len) &&
      sock_domain == AF_VSOCK) {
    return 0;
  }
  return libc_getsockopt(sockfd, level, optname, optval, optlen);
}

// A wrapper for resuming recv() call on EINTR.
// Return the total number of bytes received.
static ssize_t recv_all(int fd, void* buf, size_t len, int flags) {
  ssize_t received = 0;
  uint8_t* buffer = static_cast<uint8_t*>(buf);
  while (received < static_cast<ssize_t>(len)) {
    ssize_t r = recv(fd, buffer + received, len - received, flags);
    if (r < 0) {
      if (errno == EINTR) {
        continue;
      }
      // Otherwise, a real error happened
      return received;
    } else if (r == 0) {
      // Socket is shutdown, this is enssentially an EOF.
      return received;
    } else {
      received += r;
    }
  }
  // Here we must have len == received. Return received anyway.
  return received;
}

// In java applications, at the end of plaintext connections (e.g. HTTP), java
// may call ioctl(FIONREAD) to get to know if there is any remaining data to
// consume. FIONREAD isn't supported on VSOCK, so we just fake it. To make sure
// most ioctl calls still follow the fastest path, here we still make the ioctl
// syscall first, and only on errors we kick-in and check if that's the case we
// want to handle.
EXPORT int ioctl(int fd, unsigned long request, void* argp) {  // NOLINT
#ifndef FIONREAD
#define FIONREAD 0x541B
#endif  // FIONREAD
  int ret = libc_ioctl(fd, request, argp);
  if (ret == -1 && errno == EOPNOTSUPP && request == FIONREAD) {
    int sock_domain = 0;
    socklen_t sock_domain_len = sizeof(sock_domain);
    int r = libc_getsockopt(fd, SOL_SOCKET, SO_DOMAIN,
                            static_cast<void*>(&sock_domain), &sock_domain_len);
    if (r == 0 && sock_domain == AF_VSOCK) {
      int* intargp = static_cast<int*>(argp);
      *intargp = 0;
      return 0;
    }
  }
  return ret;
}

int socks5_client_connect(int sockfd, const struct sockaddr* addr) {
  // To simplify the IO of the handshake process, we simply stuff everything we
  // want to send to server and send all at once.
  // Ref: https://datatracker.ietf.org/doc/html/rfc1928
  // out_buffer here will contain a client greeting declaring only supporting
  // one auth method "no auth",
  //     +----+----------+----------+
  //     |VER | NMETHODS | METHODS  |
  //     +----+----------+----------+
  //     | 1  |    1     | 1 to 255 |
  //     +----+----------+----------+
  // and a connect request, leaving the address type, address and port empty for
  // filling up later:
  //     +----+-----+-------+------+----------+----------+
  //     |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
  //     +----+-----+-------+------+----------+----------+
  //     | 1  |  1  | X'00' |  1   | Variable |    2     |
  //     +----+-----+-------+------+----------+----------+
  uint8_t buffer[64] = {0x05, 0x01, 0x00, 0x05, 0x01, 0x00};
  //                     |     |     |     |     |     |
  //              VER ---      |     |     |     |     |
  //         NMETHODS ---------      |     |     |     |
  // NO AUTH REQUIRED ---------------      |     |     |
  //      request VER ---------------------      |     |
  //      request CMD ---------------------------      |
  //      request RSV ---------------------------------

  size_t out_idx = 6;
  size_t copied = FillAddrPort(&buffer[out_idx], addr);
  if (copied == 0) {
    return -1;
  }
  out_idx += copied;

  size_t out_size = out_idx;
  ssize_t ret = send(sockfd, buffer, out_size, 0);
  if (ret != static_cast<ssize_t>(out_size)) {
    return -1;
  }

  // Two messages has been sent. Receive replies now. Method selection reply:
  //     +----+--------+
  //     |VER | METHOD |
  //     +----+--------+
  //     | 1  |   1    |
  //     +----+--------+
  // Connection request reply:
  //     +----+-----+-------+------+----------+----------+
  //     |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
  //     +----+-----+-------+------+----------+----------+
  //     | 1  |  1  | X'00' |  1   | Variable |    2     |
  //     +----+-----+-------+------+----------+----------+

  static const uint8_t expected_reply[] = {0x05, 0x00, 0x05, 0x00, 0x00};
  //                                        |     |     |     |     |
  //                                VER ----      |     |     |     |
  //                             METHOD ----------      |     |     |
  //                                VER ----------------      |     |
  //                                REP ----------------------      |
  //                                RSV ----------------------------

  // Reuse buffer here. Recv 2 more bytes to reveal the ATYP byte, and
  // potentially the length byte if the bound address is a domain name (see
  // DST.ADDR definition from rfc1928).
  ssize_t to_receive = sizeof(expected_reply) + 2;
  ssize_t received = recv_all(sockfd, buffer, to_receive, 0);
  if (received != to_receive) {
    // Not enough data received. No way to proceed.
    return -1;
  }
  if (memcmp(buffer, expected_reply, sizeof(expected_reply)) != 0) {
    // Some error received. If there's a REP byte indicating errors, return the
    // REP byte inverted.
    if (buffer[3] != 0) {
      return -buffer[3];
    } else {
      return -1;
    }
  }
  uint8_t atyp = buffer[sizeof(expected_reply)];
  uint8_t extra_byte = buffer[sizeof(expected_reply) + 1];
  if (atyp == 0x01) {
    // IPv4. 4-byte addr, 2-byte port, and we've already recv'd 1 byte extra.
    to_receive = 4 + 2 - 1;
  } else if (atyp == 0x03) {
    // Domain name. The length is defined by the first byte. Plus 2 bytes port.
    to_receive = extra_byte + 2;
  } else if (atyp == 0x04) {
    // IPv6. 16-byte addr, 2-byte port, and we've already recv'd 1 byte extra.
    to_receive = 16 + 2 - 1;
  }
  uint8_t* addr_buf = nullptr;
  if (to_receive + 1 > static_cast<ssize_t>(sizeof(buffer))) {
    // to_receive is larger than sizeof(buffer), so allocate a new one. This
    // should be rare as we don't expect server to send domain names as bound
    // address. However when it does, we should be able to handle it.
    addr_buf = reinterpret_cast<uint8_t*>(malloc(to_receive + 1));
    if (addr_buf == nullptr) {
      return -1;
    }
  } else {
    addr_buf = buffer;
  }
  addr_buf[0] = extra_byte;
  // Receive remaining bytes to drain the buffer.
  received = recv_all(sockfd, &addr_buf[1], to_receive, MSG_TRUNC);
  // TODO: We may remove MSG_TRUNC and make good use of the returned address.
  if (addr_buf != buffer) {
    free(addr_buf);
  }
  if (received != to_receive) {
    // Not enough data received. No way to proceed.
    return -1;
  }
  return 0;
}
