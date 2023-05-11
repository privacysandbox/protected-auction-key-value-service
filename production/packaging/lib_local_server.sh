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

# library of functions for running and testing a local server

# require this script to be sourced rather than executed
if ! (return 0 2>/dev/null); then
  printf "Error: Script %s must be sourced\n" "${BASH_SOURCE[0]}" &>/dev/stderr
  exit 1
fi

function local_server::_sut_cleanup() {
  declare -r -i STATUS=$?
  declare -n _cleanup_args=$1
  if [[ ${_cleanup_args[0]} ]]; then
    docker compose "${_cleanup_args[@]}" down
  fi
  return ${STATUS}
}

function local_server::run_and_test_sut() {
  declare -r sut_dir="$1"
  declare -r sut_name="${sut_dir##*/}"

  set -o errexit
  if [[ -z ${WORKSPACE} ]]; then
    printf "error: WORKSPACE variable not defined\n" &>/dev/stderr
    exit 1
  fi

  declare -r compose_yaml="${sut_dir}"/docker-compose.yaml
  declare -r compose_env="${sut_dir}"/docker-compose.env
  if ! [[ -s ${compose_yaml} ]]; then
    printf "SUT yaml file [%s] must be non-empty and readable\n" "${compose_yaml}"
    exit 1
  fi
  if ! [[ -s ${compose_env} ]]; then
    printf "SUT env file [%s] must be non-empty and readable\n" "${compose_env}"
    exit 1
  fi

  local -r tmp_env=$(mktemp)
  cat <<EOF >"${tmp_env}"
SUT_DATA_DIR="${WORKSPACE}/dist/test_data/${sut_name}"
EOF
  declare -a -r docker_compose_args=(
    --file "${sut_dir}"/docker-compose.yaml
    --env-file "${sut_dir}"/docker-compose.env
    --env-file "${tmp_env}"
  )

  trap "local_server::_sut_cleanup docker_compose_args && rm -f \${tmp_env@Q}" ERR RETURN
  docker compose "${docker_compose_args[@]}" up --quiet-pull --detach
  "${WORKSPACE}"/testing/functionaltest/run-tests --sut-name "${sut_name}"
}

function local_server::run_and_test() {
  if [[ -z ${WORKSPACE} ]]; then
    printf "error: WORKSPACE variable not defined\n" &>/dev/stderr
    return 1
  fi
  declare -r selected_test="$1"
  declare -a selected_test_filter
  if [[ -n ${selected_test} ]]; then
    selected_test_filter+=(
      grep -w "${selected_test}"
    )
  else
    selected_test_filter+=(cat)
  fi
  declare -a suts
  readarray -t suts <<< "$(find "${WORKSPACE}"/testing/functionaltest/suts -mindepth 1 -maxdepth 1 -type d | "${selected_test_filter[@]}" | sort)"
  declare sut_dir
  for sut_dir in "${suts[@]}"; do
    local_server::run_and_test_sut "${sut_dir}"
  done
  "${WORKSPACE}"/builders/tools/bazel-debian run //:collect-logs local-local-logs.zip
}
