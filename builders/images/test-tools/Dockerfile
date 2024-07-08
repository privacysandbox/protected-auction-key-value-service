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

FROM alpine:3.19 as slowhttptest_builder
# hadolint ignore=DL3018
RUN apk add --no-cache autoconf automake build-base git openssl-dev
WORKDIR /build
ADD https://github.com/shekyan/slowhttptest/archive/refs/tags/v1.9.0.tar.gz /build/src.tar.gz
RUN tar xz --strip-components 1 -f src.tar.gz && ./configure && make

FROM alpine:3.19 as wrk_builder
ARG TARGETARCH
ENV BUILD_ARCH="${TARGETARCH}"
COPY build_wrk /build/
WORKDIR /build
ADD https://github.com/giltene/wrk2/archive/44a94c17d8e6a0bac8559b53da76848e430cb7a7.tar.gz /build/src.tar.gz
RUN /build/build_wrk

FROM golang:1.22-alpine3.19 AS golang
ENV GOBIN=/usr/local/go/bin
COPY build_golang_apps /scripts/
RUN /scripts/build_golang_apps

FROM fullstorydev/grpcurl:v1.9.1-alpine AS grpcurl

FROM alpine:3.19
COPY --from=golang /usr/local/go/bin/* /usr/local/bin/
COPY --from=grpcurl /bin/grpcurl /usr/local/bin/
ARG TARGETARCH
ENV BUILD_ARCH="${TARGETARCH}" \
    PATH="${PATH}:/usr/local/go/bin" \
    GOBIN=/usr/local/go/bin
COPY install_apps /scripts/
RUN /scripts/install_apps
COPY --from=slowhttptest_builder /build/src/slowhttptest /usr/bin/
COPY --from=wrk_builder /build/wrk /usr/bin/wrk2
