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

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@rules_buf//buf:defs.bzl", "buf_lint_test")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

buf_lint_test(
    name = "get_values_proto_lint",
    size = "small",
    config = "//:buf.yaml",
    targets = [
        ":get_values_v2_proto",
    ],
)

proto_library(
    name = "get_values_v2_proto",
    srcs = ["get_values_v2.proto"],
    deps = [
        "//public:api_schema_proto",
        "@com_google_googleapis//google/api:annotations_proto",
        "@com_google_googleapis//google/api:httpbody_proto",
        "@com_google_googleapis//google/rpc:status_proto",
        "@com_google_protobuf//:struct_proto",
    ],
)

cc_proto_library(
    name = "get_values_v2_cc_proto",
    deps = [":get_values_v2_proto"],
)

cc_grpc_library(
    name = "get_values_v2_cc_grpc",
    srcs = [":get_values_v2_proto"],
    grpc_only = True,
    deps = [":get_values_v2_cc_proto"],
)
