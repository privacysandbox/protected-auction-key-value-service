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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def scp_repositories():
    """Entry point for shared control plane repository and dependencies."""

    # Boost
    # latest as of 2022-06-09
    _RULES_BOOST_COMMIT = "789a047e61c0292c3b989514f5ca18a9945b0029"

    http_archive(
        name = "com_github_nelhage_rules_boost",
        sha256 = "c1298755d1e5f458a45c410c56fb7a8d2e44586413ef6e2d48dd83cc2eaf6a98",
        strip_prefix = "rules_boost-%s" % _RULES_BOOST_COMMIT,
        urls = [
            "https://github.com/nelhage/rules_boost/archive/%s.tar.gz" % _RULES_BOOST_COMMIT,
        ],
    )

    http_archive(
        name = "control_plane_shared",
        sha256 = "679ce9a94d0302078eecb92bc891d3763072fff3a51f1fc58c90b26cace3dce9",
        strip_prefix = "control-plane-shared-libraries-0.63.0",
        urls = [
            "https://github.com/privacysandbox/control-plane-shared-libraries/archive/refs/tags/v0.63.0.zip",
        ],
        patch_args = [
            # Needed to import Git-based patches.
            "-p1",
        ],
        patches = [
            "//third_party:shared_control_plane.patch",
        ],
    )

    git_repository(
        name = "oneTBB",
        # Commits on Apr 18, 2022
        commit = "9d2a3477ce276d437bf34b1582781e5b11f9b37a",
        remote = "https://github.com/oneapi-src/oneTBB.git",
        shallow_since = "1648820995 +0300",
    )
