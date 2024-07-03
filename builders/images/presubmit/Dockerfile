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

FROM ubuntu:24.04

COPY install_apps install_go.sh install_golang_apps .bazelversion .pre-commit-config.yaml /scripts/
COPY gitconfig /etc

ARG PRE_COMMIT_VENV_DIR=/usr/pre-commit-venv
ARG TARGETARCH
ENV BUILD_ARCH="${TARGETARCH}" \
    WORKSPACE=/src/workspace \
    PRE_COMMIT_HOME=/var/cache/pre-commit \
    PRE_COMMIT_TOOL=${PRE_COMMIT_VENV_DIR}/bin/pre-commit \
    TZ=Etc/UTC

RUN  \
  chmod 644 /etc/gitconfig && \
  /usr/bin/env -v PRE_COMMIT_VENV_DIR=${PRE_COMMIT_VENV_DIR} /scripts/install_apps && \
  /scripts/install_golang_apps && \
  rm -rf /scripts

ENV PATH="${PATH}:/usr/local/go/bin"
