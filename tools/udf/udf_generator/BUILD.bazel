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

load("@rules_cc//cc:defs.bzl", "cc_binary")

package(default_visibility = [
    "//production/packaging/tools:__subpackages__",
    "//testing:__subpackages__",
    "//tools:__subpackages__",
])

cc_binary(
    name = "udf_delta_file_generator",
    srcs = ["udf_delta_file_generator.cc"],
    deps = [
        "//public/data_loading:data_loading_fbs",
        "//public/data_loading:filename_utils",
        "//public/data_loading:records_utils",
        "//public/data_loading:riegeli_metadata_cc_proto",
        "//public/data_loading/writers:delta_record_stream_writer",
        "//public/data_loading/writers:delta_record_writer",
        "//public/udf:constants",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/strings",
        "@com_google_riegeli//riegeli/bytes:ostream_writer",
        "@com_google_riegeli//riegeli/records:record_writer",
    ],
)
