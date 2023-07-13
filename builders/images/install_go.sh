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

# shell library to install and uninstall golang

function _golang_install_dir() {
  printf "/usr/local/go\n"
}

function install_golang() {
  declare -r _ARCH="$1"
  declare -r FNAME=gobin.tar.gz
  declare -r VERSION=1.20.4
  # shellcheck disable=SC2155
  declare -r GO_INSTALL_DIR="$(_golang_install_dir)"
  declare -r -A GO_HASHES=(
    [amd64]="698ef3243972a51ddb4028e4a1ac63dc6d60821bf18e59a807e051fee0a385bd"
    [arm64]="105889992ee4b1d40c7c108555222ca70ae43fccb42e20fbf1eebb822f5e72c6"
  )
  declare -r GO_HASH=${GO_HASHES[${_ARCH}]}
  if [[ -z ${GO_HASH} ]]; then
    printf "Unrecognized or unsupported architecture for golang: %s\n" "${_ARCH}" &>/dev/stderr
    exit 1
  fi

  curl --silent -fSL --output ${FNAME} "https://go.dev/dl/go${VERSION}.linux-${_ARCH}.tar.gz"
  echo "${GO_HASH} ${FNAME}" | sha256sum -c
  tar --directory /usr/local -xzf ${FNAME}
  rm -f ${FNAME}
  update-alternatives --install /usr/bin/go go "${GO_INSTALL_DIR}"/bin/go 100

  go version
}

function remove_golang() {
  update-alternatives --remove-all go
  rm -rf "$(_golang_install_dir)"
}
