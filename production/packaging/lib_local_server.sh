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

function local_server::find_unused_port() {
  declare -i port
  for port in {50051..50150}; do
    if ! echo "" >/dev/tcp/127.0.0.1/${port}; then
      printf "%d" ${port}
      return
    fi
  done 2>/dev/null
  exit 1
}

function local_server::create_network() {
  declare -r network_name="$(mktemp --dry-run --suffix=-net kvsrv-local-XXX)"
  docker network create "${network_name}" >/dev/null
  printf "%s" "${network_name}"
}

function local_server::run_docker_script() {
  declare -r network="$1"
  DOCKER_NETWORK="${network}" \
    "${WORKSPACE}"/testing/functionaltest/run-server-docker
}

function local_server::run_docker() {
  declare -r _network="$1"
  declare -r _host="$2"
  declare -r -i _port="$3"
  declare -r WORKSPACE_MOUNT="$(builder::get_docker_workspace_mount)"
  declare -r container_name="${_network}-${_host}"
  docker run \
    --rm \
    --detach \
    --network "${_network}" \
    --name "${container_name}" \
    --hostname "${_host}" \
    --entrypoint=/server/bin/init_server_basic \
    --volume "${WORKSPACE_MOUNT}"/dist/test_data/deltas:/deltas \
    --tmpfs /realtime_data \
    bazel/production/packaging/aws/data_server:server_docker_image \
      --port ${_port} \
      --delta_directory /deltas \
      --realtime_directory /realtime_data \
    >/dev/null
  printf "%s" "${_network}-${_host}"
  printf "running server on docker network: %s\n" "${_network}" &>/dev/stderr
  if [[ -t 0 ]] && [[ -t 1 ]]; then
    docker container ls --filter network="${_network}"
  fi
}

function local_server::run_and_test() {
  if [[ -z ${WORKSPACE} ]]; then
    printf "error: WORKSPACE variable not defined\n" &>/dev/stderr
    return 1
  fi
  trap "docker container rm --force \${container_name} >/dev/null && docker network rm \${network} >/dev/null" ERR RETURN
  declare -r network="$(local_server::create_network)"
  declare -r host=srv
  declare -r -i port=2000
  declare -r container_name="$(local_server::run_docker "${network}" "${host}" ${port})"
  "${WORKSPACE}"/testing/functionaltest/run-tests --network "${network}" --endpoint "${host}:${port}" --verbose
}
