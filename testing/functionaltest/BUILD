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

genrule(
    name = "copy_to_dist",
    srcs = [
        "//testing/functionaltest/test_cases/grpc:delta_files",
    ],
    outs = ["copy_to_dist.bin"],
    cmd_bash = """cat << EOF > '$@'
mkdir -p dist/test_data/deltas
cp $(execpaths //testing/functionaltest/test_cases/grpc:delta_files) dist/test_data/deltas
builders/tools/normalize-dist
EOF""",
    executable = True,
    local = True,
    message = "copying test data to dist/test_data/deltas directory",
)
