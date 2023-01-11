# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = [
    "//components:__subpackages__",
])

cc_library(
    name = "bhttp_utils",
    hdrs = [
        "bhttp_utils.h",
    ],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_github_google_quiche//quiche:binary_http_unstable_api",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "bhttp_utils_test",
    size = "small",
    srcs = [
        "bhttp_utils_test.cc",
    ],
    deps = [
        ":bhttp_utils",
        "@com_github_google_quiche//quiche:binary_http_unstable_api",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
