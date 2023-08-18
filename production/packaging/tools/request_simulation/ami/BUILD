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

load("@rules_pkg//pkg:zip.bzl", "pkg_zip")

# AWS artifacts for the AMI
pkg_zip(
    name = "aws_artifacts",
    srcs = [
        "//production/packaging/tools/request_simulation/otel_collector:aws-otel-collector.rpm",
        "//production/packaging/tools/request_simulation/otel_collector:aws_otel_collector_cfg",
    ],
)

genrule(
    name = "copy_to_dist",
    srcs = [
        ":aws_artifacts",
    ],
    outs = ["copy_to_dist.bin"],
    cmd_bash = """cat << EOF > '$@'
mkdir -p dist/request_simulation/aws
cp $(execpath :aws_artifacts) dist/request_simulation/aws
EOF""",
    executable = True,
    local = True,
    message = "copying AMI artifacts to dist/request_simulation/aws directory",
)
