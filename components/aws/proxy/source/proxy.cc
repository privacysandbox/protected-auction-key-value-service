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

#include <getopt.h>
#include <signal.h>

#include <chrono>
#include <iostream>
#include <thread>

#include "components/aws/proxy/source/server.h"

constexpr char version[] = "v0.01";

struct Config {
  static constexpr uint16_t kDefaultPort = 8888;
  static constexpr size_t kDefaultBufferSize = 65536;

  Config()
      : buffer_size_(kDefaultBufferSize),
        socks5_port_(kDefaultPort),
        vsock_(true),
        bad_(false) {}
  // The buffer size to use for internal logic.
  size_t buffer_size_;
  // Port that socks5 server listens on.
  uint16_t socks5_port_;
  // True if listen on vsock. Otherwise on TCP.
  bool vsock_;
  // If the config is bad.
  bool bad_;
};

Config GetConfig(int argc, char* argv[]) {
  Config config;
  // clang-format off
  static struct option long_options[] {
    { "tcp", no_argument, 0, 't'},
    { "port", required_argument, 0, 'p'},
    { "buffer_size", required_argument, 0, 'b'},
    { 0, 0, 0, 0},
  };
  // clang-format on
  while (true) {
    int opt_idx = 0;
    int c = getopt_long(argc, argv, "tp:b:", long_options, &opt_idx);
    if (c == -1) {
      break;
    }
    switch (c) {
      case 0: {
        break;
      }
      case 't': {
        config.vsock_ = false;
        break;
      }
      case 'p': {
        char* endptr;
        std::string port_str(optarg);
        auto port = strtoul(port_str.c_str(), &endptr, 10);
        if (port == 0 || port > UINT16_MAX) {
          std::cerr << "ERROR: Invalid port number: " << port_str << "\n";
          exit(1);
        }
        config.socks5_port_ = static_cast<uint16_t>(port);
        break;
      }
      case 'b': {
        char* endptr;
        std::string bs_str(optarg);
        auto bs = strtoul(bs_str.c_str(), &endptr, 10);
        if (bs == 0) {
          std::cerr << "ERROR: Invalid buffer size: " << bs_str << "\n";
          exit(1);
        }
        config.buffer_size_ = bs;
        break;
      }
      case '?': {  // Unrecognized option. Error should be printed already.
        config.bad_ = true;
        break;
      }
      default: {
        std::cerr << "ERROR: unknown error.\n";
        abort();
      }
    }
  }
  return config;
}

// Main loop - it all starts here...
int main(int argc, char* argv[]) {
  std::cout << "Nitro Enclave Proxy " << version << " (c) Google 2021\n";

  {
    // Ignore SIGPIPE.
    struct sigaction act {};
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, nullptr);
  }

  // Process command line parameters
  auto config = GetConfig(argc, argv);
  if (config.bad_) {
    return 1;
  }

  Server server(config.socks5_port_, config.buffer_size_, config.vsock_, true);

  // NOLINTNEXTLINE(readability/todo)
  // TODO: get rid of these magic numbers and make them configurable
  int retries = 5;
  int delay = 500;
  while (!server.Start() && retries-- > 0) {
    delay *= 2;
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
  }
  if (retries < 0) {
    std::cerr << "ERROR: cannot start SOCKS5 server at port "
              << config.socks5_port_ << "\n";
    return 1;
  }

  server.Serve();

  std::cerr
      << "ERROR: A fatal error has occurred, terminating proxy instance\n";
  return 1;
}
