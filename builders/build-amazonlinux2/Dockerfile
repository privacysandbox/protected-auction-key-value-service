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

# build command:
# `tar --create --dereference --gzip --directory=builders/build-amazonlinux2 . | docker build - --tag privacysandbox/builders/build-amazonlinux2:bazel_build_image`

FROM amazonlinux:2.0.20220912.1

COPY /install_apps install_golang_apps install_go.sh generate_system_bazelrc .bazelversion /scripts/

RUN  \
  /scripts/install_apps && \
  /scripts/generate_system_bazelrc --user-root build_kvserver_amazonlinux2 && \
  /scripts/install_golang_apps && \
  rm -rf /scripts

ENV WORKSPACE=/src/workspace \
    PATH="${PATH}:/usr/local/go/bin"
