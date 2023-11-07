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

load("@rules_buf//buf:defs.bzl", "buf_lint_test")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library", "cc_test")
load("@rules_proto//proto:defs.bzl", "proto_descriptor_set", "proto_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "base_types_proto",
    srcs = ["base_types.proto"],
    deps = [
    ],
)

buf_lint_test(
    name = "base_types_proto_lint",
    size = "small",
    config = "//:buf.yaml",
    targets = [":base_types_proto"],
)

proto_descriptor_set(
    name = "base_types_descriptor_set",
    deps = [":base_types_proto"],
)

cc_proto_library(
    name = "base_types_cc_proto",
    deps = [":base_types_proto"],
)

cc_library(
    name = "constants",
    srcs = ["constants.cc"],
    hdrs = ["constants.h"],
    deps = [
        "//public:base_types_cc_proto",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "constants_test",
    size = "small",
    srcs = [
        "constants_test.cc",
    ],
    deps = [
        ":constants",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "api_schema_proto",
    srcs = ["api_schema.proto"],
    deps = [
        "@com_google_protobuf//:struct_proto",
    ],
)

cc_proto_library(
    name = "api_schema_cc_proto",
    deps = [":api_schema_proto"],
)
