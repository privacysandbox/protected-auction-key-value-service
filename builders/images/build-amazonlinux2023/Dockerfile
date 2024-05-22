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

FROM amazonlinux:2023.4.20240416.0

COPY /install_apps install_golang_apps install_go.sh generate_system_bazelrc .bazelversion /scripts/
COPY get_workspace_mount /usr/local/bin
COPY gitconfig /etc

ARG TARGETARCH
ENV BUILD_ARCH="${TARGETARCH}" \
    WORKSPACE=/src/workspace \
    CC=clang

RUN  \
  chmod 644 /etc/gitconfig && \
  /scripts/install_apps && \
  /scripts/generate_system_bazelrc --user-root-name amazonlinux2023 && \
  /scripts/install_golang_apps && \
  rm -rf /scripts

ENV PATH="${PATH}:/usr/local/go/bin:/opt/bin" \
    PYTHON_BIN_PATH="/opt/bin/python3" \
    PYTHON_LIB_PATH="/usr/lib/python3.8"
