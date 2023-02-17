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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library", "cc_test")
load("@rules_proto//proto:defs.bzl", "proto_descriptor_set", "proto_library")

proto_library(
    name = "custom_audience_data_proto",
    srcs = ["data/custom_audience_data.proto"],
    deps = [
        "@com_google_protobuf//:struct_proto",
    ],
)

proto_descriptor_set(
    name = "custom_audience_data_proto_descriptor_set",
    deps = [":custom_audience_data_proto"],
)

cc_proto_library(
    name = "custom_audience_data_cc_proto",
    deps = [":custom_audience_data_proto"],
)

cc_library(
    name = "delta_key_value_writer",
    srcs = ["delta_key_value_writer.cc"],
    hdrs = ["delta_key_value_writer.h"],
    deps = [
        "//public:constants",
        "//public/data_loading/writers:delta_record_stream_writer",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "custom_audience_data_parser",
    srcs = ["custom_audience_data_parser.cc"],
    hdrs = ["custom_audience_data_parser.h"],
    deps = [
        ":custom_audience_data_cc_proto",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "delta_key_value_writer_test",
    size = "small",
    srcs = ["delta_key_value_writer_test.cc"],
    deps = [
        ":delta_key_value_writer",
        "//public/data_loading/readers:delta_record_stream_reader",
        "//public/data_loading/writers:delta_record_stream_writer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "custom_audience_data_parser_test",
    size = "small",
    srcs = ["custom_audience_data_parser_test.cc"],
    deps = [
        ":custom_audience_data_parser",
        "@com_google_googletest//:gtest_main",
    ],
)
