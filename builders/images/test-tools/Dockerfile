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

FROM alpine:3.16 as builder
# hadolint ignore=DL3018
RUN apk add --no-cache build-base git openssl-dev autoconf automake
WORKDIR /build
ADD https://github.com/shekyan/slowhttptest/archive/refs/tags/v1.9.0.tar.gz /build
RUN tar xz --strip-components 1 -f v1.9.0.tar.gz && ls -l && ./configure && make

FROM golang:1.19.4-alpine3.17 AS golang
ENV BUILD_ARCH="${TARGETARCH}" \
    GOBIN=/usr/local/go/bin
COPY build_golang_apps /scripts/
RUN /scripts/build_golang_apps

FROM fullstorydev/grpcurl:v1.8.7 AS grpcurl
FROM alpine:3.17.2

COPY --from=golang /usr/local/go/bin/* /usr/local/bin/
COPY --from=grpcurl /bin/grpcurl /usr/local/bin/

ARG TARGETARCH
ENV BUILD_ARCH="${TARGETARCH}" \
    PATH="${PATH}:/usr/local/go/bin" \
    GOBIN=/usr/local/go/bin

COPY install_apps /scripts/

RUN /scripts/install_apps
COPY --from=builder /build/src/slowhttptest /usr/bin/
