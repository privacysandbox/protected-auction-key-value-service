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

FROM golang:1.19.3-alpine3.16 AS golang-1.19
FROM node:18.9.0-alpine3.16

COPY --from=golang-1.19 /usr/local/go/ /usr/local/go/
ENV PATH="${PATH}:/usr/local/go/bin"

COPY install_release_apps /scripts/
COPY gitconfig /etc

ARG TARGETARCH
ENV BUILD_ARCH="${TARGETARCH}" \
    WORKSPACE=/src/workspace

RUN  \
  chmod 644 /etc/gitconfig && \
  /scripts/install_release_apps && \
  rm -rf /scripts

ENTRYPOINT ["/bin/sh"]
