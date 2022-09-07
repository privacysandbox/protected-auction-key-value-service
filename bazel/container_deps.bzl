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

load("@io_bazel_rules_docker//container:container.bzl", "container_pull")

def container_deps():
    container_pull(
        name = "debian_slim_base_image",
        digest = "sha256:845224f88b8ee580ed28780ce9d9415e8157a3d911be2a7429de9e3f98c3d8aa",
        registry = "docker.io",
        repository = "library/debian",
        tag = "stable-slim",
    )

    container_pull(
        name = "amazonlinux2_image",
        digest = "sha256:c9ce7208912b7897c9a4cb273f20bbfd54fd745d1dd64f5e625fff6778469e69",
        registry = "docker.io",
        repository = "library/amazonlinux",
        tag = "2",
    )

    container_pull(
        name = "golang_debian_bullseye_image",
        digest = "sha256:5417b4917fa7ed3ad2678a3ce6378a00c95bfd430c2ffa39936fce55130b5f2c",
        registry = "docker.io",
        repository = "library/golang",
        tag = "1.18-bullseye",
    )

    container_pull(
        name = "gcr_cloud_builders_npm",
        digest = "sha256:751d41f241ab3eb3ae7f331c6668d44cacabd37df1fda2aaf7f141c0d26de3cc",
        registry = "gcr.io",
        repository = "cloud-builders/npm",
        tag = "node-14.10.1",
    )

    container_pull(
        name = "aws-lambda-python",
        digest = "sha256:4dddb01519f7411275c6b7df6db68a172646510d7496d063e1535bcd5e1883aa",
        registry = "docker.io",
        repository = "amazon/aws-lambda-python",
        tag = "3.9",
    )

    container_pull(
        name = "envoy-distroless",
        digest = "sha256:541d31419b95e3c62d8cc0967db9cdb4ad2782cc08faa6f15f04c081200e324a",
        registry = "docker.io",
        repository = "envoyproxy/envoy-distroless",
        tag = "v1.22.2",
    )
