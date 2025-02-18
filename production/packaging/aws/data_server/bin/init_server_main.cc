// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <sys/wait.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"

ABSL_FLAG(bool, with_proxify, false,
          "if running inside AWS Nitro enclave, set this");

namespace {

// Returns value for AWS_DEFAULT_REGION environmental variable.
absl::StatusOr<std::string> GetRegion() {
  constexpr std::string_view kRegionOutputFile = "/tmp/region";
  if (const pid_t pid = fork(); pid == 0) {
    // Run `/get_region` binary in proxify.
    const std::string output_file_flag =
        absl::StrCat("--output_file=", kRegionOutputFile);
    execl("/proxify", "/proxify", "--", "/get_region", output_file_flag.c_str(),
          nullptr);
    exit(errno);
  } else if (pid == -1) {
    return absl::ErrnoToStatus(errno, "fork failure");
  } else if (int status; waitpid(pid, &status, /*options=*/0) == -1) {
    return absl::ErrnoToStatus(errno, "wait failure");
  } else if (!WIFEXITED(status)) {
    return absl::InternalError("Execution of `/get_region` exited abnormally.");
  } else if (const int child_errno = WEXITSTATUS(status);
             child_errno != EXIT_SUCCESS) {
    return absl::ErrnoToStatus(child_errno, "`/get_region` failure");
  }

  // Return contents of `/get_region` output file.
  std::ifstream ifs(kRegionOutputFile.data());
  std::string content((std::istreambuf_iterator<char>(ifs)),
                      (std::istreambuf_iterator<char>()));
  return content;
}

}  // namespace

int main(int argc, char* argv[]) {
  std::vector<const char*> exec_args = {"/proxify", "--", "/server"};

  // Forward extra flags to `/server`.
  const std::vector<char*> positional_args = absl::ParseCommandLine(argc, argv);
  exec_args.insert(exec_args.end(), positional_args.begin(),
                   positional_args.end());
  exec_args.push_back(nullptr);
  if (absl::GetFlag(FLAGS_with_proxify)) {
    if (const absl::StatusOr<std::string> region = GetRegion(); !region.ok()) {
      std::cerr << region.status() << std::endl;
      return EXIT_FAILURE;
    } else if (setenv("AWS_DEFAULT_REGION", region->c_str(), 1)) {
      std::cerr << "setenv failure: " << std::strerror(errno) << std::endl;
      return errno;
    } else {
      std::cout << "AWS_DEFAULT_REGION " << *region << std::endl;
    }
    execv(exec_args[0], const_cast<char* const*>(&exec_args[0]));
  } else {
    execv(exec_args[2], const_cast<char* const*>(&exec_args[2]));
  }
  std::cerr << "exec failure: " << std::strerror(errno) << std::endl;
  return errno;
}
