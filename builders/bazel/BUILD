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

"""Register container-based python toolchain."""

load("@bazel_tools//tools/python:toolchain.bzl", "py_runtime_pair")
load("@rules_python//python:defs.bzl", "py_runtime")

# define the python3 runtime; this path must exist in the bazel build environment
PY3_PATH = "/opt/bin/python3"

py_runtime(
    name = "py_runtime",
    interpreter_path = PY3_PATH,
    python_version = "PY3",
    visibility = ["//visibility:public"],
)

py_runtime_pair(
    name = "py_runtime_pair",
    py2_runtime = None,
    py3_runtime = ":py_runtime",
)

toolchain(
    name = "py_toolchain",
    toolchain = ":py_runtime_pair",
    toolchain_type = "@bazel_tools//tools/python:toolchain_type",
)
