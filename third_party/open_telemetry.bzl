# Copyright 2022 Google LLC
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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def open_telemetry_dependencies():
    http_archive(
        name = "io_opentelemetry_cpp",
        sha256 = "20fa97e507d067e9e2ab0c1accfc334f5a4b10d01312e55455dc3733748585f4",
        strip_prefix = "opentelemetry-cpp-1.8.2",
        urls = [
            "https://github.com/open-telemetry/opentelemetry-cpp/archive/refs/tags/v1.8.2.tar.gz",
        ],
    )
