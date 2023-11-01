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

load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_image",
    "container_layer",
    "container_push",
)
load("@rules_python//python:defs.bzl", "py_library", "py_test")

py_library(
    name = "sqs_cleanup_manager",
    srcs = ["sqs_cleanup_manager.py"],
)

py_test(
    name = "sqs_cleanup_manager_test",
    size = "small",
    srcs = ["sqs_cleanup_manager_test.py"],
    python_version = "PY3",
    deps = [
        ":sqs_cleanup_manager",
    ],
)

container_layer(
    name = "lambda_binary_layer",
    directory = "/var/task",  # ${LAMBDA_TASK_ROOT}
    files = [
        "sqs_cleanup.py",
        "sqs_cleanup_manager.py",
    ],
)

container_image(
    name = "sqs_lambda",
    base = select({
        "@platforms//cpu:arm64": "@aws-lambda-python-arm64//image",
        "@platforms//cpu:x86_64": "@aws-lambda-python-amd64//image",
    }),
    cmd = ["sqs_cleanup.handler"],
    layers = [
        ":lambda_binary_layer",
    ],
    visibility = ["//production/packaging:__subpackages__"],
)

container_push(
    name = "sqs_lambda_push_aws_ecr",
    format = "Docker",
    image = ":sqs_lambda",
    registry = "$${AWS_ECR}",
    repository = "sqs_lambda",
    tag = "latest",
)

exports_files(
    ["push_sqs"],
)
