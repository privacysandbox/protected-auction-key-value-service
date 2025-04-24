# Copyright 2025 Google LLC
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

# Based on aws-nitro-enclaves-cli tools/Dockerfile:
#   https://github.com/aws/aws-nitro-enclaves-cli/blob/ff5d3c05a6d0e3fbf31f50594448b42d466c54cf/tools/Dockerfile)

#
# stage: bootstrap-build
#
FROM nixos/nix:2.21.4 AS bootstrap-build
ARG TARGET=all

ADD https://github.com/aws/aws-nitro-enclaves-sdk-bootstrap/archive/refs/tags/v6.6.79.tar.gz /tmp/sdk.tgz
WORKDIR /build
RUN tar --extract --strip-components=1 --file /tmp/sdk.tgz \
  && rm -f /tmp/sdk.tgz \
  && nix-build -A ${TARGET}
RUN ARCH="$(uname -m)" \
  && mkdir -p /build/result/blobs \
  && mv /build/result/${ARCH}/* /build/result/blobs

#
# stage: bootstrap-artifacts
#
FROM scratch AS bootstrap-artifacts
COPY --from=bootstrap-build /build/result/blobs/* /blobs/

#
# stage: rust-base
#
# ubuntu 22.04
FROM ubuntu@sha256:ed1544e454989078f5dec1bfdabd8c5cc9c48e0705d07b678ab6ae3fb61952d2 AS rust-base
ARG TARGETARCH

SHELL ["/bin/bash", "-c"]
ENV DEBIAN_FRONTEND=noninteractive
RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
  --mount=target=/var/cache/apt,type=cache,sharing=locked \
  rm -f /etc/apt/apt.conf.d/docker-clean \
  && apt-get update \
  && apt-get --quiet install -y --no-install-recommends \
  apt-transport-https="2.4.*" \
  ca-certificates \
  curl="7.81.*" \
  "$(test "${TARGETARCH}" = amd64 && echo libc6-dev-i386="2.35-*" || true)" \
  gnupg2="2.2.*" \
  libssl-dev="3.0.*" \
  lsb-release="11.1.*" \
  make="4.3-*" \
  musl-tools="1.2.*" \
  pkg-config="0.29.*" \
  software-properties-common="0.99.*" \
  unzip="6.0-*" \
  wget="1.21.*" \
  && curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --default-toolchain none \
  && echo "source /root/.cargo/env" >>/root/.bashrc

#
# stage: build-toolchains
#
FROM rust-base AS build-toolchains
ARG CLANG_VER=18

# Install dependencies to be able to use musl target to produce statically
# linked binaries.
ADD https://apt.llvm.org/llvm.sh /tmp/llvm.sh
RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
  --mount=target=/var/cache/apt,type=cache,sharing=locked \
  rm -f /etc/apt/apt.conf.d/docker-clean \
  && apt-get update \
  && bash /tmp/llvm.sh ${CLANG_VER} \
  && apt-get --quiet install -y --no-install-recommends libc++-${CLANG_VER}-dev libc++abi-${CLANG_VER}-dev libclang-${CLANG_VER}-dev \
  && for p in clang lldb lld ld.lld; do update-alternatives --install /usr/bin/${p} ${p} /usr/bin/${p}-${CLANG_VER} 100; done \
  && rm -f /tmp/llvm.sh

#
# stage: openssl
#
FROM build-toolchains AS build-toolchains-openssl
# Build static version of Openssl.
ENV OPENSSL_VERSION=OpenSSL_1_1_1q
#RUN mkdir /tmp/openssl_src
ADD https://github.com/openssl/openssl/archive/${OPENSSL_VERSION}.zip /tmp/openssl_src/openssl.zip
RUN unzip /tmp/openssl_src/openssl.zip -d /tmp/openssl_src
RUN cd /tmp/openssl_src/openssl-${OPENSSL_VERSION} &&  \
	CC=musl-gcc CFLAGS=-fPIC ./Configure --prefix=/musl_openssl --openssldir=/musl_openssl no-shared no-engine no-afalgeng linux-$(uname -m) -DOPENSSL_NO_SECURE_MEMORY no-tests \
  && make -j$(nproc) \
  && make install_sw

#
# stage: rust-toolchains
#
FROM build-toolchains-openssl AS rust-toolchains
ARG RUST_VERSION=1.71.1 \
    CARGO_ABOUT_VERSION=0.6.6 \
    CARGO_AUDIT_VERSION=0.17.6
RUN --mount=type=cache,target=/usr/local/cargo/registry \
    --mount=type=cache,target=/home/root/app/target \
    source /root/.cargo/env \
    && ARCH="$(uname -m)" \
    && rustup toolchain install ${RUST_VERSION}-${ARCH}-unknown-linux-gnu \
    && rustup default ${RUST_VERSION}-${ARCH}-unknown-linux-gnu \
    && rustup target add --toolchain ${RUST_VERSION} ${ARCH}-unknown-linux-musl \
    && rustup toolchain install stable-${ARCH}-unknown-linux-gnu \
    && cargo +stable install cargo-about --version ${CARGO_ABOUT_VERSION} --locked \
    && cargo install cargo-audit --version ${CARGO_AUDIT_VERSION} --locked

#
# stage: nitro-cli-build
#
FROM rust-toolchains AS nitro-cli-build
ARG TOOLCHAIN_PREFIX=unknown-linux-musl
ADD https://github.com/aws/aws-nitro-enclaves-cli/archive/refs/tags/v1.4.0.tar.gz /tmp/nitro-cli.tgz
WORKDIR /nitro_src
RUN tar --extract --strip-components=1 --file /tmp/nitro-cli.tgz \
  && rm -f /tmp/nitro-cli.tgz
RUN --mount=type=cache,target=/usr/local/cargo/registry \
    --mount=type=cache,target=/home/root/app/target \
    source /root/.cargo/env \
  && ARCH="$(uname -m)" \
  && OPENSSL_STATIC=yes OPENSSL_DIR=/musl_openssl/ \
  cargo build \
  --release \
  --manifest-path=Cargo.toml \
  --target=${ARCH}-${TOOLCHAIN_PREFIX} \
  --target-dir=build \
  && mv /nitro_src/build/${ARCH}-*/release/nitro-cli /nitro_src/build/

#
# stage: nitro-cli-artifacts
#
FROM scratch AS nitro-cli-artifacts
COPY --from=nitro-cli-build /nitro_src/build/nitro-cli /

#
# stage: nitro-cli-runtime
#
# Install docker for nitro-cli build-enclave runs
# ubuntu 22.04
FROM ubuntu@sha256:ed1544e454989078f5dec1bfdabd8c5cc9c48e0705d07b678ab6ae3fb61952d2 AS nitro-cli-runtime
ARG TARGETARCH
# Install docker for nitro-cli build-enclave runs
RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
  --mount=target=/var/cache/apt,type=cache,sharing=locked \
  rm -f /etc/apt/apt.conf.d/docker-clean \
  && apt-get update \
  && apt-get --quiet install -y --no-install-recommends \
  apt-transport-https="2.4.*" \
  ca-certificates \
  curl="7.81.*" \
  gnupg2="2.2.*" \
  jq="1.6-*" \
  software-properties-common="0.99.*" \
  && curl -fsSL https://download.docker.com/linux/$(. /etc/os-release; echo "$ID")/gpg >/tmp/dkey \
  && apt-key add /tmp/dkey \
  && add-apt-repository \
        "deb [arch=${TARGETARCH}] https://download.docker.com/linux/$(. /etc/os-release; echo "$ID") \
        $(lsb_release -cs) \
        stable" \
  && apt-get --quiet install -y --no-install-recommends docker-ce

#
# stage: nitro-cli
#
# ubuntu 22.04
FROM nitro-cli-runtime AS nitro-cli
COPY --from=nitro-cli-artifacts /nitro-cli /usr/bin/
COPY --from=bootstrap-artifacts /blobs/* /usr/share/nitro_enclaves/blobs/
RUN mkdir -p /var/log/nitro_enclaves
ENTRYPOINT ["/usr/bin/nitro-cli"]
