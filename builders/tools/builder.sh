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
# shell package enabling support for docker
#

# require this script to be sourced rather than executed
if ! (return 0 2>/dev/null); then
    printf "Error: Script %s must be sourced\n" "${BASH_SOURCE[0]}" &>/dev/stderr
    exit 1
fi

#######################################
# Configure and export the WORKSPACE variable. Does not
# overwrite existing WORKSPACE value.
#######################################
function builder::set_workspace() {
  export WORKSPACE
  if [[ -n ${WORKSPACE} ]]; then
    return
  fi
  local GIT_TOPLEVEL="$(git rev-parse --show-superproject-working-tree)"
  if [[ -n ${GIT_TOPLEVEL} ]]; then
    WORKSPACE="${GIT_TOPLEVEL}"
  else
    WORKSPACE="$(git rev-parse --show-toplevel)"
  fi
}

#######################################
# Return the path to the build workspace, for use with
# the docker volume or mount argument
#######################################
function builder::get_docker_workspace_mount() {
  # when running inside a docker container, expect /.dockerenv to exist
  if ! [[ -f /.dockerenv ]]; then
    printf "%s" "${WORKSPACE}"
    return
  fi

  # if running inside a docker container, the workspace mount point cannot be
  # determined by git or bazel or inspecting the filesystem itself. Instead, we
  # need to use docker to expose its mount info for the /src/workspace path.
  # determine the current container's ID
  declare CONTAINER_ID
  CONTAINER_ID="$(uname --nodename)"
  # use docker inspect to extract the current mount path for /src/workspace
  # this format string is a golang template (https://pkg.go.dev/text/template) processed
  # by docker's --format flag, per https://docs.docker.com/config/formatting/
  # shellcheck disable=SC2016
  declare -r FORMAT_STR='
    {{- range $v := .HostConfig.Binds -}}
      {{$pathpair := split $v ":" -}}
      {{if eq (index $pathpair 1) "/src/workspace" -}}
        {{print (index $pathpair 0) -}}
      {{end -}}
    {{end -}}
  '
  local -r MOUNT_PATH="$(docker inspect --format "${FORMAT_STR}" "${CONTAINER_ID}")"
  if [[ -z ${MOUNT_PATH} ]]; then
    printf "Error: Unable to determine mount point for /src/workspace. Exiting\n" &>/dev/stderr
    exit 1
  fi
  printf "%s" "${MOUNT_PATH}"
}

function builder::get_tools_dir() {
  dirname "$(readlink -f "${BASH_SOURCE[0]}")"
}

#######################################
# Invoke cbuild tool in a build-debian container
#######################################
function builder::cbuild_debian() {
  local -r CBUILD="$(builder::get_tools_dir)"/cbuild
  printf "=== cbuild debian action envs ===\n"
  # shellcheck disable=SC2086
  "${CBUILD}" ${CBUILD_ARGS} --image build-debian --cmd "grep -o 'action_env.*' /etc/bazel.bazelrc 1>/dev/stderr 2>/dev/null"
  # shellcheck disable=SC2086
  "${CBUILD}" ${CBUILD_ARGS} --image build-debian --cmd "$*"
}

#######################################
# Add AWS env vars to a specified array
# Arguments:
#   * the name of an array to which to append values
#######################################
function builder::add_aws_env_vars() {
  declare -n args=$1
  args+=(
    "AWS_ACCESS_KEY_ID"
    "AWS_SECRET_ACCESS_KEY"
    "AWS_SESSION_TOKEN"
    "AWS_REGION"
    "AWS_DEFAULT_REGION"
    "AWS_PROFILE"
  )
}

#######################################
# Invoke cbuild tool in a build-amazonlinux2 container
#######################################
function builder::cbuild_al2() {
  local -r CBUILD="$(builder::get_tools_dir)"/cbuild
  declare -a env_vars
  builder::add_aws_env_vars env_vars
  declare env_args
  for evar in "${env_vars[@]}"
  do
    env_args+=("--env" "${evar}")
  done
  printf "=== cbuild amazonlinux2 action envs ===\n"
  # shellcheck disable=SC2086
  "${CBUILD}" ${CBUILD_ARGS} "${env_args[@]}" --image build-amazonlinux2 --cmd "grep -o 'action_env.*' /etc/bazel.bazelrc 1>/dev/stderr 2>/dev/null"
  # shellcheck disable=SC2086
  "${CBUILD}" ${CBUILD_ARGS} "${env_args[@]}" --image build-amazonlinux2 --cmd "$*"
}

#######################################
# Return the numeric id of the user or group
# Arguments:
#   single character, either "u" for user or "g" for group
#######################################
function builder::id() {
  declare -r mode="$1"
  declare -i _id
  _id=$(id "-${mode}")
  if [[ ${_id} -ne 0 ]]; then
    printf "%s" "${_id}"
  else
    # id is 0 (root), use the owner/group of the WORKSPACE file instead
    stat -c "%${mode}" "${WORKSPACE}"/WORKSPACE
  fi
}

#######################################
# invoke functions to configure the build environment
#######################################

builder::set_workspace
