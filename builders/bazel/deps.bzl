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

def python_deps(bazel_package):
    """Load rules_python and register container-based python toolchain

    Note: the bazel_package arg will depend on the import/submodule location in your workspace

    Args:
      bazel_package: repo-relative bazel package to builders/bazel/BUILD eg. "//builders/bazel"
    """
    http_archive(
        name = "rules_python",
        sha256 = "8c8fe44ef0a9afc256d1e75ad5f448bb59b81aba149b8958f02f7b3a98f5d9b4",
        strip_prefix = "rules_python-0.13.0",
        url = "https://github.com/bazelbuild/rules_python/archive/refs/tags/0.13.0.tar.gz",
    )
    native.register_toolchains("{}:py_toolchain".format(bazel_package))
