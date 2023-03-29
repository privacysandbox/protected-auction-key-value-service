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

function local_server::run() {
  declare -r -i port=$1

  if [[ -z ${WORKSPACE} ]]; then
    printf "error: WORKSPACE variable not defined\n" &>/dev/stderr
    return 1
  fi
  # Create temporary directores for holding deltas and realtime files
  # TODO(b/272558207): Clean these up after the run.
  declare -r delta_directory="$(mktemp -d -t "delta_XXXXXXXXXX")"
  declare -r realtime_directory="$(mktemp -d -t "realtime_XXXXXXXXXX")"
  cp "${WORKSPACE}"/dist/test_data/deltas/* "${delta_directory}"
  declare -r -a server_args=(
    --port ${port}
    --delta_directory "${delta_directory}"
    --realtime_directory "${realtime_directory}"
  )
  "${WORKSPACE}"/dist/debian/server "${server_args[@]}" >/dev/null &
  declare -r -i server_pid=$!
  printf "%d" ${server_pid}
}

function local_server::run_and_test() {
  if [[ -z ${WORKSPACE} ]]; then
    printf "error: WORKSPACE variable not defined\n" &>/dev/stderr
    return 1
  fi
  trap "kill -s SIGTERM \${server_pid} && wait" ERR RETURN
  EXTRA_DOCKER_RUN_ARGS="--workdir /src/workspace/dist/debian" \
    "${WORKSPACE}"/builders/tools/unzip -j -u server_artifacts.zip server/bin/server
  touch "${WORKSPACE}"/dist/debian/server || true
  declare -r -i port=$(local_server::find_unused_port)
  declare -r -i server_pid=$(local_server::run ${port})
  "${WORKSPACE}"/testing/functionaltest/run-tests --network host --endpoint localhost:${port} --verbose
}
