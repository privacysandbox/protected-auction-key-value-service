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
# limitations under the License.jjj

# build command:
# `tar --create --dereference --gzip --directory=builders/build-debian . | docker build - --tag privacysandbox/builders/build-debian:bazel_build_image`

FROM ubuntu:20.04

COPY install_* generate_system_bazelrc .pre-commit-config.yaml .bazelversion /scripts/
COPY gitconfig /etc

ENV PRE_COMMIT_HOME=/var/cache/pre-commit
ENV PRE_COMMIT_TOOL=/usr/pre-commit-venv/bin/pre-commit

RUN  \
  chmod 644 /etc/gitconfig && \
  /scripts/install_apt_apps --with-golang --with-clang --with-docker --with-packer && \
  /scripts/generate_system_bazelrc && \
  /scripts/install_golang_apps --with-bazel --bazel-symlink-path /usr/bin && \
  /scripts/install_precommit_cache && \
  rm -rf /scripts

ENV PATH="${PATH}:/usr/local/go/bin"

# go version; clang --version; bazel version; packer --version
