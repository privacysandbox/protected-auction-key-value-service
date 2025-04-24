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

load("@rules_oci//oci:pull.bzl", "oci_pull")

# All container images must be compatible with the `build-debian` image.
_images = {
    "runtime-debian-debug-nonroot": {
        "arch_hashes": {
            # Feb 28, 2024
            "amd64": "72b9108b17a4ef0398998d45cbc14af2f3270af374fc2aa2c74823c6c7054fac",
            # Feb 28, 2024
            "arm64": "623676598d55f93ff93ea3b9d95f3cb5a379eca66dfcf9b2734f2cc3e5f34666",
        },
        "registry": "gcr.io",
        "repository": "distroless/cc-debian11",
    },
    "runtime-debian-debug-root": {
        # debug build so we can use 'sh'. Root, for gcp coordinators
        # auth to work
        "arch_hashes": {
            # Feb 28, 2024
            "amd64": "d5a2169bc2282598f0cf886a3d301269d0ee5bf7f7392184198dd41d36b70548",
            # Feb 28, 2024
            "arm64": "6449313a9a80b2758f505c81462c492da87f76954d319f2adb55401177798cce",
        },
        "registry": "gcr.io",
        "repository": "distroless/cc-debian11",
    },
    "runtime-debian-nondebug-nonroot": {
        # cc-debian11:nonroot
        # This image contains a minimal Linux, glibc runtime for
        # "mostly-statically compiled" languages like Rust and D.
        # https://github.com/GoogleContainerTools/distroless/blob/main/cc/README.md
        "arch_hashes": {
            # Jul 4, 2024
            "amd64": "5a9e854bab8498a61a66b2cfa4e76e009111d09cb23a353aaa8d926e29a653d9",
            # Jul 4, 2024
            "arm64": "3122cd55375a0a9f32e56a18ccd07572aeed5682421432701a03c335ab79c650",
        },
        "registry": "gcr.io",
        "repository": "distroless/cc-debian11",
    },
    "runtime-debian-nondebug-root": {
        "arch_hashes": {
            # Jul 29, 2024
            "amd64": "72f2f861fdc130b1b083f3f055bbe84b73eec8c8c48f16a435194b41feedc674",
            # Jul 4, 2024
            "arm64": "74bbfb32c5341d7abc6de0379ffe330a9aec2819c5145c4c71917975c93b83e5",
        },
        "registry": "gcr.io",
        "repository": "distroless/cc-debian11",
    },
    # Non-distroless; only for debugging purposes
    "runtime-ubuntu-fulldist-debug-root": {
        # Ubuntu 20.04 ubuntu:focal-20240530
        "arch_hashes": {
            "amd64": "d86db849e59626d94f768c679aba441163c996caf7a3426f44924d0239ffe03f",
            "arm64": "6edb9576e2a2080a42e4e0e9a6bc0bd91a2bf06375f9832d400bf33841d35ece",
        },
        "registry": "docker.io",
        "repository": "library/ubuntu",
    },
}

def container_deps():
    [
        oci_pull(
            name = "{}-{}".format(img_name, arch),
            digest = "sha256:{}".format(hash),
            image = "{}/{}".format(image["registry"], image["repository"]),
        )
        for img_name, image in _images.items()
        for arch, hash in image["arch_hashes"].items()
    ]

def container_image(image, arch):
    img = _images[image]
    return struct(
        digest = "sha256:{}".format(img["arch_hashes"][arch]),
        image = "{}/{}".format(img["registry"], img["repository"]),
        uri = "{}/{}@sha256:{}".format(img["registry"], img["repository"], img["arch_hashes"][arch]),
    )

DISTROLESS_USERS = [
    struct(
        flavor = "nonroot",
        uid = 65532,
        user = "nonroot",
        gid = 65532,
        group = "nonroot",
    ),
    struct(
        flavor = "root",
        uid = 0,
        user = "root",
        gid = 0,
        group = "root",
    ),
]

def get_user(user = "nonroot"):
    """
    Extracts a struct with details from DISTROLESS_USERS based on the given user.

    Args:
      user: The user to search for (e.g., "root" or "nonroot").

    Returns:
      The struct with the matching user, or None if no match is found.
    """
    for entry in DISTROLESS_USERS:
        if entry.user == user:
            return entry
    return None
