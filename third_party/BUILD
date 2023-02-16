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

package(
    default_visibility = ["//visibility:public"],
)

alias(
    name = "grpc_cli",
    actual = "@com_github_grpc_grpc//test/cpp/util:grpc_cli",
)

genrule(
    name = "copy_to_dist",
    srcs = [
        ":grpc_cli",
    ],
    outs = ["copy_to_dist.bin"],
    cmd_bash = """cat << EOF > '$@'
mkdir -p dist/aws
cp $(execpath :grpc_cli) dist/debian
chmod 755 dist/debian/grpc_cli
EOF""",
    executable = True,
    local = True,
    message = "copying grpc_cli to dist/debian/ directory",
)

exports_files(glob(["*.patch"]))
