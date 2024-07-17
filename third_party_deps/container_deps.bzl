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

# ---- Azure midification for docker registry config ----
# This file has azure specific hack to configure docker registry.
# `@<image name>-amd64-repo-digests-replace-marker@` is used to update repoDigest
# when you replace registry.
# If there is a way to configure repository and repoDigest as a bazel feature,
# that would be better.

load("@io_bazel_rules_docker//container:container.bzl", "container_pull")

def container_deps():
    images = {
        "aws-lambda-python": {
            "arch_hashes": {
                # 3.9.2022.09.27.12
                "amd64": "c5d5475944755926a3caa6aac4486632f5fed11d531e6437b42dd48718725f29",
                "arm64": "d4d6d56eae30e4f74c6aa617043e2018142b2c4aafa02468653799c891bf86cf",
            },
            "registry": "public.ecr.aws",
            "repository": "lambda/python",
        },
        # Used for deploying Envoy locally for testing
        "envoy-distroless": {
            "arch_hashes": {
                # v1.23.1
                "amd64": "e2c642bc6949cb3053810ca14524324d7daf884a0046d7173e46e2b003144f1d",  # @envoy-distroless-amd64-repo-digests-replace-marker@
                "arm64": "7763f6325882122afb1beb6ba0a047bed318368f9656fd9c1df675f3d89f1dbe",
            },
            "registry": "docker.io",
            "repository": "envoyproxy/envoy-distroless",
        },
        "runtime-debian-debug-nonroot": {
            "arch_hashes": {
                # cc-debian11:debug-nonroot
                "amd64": "7caec0c1274f808d29492012a5c3f57331c7f44d5e9e83acf5819eb2e3ae14dc",
                "arm64": "f17be941beeaa468ef03fc986cd525fe61e7550affc12fbd4160ec9e1dac9c1d",
            },
            "registry": "gcr.io",
            "repository": "distroless/cc-debian11",
        },
        "runtime-debian-debug-root": {
            # debug build so we can use 'sh'. Root, for gcp coordinators
            # auth to work
            "arch_hashes": {
                "amd64": "6865ad48467c89c3c3524d4c426f52ad12d9ab7dec31fad31fae69da40eb6445",  # @runtime-cc-debian-amd64-repo-digests-replace-marker@
                "arm64": "3c399c24b13bfef7e38257831b1bb05cbddbbc4d0327df87a21b6fbbb2480bc9",
            },
            "registry": "gcr.io",
            "repository": "distroless/cc-debian11",
        },
        # Non-distroless; only for debugging purposes
        "runtime-ubuntu-fulldist-debug-root": {
            # Ubuntu 20.04
            "arch_hashes": {
                "amd64": "218bb51abbd1864df8be26166f847547b3851a89999ca7bfceb85ca9b5d2e95d",
                "arm64": "a80d11b67ef30474bcccab048020ee25aee659c4caaca70794867deba5d392b6",
            },
            "registry": "docker.io",
            "repository": "library/ubuntu",
        },
    }
    [
        container_pull(
            name = img_name + "-" + arch,
            digest = "sha256:" + hash,
            registry = image["registry"],
            repository = image["repository"],
        )
        for img_name, image in images.items()
        for arch, hash in image["arch_hashes"].items()
    ]
