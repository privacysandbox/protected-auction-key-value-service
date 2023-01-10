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
    name = "data_orchestrator",
    srcs = [
        "data_orchestrator.cc",
    ],
    hdrs = [
        "data_orchestrator.h",
    ],
    deps = [
        "//components/data:blob_storage_change_notifier",
        "//components/data:blob_storage_client",
        "//components/data:delta_file_notifier",
        "//components/data_server/cache",
        "//components/errors:retry",
        "//components/telemetry",
        "//public:constants",
        "//public/data_loading:data_loading_fbs",
        "//public/data_loading:filename_utils",
        "//public/data_loading/readers:riegeli_stream_io",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "data_orchestrator_test",
    size = "small",
    srcs = [
        "data_orchestrator_test.cc",
    ],
    deps = [
        ":data_orchestrator",
        "//components/data:mocks",
        "//components/data_server/cache:mocks",
        "//public/data_loading:filename_utils",
        "//public/data_loading:records_utils",
        "//public/test_util:mocks",
        "//public/test_util:proto_matcher",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
