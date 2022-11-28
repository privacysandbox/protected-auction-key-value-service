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

def quiche_dependencies():
    http_archive(
        name = "com_github_google_quiche",
        urls = ["https://github.com/google/quiche/archive/44a71d94ae4f89db150cd5ed664891793250551b.tar.gz"],
        strip_prefix = "quiche-44a71d94ae4f89db150cd5ed664891793250551b",
    )

    http_archive(
        name = "boringssl",
        sha256 = "482a59ea63d03fbb4d3cab22e40043fb4eef2cbbdc2eb944cd627675ae8b0cf3",  # Last updated 2022-05-18
        strip_prefix = "boringssl-02c6711efbb6c9dc6236a1c3779af1bcb0274dfd",
        urls = ["https://github.com/google/boringssl/archive/02c6711efbb6c9dc6236a1c3779af1bcb0274dfd.tar.gz"],
    )

    http_archive(
        name = "com_google_quic_trace",
        sha256 = "079331de8c3cbf145a3b57adb3ad4e73d733ecfa84d3486e1c5a9eaeef286549",  # Last updated 2022-05-18
        strip_prefix = "quic-trace-c7b993eb750e60c307e82f75763600d9c06a6de1",
        urls = ["https://github.com/google/quic-trace/archive/c7b993eb750e60c307e82f75763600d9c06a6de1.tar.gz"],
    )

    http_archive(
        name = "com_google_googleurl",
        sha256 = "a1bc96169d34dcc1406ffb750deef3bc8718bd1f9069a2878838e1bd905de989",
        urls = ["https://storage.googleapis.com/quiche-envoy-integration/googleurl_9cdb1f4d1a365ebdbcbf179dadf7f8aa5ee802e7.tar.gz"],
    )
