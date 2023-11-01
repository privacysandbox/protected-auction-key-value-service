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

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "retrieval_request_builder",
    srcs = ["retrieval_request_builder.cc"],
    hdrs = ["retrieval_request_builder.h"],
    deps = [
        "//public/query/v2:get_values_v2_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "retrieval_request_builder_test",
    srcs = ["retrieval_request_builder_test.cc"],
    deps = [
        ":retrieval_request_builder",
        "//public/query/cpp:client_utils",
        "//public/test_util:proto_matcher",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "retrieval_response_parser",
    srcs = ["retrieval_response_parser.cc"],
    hdrs = ["retrieval_response_parser.h"],
    deps = [
        "//public/query/v2:get_values_v2_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "retrieval_response_parser_test",
    srcs = ["retrieval_response_parser_test.cc"],
    deps = [
        ":retrieval_response_parser",
        "@com_google_googletest//:gtest_main",
    ],
)
