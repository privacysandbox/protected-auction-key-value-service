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
    "//tools:__subpackages__",
])

cc_library(
    name = "cache",
    hdrs = [
        "cache.h",
    ],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "key_value_cache",
    srcs = [
        "key_value_cache.cc",
    ],
    hdrs = [
        "key_value_cache.h",
    ],
    deps = [
        ":cache",
        "//public:base_types_cc_proto",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "key_value_cache_test",
    size = "small",
    srcs = [
        "key_value_cache_test.cc",
    ],
    deps = [
        ":key_value_cache",
        ":mocks",
        "//public:base_types_cc_proto",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mocks",
    testonly = 1,
    hdrs = ["mocks.h"],
    deps = [
        ":cache",
        "@com_google_googletest//:gtest",
    ],
)
