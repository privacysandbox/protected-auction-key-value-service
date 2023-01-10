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
    name = "delta_record_reader",
    hdrs = ["delta_record_reader.h"],
    deps = [
        "//public/data_loading:records_utils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "riegeli_stream_io",
    hdrs = ["riegeli_stream_io.h"],
    deps = [
        "//public/data_loading:riegeli_metadata_cc_proto",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_riegeli//riegeli/bytes:istream_reader",
        "@com_google_riegeli//riegeli/records:record_reader",
    ],
)

cc_test(
    name = "riegeli_stream_io_test",
    size = "small",
    srcs = [
        "riegeli_stream_io_test.cc",
    ],
    deps = [
        ":riegeli_stream_io",
        "//public/test_util:mocks",
        "//public/test_util:proto_matcher",
        "@com_google_googletest//:gtest_main",
        "@com_google_riegeli//riegeli/bytes:string_writer",
        "@com_google_riegeli//riegeli/records:record_writer",
    ],
)

cc_library(
    name = "delta_record_stream_reader",
    hdrs = ["delta_record_stream_reader.h"],
    deps = [
        ":delta_record_reader",
        ":riegeli_stream_io",
        "//public/data_loading:records_utils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "delta_record_stream_reader_test",
    size = "small",
    srcs = ["delta_record_stream_reader_test.cc"],
    deps = [
        ":delta_record_stream_reader",
        "//public/data_loading:records_utils",
        "//public/data_loading/writers:delta_record_stream_writer",
        "@com_google_googletest//:gtest_main",
    ],
)
