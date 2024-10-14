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

"""Load definitions for use in WORKSPACE files."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def python_deps():
    """Load rules_python. Use python_register_toolchains to also resgister container-based python toolchain."""
    maybe(
        http_archive,
        name = "rules_python",
        sha256 = "be04b635c7be4604be1ef20542e9870af3c49778ce841ee2d92fcb42f9d9516a",
        strip_prefix = "rules_python-0.35.0",
        url = "https://github.com/bazelbuild/rules_python/releases/download/0.35.0/rules_python-0.35.0.tar.gz",
    )

def python_register_toolchains(bazel_package):
    """Register container-based python toolchain.

    Note: the bazel_package arg will depend on the import/submodule location in your workspace

    Args:
      bazel_package: repo-relative bazel package to builders/bazel/BUILD eg. "//builders/bazel"
    """
    native.register_toolchains("{}:py_toolchain".format(bazel_package))
