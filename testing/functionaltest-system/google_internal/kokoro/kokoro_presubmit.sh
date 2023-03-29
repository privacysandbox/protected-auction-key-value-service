#!/bin/bash
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

set -o errexit
set -o xtrace
export TZ=Etc/UTC
export PS4='+\t $(basename ${BASH_SOURCE[0]}):${LINENO} ' # xtrace prompt

trap _cleanup EXIT
function _cleanup() {
  sleep 5s
}

# shellcheck disable=SC1091
source "$(dirname "$(readlink -f "$0")")"/lib_build.sh

export IMAGE_BUILD_VERBOSE=1
kokoro::configure_build_env

"${WORKSPACE}"/builders/tools/pre-commit
"${WORKSPACE}"/tests/local/docker-deploy-test-teardown
ARCH="$("${WORKSPACE}"/builders/tools/get-architecture)"
readonly ARCH

declare -r PERFGATE_APP_PATH=chrome/privacy_sandbox/potassium_engprod/perfgate
declare -r PERFGATE_IMAGE=perfgate_uploader_gce_"${ARCH}"_image
declare -r  PERFGATE_TAR_DIR="${KOKORO_BLAZE_DIR}"/perfgate_uploader_tar/blaze-bin/"${PERFGATE_APP_PATH}"
docker load --input "${PERFGATE_TAR_DIR}"/${PERFGATE_IMAGE}.tar
docker run --rm bazel/"${PERFGATE_APP_PATH}":${PERFGATE_IMAGE} --version
