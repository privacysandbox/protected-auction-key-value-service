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
        "debian-slim": {
            "arch_hashes": {
                # stable-20220912-slim
                "amd64": "9612e64116a7ac412faf2f7ecda5fa88bdcb77148f60dd32a6209580c056a0bd",
                "arm64": "83c4fcf26f4ec147ec3d2cfdb9f2f006e60bd6229aad98602dfbaa6cce11e503",
            },
            "registry": "docker.io",
            "repository": "library/debian",
        },
        "envoy-distroless": {
            "arch_hashes": {
                # v1.23.1
                "amd64": "e2c642bc6949cb3053810ca14524324d7daf884a0046d7173e46e2b003144f1d",
                "arm64": "7763f6325882122afb1beb6ba0a047bed318368f9656fd9c1df675f3d89f1dbe",
            },
            "registry": "docker.io",
            "repository": "envoyproxy/envoy-distroless",
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
