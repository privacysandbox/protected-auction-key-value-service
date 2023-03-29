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

#
# library of build-related functions
#

#######################################
# Configure and export the WORKSPACE variable in kokoro
# Globals:
#   WORKSPACE (optional)
#######################################
function kokoro::set_workspace() {
  if [[ -n ${KOKORO_ARTIFACTS_DIR} ]]; then
    WORKSPACE="${KOKORO_ARTIFACTS_DIR}/git/functionaltest-system"
  elif [[ -z ${WORKSPACE} ]]; then
    WORKSPACE="$(git rev-parse --show-toplevel)"
  fi
  export WORKSPACE
}

#######################################
# Execute tool build_and_test_all_in_docker
# Globals:
#   WORKSPACE (optional)
#   KOKORO_ARTIFACTS_DIR (optional)
#######################################
function kokoro::configure_build_env() {
  export BAZEL_STARTUP_ARGS="--bazelrc=google_internal/.bazelrc"
  declare -a _BAZEL_ARGS=(
    "--config=rbecache"
  )
  if [[ -n ${KOKORO_ARTIFACTS_DIR} ]]; then
    trap "sleep 5s" RETURN
  fi

  kokoro::set_workspace

  declare -r GAR_HOST=us-docker.pkg.dev
  declare -r GAR_PROJECT=kiwi-air-force-remote-build
  declare -r GAR_REPO="${GAR_HOST}/${GAR_PROJECT}/privacysandbox/builders"

  if [[ -z ${KOKORO_ARTIFACTS_DIR} ]]; then
    declare -r _ACCOUNT="${USER}@google.com"
    if [[ $(gcloud config get account) != "${_ACCOUNT}" ]]; then
      printf "Error. Set default gcloud account using \`gcloud config set account %s\`\n" "${_ACCOUNT}"
      return 1
    fi
    if [[ $(gcloud config get project) != "${GAR_PROJECT}" ]]; then
      printf "Error. Set default gcloud project using \`gcloud config set project %s\`\n" "${GAR_PROJECT}"
      return 1
    fi
  fi

  # update gcloud if it doesn't support artifact registry
  if ! gcloud artifacts --help &>/dev/null; then
    yes | gcloud components update
  fi

  # update docker config to use gcloud for auth to required artifact registry repo
  if ! yes | gcloud auth configure-docker ${GAR_HOST} >/dev/null; then
    printf "Error configuring docker for Artifact Registry [%s]\n" "${GAR_HOST}"
    return 1
  fi

  # test connecting to GAR repo
  if ! gcloud artifacts docker images list "${GAR_REPO}/presubmit" --include-tags --limit 1 >/dev/null; then
    printf "Error connecting to Artifact Registry [%s]\n" "${GAR_REPO}"
    return 1
  fi

  for IMAGE in presubmit build-debian test-tools; do
    printf "Pulling or generating image [%s]\n" "${IMAGE}"
    if ! "${WORKSPACE}"/google_internal/tools/pull_builder_image --image ${IMAGE} --verbose; then
      printf "Error pulling, regenerating or pushing image [%s]\n" "${IMAGE}"
      return 1
    fi
  done

  export BAZEL_DIRECT_ARGS="${_BAZEL_ARGS[*]} --google_default_credentials"
  declare -a DOCKER_RUN_ARGS
  # optionally set credentials (likely useful only if executing this outside kokoro)
  declare -r HOST_CREDS_JSON="${HOME}/.config/gcloud/application_default_credentials.json"
  if [[ -s ${HOST_CREDS_JSON} ]]; then
    declare -r CREDS_JSON=/gcloud/application_default_credentials.json
    export BAZEL_EXTRA_ARGS="${_BAZEL_ARGS[*]} --google_credentials=${CREDS_JSON}"
    DOCKER_RUN_ARGS+=(
      "--volume ${HOST_CREDS_JSON}:${CREDS_JSON}"
    )
  else
    export BAZEL_EXTRA_ARGS="${BAZEL_DIRECT_ARGS}"
  fi

  export EXTRA_DOCKER_RUN_ARGS="${DOCKER_RUN_ARGS[*]}"
}
