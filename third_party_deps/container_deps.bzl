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
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@rules_oci//oci:pull.bzl", "oci_pull")
load("//builders/bazel:container_deps.bzl", common_container_deps = "container_deps")

def container_deps():
    common_container_deps()

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
        # Used for deploying Envoy locally
        "envoy-distroless": {
            "arch_hashes": {
                # v1.24.1
                "amd64": "9f5d0d7c817c588cd4bd6ef4508ad544ef19cef6d217aa894315790da7662ba7",
                "arm64": "94c9e77eaa85893daaf95a20fdd5dfb3141250a8c5d707d789265ee3abe49a1e",
            },
            "registry": "docker.io",
            "repository": "envoyproxy/envoy-distroless",
        },
    }
    [
        oci_pull(
            name = "{}-{}".format(img_name, arch),
            digest = "sha256:" + hash,
            image = "{}/{}".format(image["registry"], image["repository"]),
        )
        for img_name, image in images.items()
        for arch, hash in image["arch_hashes"].items()
    ]

    # Used for deploying Envoy on GCP
    # version 1.24.1, same version as the one used for AWS
    maybe(
        http_file,
        name = "envoy_binary",
        downloaded_file_path = "envoy",
        executable = True,
        url = "https://github.com/envoyproxy/envoy/releases/download/v1.24.1/envoy-1.24.1-linux-x86_64",
        sha256 = "b4984647923c1506300995830f51b03008b18977e72326dc33cd414e21f5036e",
    )
