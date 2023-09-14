# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def rules_closure_repositories():
    http_archive(
        name = "io_bazel_rules_closure",
        sha256 = "9498e57368efb82b985db1ed426a767cbf1ba0398fd7aed632fc3908654e1b1e",
        strip_prefix = "rules_closure-0.12.0",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_closure/archive/0.12.0.tar.gz",
            "https://github.com/bazelbuild/rules_closure/archive/0.12.0.tar.gz",
        ],
    )
