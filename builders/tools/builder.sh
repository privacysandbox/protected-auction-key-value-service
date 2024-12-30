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
  if [[ -v WORKSPACE ]]; then
    return
  fi
  local -r GIT_TOPLEVEL="$(git rev-parse --show-superproject-working-tree)"
  if [[ -n ${GIT_TOPLEVEL} ]]; then
    WORKSPACE="${GIT_TOPLEVEL}"
  else
    WORKSPACE="$(git rev-parse --show-toplevel)"
  fi
  local -r ws_path="$(realpath "${WORKSPACE}"/WORKSPACE)"
  WORKSPACE="$(dirname "${ws_path}")"
}

#######################################
# Return the path to the build workspace, for use with
# the docker volume or mount argument
#######################################
function builder::get_docker_workspace_mount() {
  if [[ -v WORKSPACE_MOUNT ]]; then
    printf "%s" "${WORKSPACE_MOUNT}"
    return
  fi
  # when running inside a docker container, expect /.dockerenv to exist
  if ! [[ -f /.dockerenv ]]; then
    printf "%s" "${WORKSPACE}"
    return
  fi

  # if running inside a docker container, the workspace mount point cannot be
  # determined by git or bazel or inspecting the filesystem itself. Instead, we
  # need to use docker to expose its mount info for the /src/workspace path.
  # determine the current container's ID
  local -r CONTAINER_ID="$(uname --nodename)"
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
    "AWS_CONTAINER_CREDENTIALS_RELATIVE_URI"
  )
}

#######################################
# Build a Nitro EIF from a docker image tarfile
# Arguments:
#   DOCKER_IMAGE_TAR   Docker image tarfile
#   DOCKER_IMAGE_URI  Docker image uri corresponding to tarfile (without tag)
#   DOCKER_IMAGE_TAG  Docker image tag corresponding to tarfile
#   OUTPUT_PATH  Output path, must be within the workspace
#   EIF_NAME  Output base filename
#   BUILDER_IMAGE_NAME (optional)  Amazon Linux builder image name
#######################################
function builder::docker_img_to_nitro() {
  local -r docker_image_tar="$1"
  local -r docker_image_uri="$2"
  local -r docker_image_tag="$3"
  local -r output_path="$4"
  local -r eif_name="$5"
  local -r image="${6-build-amazonlinux2023}"
  local -r temp_tag="$(mktemp --dry-run temp-XXXXXX)"
  docker load -i "${docker_image_tar}"
  # add a temp tag to reduce the chance of conflicts or race conditions
  docker tag "${docker_image_uri}:${docker_image_tag}" "${docker_image_uri}:${temp_tag}"
  builder::docker_uri_to_nitro \
    "${docker_image_uri}:${temp_tag}" \
    "${output_path}/${eif_name}" \
    "${image}"
  # remove the temp tag
  docker image rm "${docker_image_uri}:${temp_tag}"
}

#######################################
# Build a Nitro EIF from a docker image
# Arguments:
#   DOCKER_URI (with tag)
#   OUTPUT_BASE_FILENAME  path and base filename
#   BUILDER_IMAGE_NAME (optional)  Amazon Linux builder image name
#######################################
function builder::docker_uri_to_nitro() {
  local -r docker_uri="$1"
  local -r output_base_fname="$2"
  local -r image="${3-build-amazonlinux2023}"
  local -r output_eif="${output_base_fname}".eif
  local -r output_json="${output_base_fname}".json
  local -r output_pcr0_json="${output_base_fname}".pcr0.json
  local -r output_pcr0_txt="${output_base_fname}".pcr0.txt
  builder::cbuild_al "${image}" "
nitro-cli build-enclave --docker-uri ${docker_uri} --output-file ${output_eif@Q} >${output_json@Q}
jq --compact-output '{PCR0: .Measurements.PCR0}' ${output_json@Q} >${output_pcr0_json@Q}
jq --compact-output --raw-output '.Measurements.PCR0'  ${output_json@Q} >${output_pcr0_txt@Q}
"
}

#######################################
# Invoke cbuild tool in an amazonlinux build container
# Arguments:
#   BUILDER_IMAGE_NAME  Amazon Linux builder image name
#######################################
function builder::cbuild_al() {
  local -r image="$1"
  shift
  local -r cbuild="$(builder::get_tools_dir)"/cbuild
  declare -a env_vars
  builder::add_aws_env_vars env_vars
  declare env_args
  for evar in "${env_vars[@]}"; do
    env_args+=(--env "${evar}")
  done
  printf "=== cbuild %s action envs ===\n" "${image}"
  # shellcheck disable=SC2086
  "${cbuild}" ${CBUILD_ARGS} "${env_args[@]}" --image "${image}" --cmd "grep -o 'action_env.*' /etc/bazel.bazelrc 1>/dev/stderr 2>/dev/null"
  # shellcheck disable=SC2086
  "${cbuild}" ${CBUILD_ARGS} "${env_args[@]}" --image "${image}" --cmd "$*"
}

function builder::cbuild_al2023() {
  builder::cbuild_al build-amazonlinux2023 "$@"
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
