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
        # Used for deploying Envoy locally for testing
        "envoy-distroless": {
            "arch_hashes": {
                # v1.23.1
                "amd64": "e2c642bc6949cb3053810ca14524324d7daf884a0046d7173e46e2b003144f1d",
                "arm64": "7763f6325882122afb1beb6ba0a047bed318368f9656fd9c1df675f3d89f1dbe",
            },
            "registry": "docker.io",
            "repository": "envoyproxy/envoy-distroless",
        },
        "runtime-debian": {
            "arch_hashes": {
                # cc-debian11:debug-nonroot
                "amd64": "7caec0c1274f808d29492012a5c3f57331c7f44d5e9e83acf5819eb2e3ae14dc",
                "arm64": "f17be941beeaa468ef03fc986cd525fe61e7550affc12fbd4160ec9e1dac9c1d",
            },
            "registry": "gcr.io",
            "repository": "distroless/cc-debian11",
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
