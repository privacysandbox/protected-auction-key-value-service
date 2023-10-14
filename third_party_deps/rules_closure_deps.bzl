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
load(
    "@io_bazel_rules_closure//closure:repositories.bzl",
    "rules_closure_dependencies",
    "rules_closure_toolchains",
)

def rules_closure_deps():
    rules_closure_dependencies()
    rules_closure_toolchains()

    # This is currently needed for closure_js_proto_library,
    # since the server is using protobuf 3.23*., which no longer has javascript support.
    # rules_closure uses this version of protoc.
    # To make `closure_js_proto_library` work, we need to pass
    # closure_js_proto_library(
    #   ...
    #   protocbin = "@com_google_protobuf_for_closure//:protoc"
    # )
    http_archive(
        name = "com_google_protobuf_for_closure",
        strip_prefix = "protobuf-3.19.1",
        sha256 = "87407cd28e7a9c95d9f61a098a53cf031109d451a7763e7dd1253abf8b4df422",
        urls = [
            "https://github.com/protocolbuffers/protobuf/archive/v3.19.1.tar.gz",
        ],
    )
