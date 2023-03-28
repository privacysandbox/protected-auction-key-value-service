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

#
# source this helper script to set up your development environment
#

# require this script to be sourced rather than executed
if ! (return 0 2>/dev/null); then
    printf "Error: Script %s must be sourced\n" "${BASH_SOURCE[0]}" &>/dev/stderr
    exit 1
fi

SCRIPT_DIR="$(dirname $(readlink -f "${BASH_SOURCE[0]}"))"
. "${SCRIPT_DIR}"/kokoro/lib_build.sh

kokoro::configure_build_env

function bazel_rbe() {
  declare -r CMD="$1"
  shift
  bazel ${BAZEL_STARTUP_ARGS} ${CMD} ${BAZEL_DIRECT_ARGS} "$@"
}
