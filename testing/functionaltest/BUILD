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

sut_names = [
    "baseline-dsp",
    "baseline-ssp",
    "multiple-kv-servers",
    "no-udf",
]

sut_cp_cmds = [
    "cp $(execpaths //testing/functionaltest/suts/{sut}:sut_data) dist/test_data/{sut}.zip".format(sut = sut)
    for sut in sut_names
]

genrule(
    name = "copy_to_dist",
    srcs = [
        "//testing/functionaltest/suts/{}:sut_data".format(sut)
        for sut in sut_names
    ],
    outs = ["copy_to_dist.bin"],
    cmd_bash = """cat << EOF > '$@'
mkdir -p dist/test_data
{cp_cmds}
chmod -x dist/test_data/*.zip
builders/tools/normalize-dist
EOF""".format(cp_cmds = "; ".join(sut_cp_cmds)),
    executable = True,
    local = True,
    message = "copying test artifacts to dist/test_data directory",
)
