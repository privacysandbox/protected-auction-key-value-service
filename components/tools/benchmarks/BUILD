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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

package(default_visibility = [
    "//production:__subpackages__",
])

cc_library(
    name = "benchmark_util",
    srcs = ["benchmark_util.cc"],
    hdrs = ["benchmark_util.h"],
    deps = [
        "//public/data_loading:records_utils",
        "//public/data_loading/writers:delta_record_stream_writer",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "benchmark_util_test",
    size = "small",
    srcs = ["benchmark_util_test.cc"],
    deps = [
        ":benchmark_util",
        "//public/data_loading/readers:delta_record_stream_reader",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "data_loading_benchmark",
    srcs = ["data_loading_benchmark.cc"],
    deps = [
        ":benchmark_util",
        "//components/data/blob_storage:blob_storage_client",
        "//components/data_server/cache",
        "//components/data_server/cache:key_value_cache",
        "//components/data_server/cache:noop_key_value_cache",
        "//components/util:platform_initializer",
        "//public/data_loading:data_loading_fbs",
        "//public/data_loading:records_utils",
        "//public/data_loading/readers:riegeli_stream_io",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_benchmark//:benchmark",
    ],
)

cc_binary(
    name = "cache_benchmark",
    srcs = ["cache_benchmark.cc"],
    deps = [
        ":benchmark_util",
        "//components/data_server/cache",
        "//components/data_server/cache:key_value_cache",
        "//components/data_server/cache:noop_key_value_cache",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_benchmark//:benchmark",
    ],
)
