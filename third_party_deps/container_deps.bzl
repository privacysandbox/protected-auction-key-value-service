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
load("@google_privacysandbox_servers_common//third_party:container_deps.bzl", common_container_deps = "container_deps")
load("@rules_oci//oci:pull.bzl", "oci_pull")

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
                # v1.33.2
                "amd64": "0d8c8b3fc3250b5f664dd93ac305738f9c4e6ff38fcb8499088cca1b827f787e",
                "arm64": "2b56a5e43c7644cabc3e7c9702addfba09fe64ff7d9fee378bc167f9044f1fd8",
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
        url = "https://github.com/envoyproxy/envoy/releases/download/v1.33.2/envoy-1.33.2-linux-x86_64",
        sha256 = "bd2ff87c5efc95fd7880d710915f573c1aab4d9c55d8da886dcd91df43fad741",
    )
