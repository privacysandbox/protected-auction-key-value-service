load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

http_archive(
    name = "bazel_skylib",
    sha256 = "f7be3474d42aae265405a592bb7da8e171919d74c16f082a5457840f06054728",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.2.1/bazel-skylib-1.2.1.tar.gz",
        "https://github.com/bazelbuild/bazel-skylib/releases/download/1.2.1/bazel-skylib-1.2.1.tar.gz",
    ],
)

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")

bazel_skylib_workspace()

### python
http_archive(
    name = "rules_python",
    sha256 = "56dc7569e5dd149e576941bdb67a57e19cd2a7a63cc352b62ac047732008d7e1",
    strip_prefix = "rules_python-0.10.0",
    url = "https://github.com/bazelbuild/rules_python/archive/refs/tags/0.10.0.tar.gz",
)

load("@rules_python//python:repositories.bzl", "python_register_toolchains")

# Build steps will discover the right python via Bazel's toolchain support.
# See versions: https://github.com/bazelbuild/rules_python/blob/0.10.0/python/versions.bzl#L30
python_register_toolchains(
    name = "python3",
    python_version = "3.9",
)

load("@python3//:defs.bzl", py_interpreter = "interpreter")
load("@rules_python//python:pip.bzl", "pip_parse")

# parse local python requirements file (base.txt)
pip_parse(
    name = "pip_deps",
    extra_pip_args = [
        "--quiet",
        "--disable-pip-version-check",
    ],
    python_interpreter_target = py_interpreter,
    requirements_lock = "//third_party/py_requirements:base.txt",
)

# Load the starlark macro which will define your dependencies.
load("@pip_deps//:requirements.bzl", install_pip_deps = "install_deps")

install_pip_deps()

http_archive(
    name = "io_bazel_rules_go",
    sha256 = "16e9fca53ed6bd4ff4ad76facc9b7b651a89db1689a2877d6fd7b82aa824e366",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.34.0/rules_go-v0.34.0.zip",
        "https://github.com/bazelbuild/rules_go/releases/download/v0.34.0/rules_go-v0.34.0.zip",
    ],
)

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

http_archive(
    name = "bazel_gazelle",
    sha256 = "de69a09dc70417580aabf20a28619bb3ef60d038470c7cf8442fafcf627c21cb",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.24.0/bazel-gazelle-v0.24.0.tar.gz",
        "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.24.0/bazel-gazelle-v0.24.0.tar.gz",
    ],
)

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")

go_rules_dependencies()

go_register_toolchains(go_version = "1.18")

gazelle_dependencies()

http_archive(
    name = "rules_pkg",
    sha256 = "8a298e832762eda1830597d64fe7db58178aa84cd5926d76d5b744d6558941c2",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/0.7.0/rules_pkg-0.7.0.tar.gz",
        "https://github.com/bazelbuild/rules_pkg/releases/download/0.7.0/rules_pkg-0.7.0.tar.gz",
    ],
)

load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")

rules_pkg_dependencies()

### Buildifier
# buildifier is written in Go and hence needs rules_go to be built.
http_archive(
    name = "com_github_bazelbuild_buildtools",
    sha256 = "e3bb0dc8b0274ea1aca75f1f8c0c835adbe589708ea89bf698069d0790701ea3",
    strip_prefix = "buildtools-5.1.0",
    urls = [
        "https://github.com/bazelbuild/buildtools/archive/refs/tags/5.1.0.tar.gz",
    ],
)

### gRPC
http_archive(
    name = "com_github_grpc_grpc",
    sha256 = "ec19657a677d49af59aa806ec299c070c882986c9fcc022b1c22c2a3caf01bcd",
    strip_prefix = "grpc-1.45.0",
    urls = ["https://github.com/grpc/grpc/archive/refs/tags/v1.45.0.tar.gz"],
)

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")

grpc_extra_deps()

### googletest
http_archive(
    name = "com_google_googletest",
    strip_prefix = "googletest-e2239ee6043f73722e7aa812a459f54a28552929",
    urls = ["https://github.com/google/googletest/archive/e2239ee6043f73722e7aa812a459f54a28552929.zip"],
)

### rules_buf (https://docs.buf.build/build-systems/bazel)
http_archive(
    name = "rules_buf",
    sha256 = "3fe244c9efa42a41edd83f63dee1b5570a1951a654030658b86bfaea6a268164",
    strip_prefix = "rules_buf-0.1.0",
    urls = ["https://github.com/bufbuild/rules_buf/archive/refs/tags/v0.1.0.zip"],
)

load("@rules_buf//buf:repositories.bzl", "rules_buf_dependencies", "rules_buf_toolchains")

rules_buf_dependencies()

rules_buf_toolchains(version = "v1.7.0")

http_archive(
    name = "io_bazel_rules_docker",
    sha256 = "b1e80761a8a8243d03ebca8845e9cc1ba6c82ce7c5179ce2b295cd36f7e394bf",
    urls = ["https://github.com/bazelbuild/rules_docker/releases/download/v0.25.0/rules_docker-v0.25.0.tar.gz"],
)

load(
    "@io_bazel_rules_docker//repositories:repositories.bzl",
    container_repositories = "repositories",
)

container_repositories()

load("@io_bazel_rules_docker//repositories:deps.bzl", container_deps = "deps")

container_deps()

load("@io_bazel_rules_docker//container:container.bzl", "container_pull")

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

### Abseil
http_archive(
    name = "com_google_absl",
    sha256 = "dcf71b9cba8dc0ca9940c4b316a0c796be8fab42b070bb6b7cab62b48f0e66c4",  # SHARED_ABSL_SHA
    strip_prefix = "abseil-cpp-20211102.0",
    urls = [
        "https://github.com/abseil/abseil-cpp/archive/refs/tags/20211102.0.tar.gz",
    ],
)

http_archive(
    name = "aws-checksums",
    build_file = "//third_party:aws_checksums.BUILD",
    sha256 = "6e6bed6f75cf54006b6bafb01b3b96df19605572131a2260fddaf0e87949ced0",
    strip_prefix = "aws-checksums-0.1.5",
    urls = [
        "https://github.com/awslabs/aws-checksums/archive/v0.1.5.tar.gz",
    ],
)

http_archive(
    name = "aws-c-common",
    build_file = "//third_party:aws_c_common.BUILD",
    sha256 = "01c2a58553a37b3aa5914d9e0bf7bf14507ff4937bc5872a678892ca20fcae1f",
    strip_prefix = "aws-c-common-0.4.29",
    urls = [
        "https://github.com/awslabs/aws-c-common/archive/v0.4.29.tar.gz",
    ],
)

http_archive(
    name = "aws-c-event-stream",
    build_file = "//third_party:aws_c_event_stream.BUILD",
    sha256 = "31d880d1c868d3f3df1e1f4b45e56ac73724a4dc3449d04d47fc0746f6f077b6",
    strip_prefix = "aws-c-event-stream-0.1.4",
    urls = [
        "https://github.com/awslabs/aws-c-event-stream/archive/v0.1.4.tar.gz",
    ],
)

http_archive(
    name = "aws_sdk_cpp",
    build_file = "//third_party:aws_sdk_cpp.BUILD",
    patch_cmds = [
        """sed -i.bak 's/UUID::RandomUUID/Aws::Utils::UUID::RandomUUID/g' aws-cpp-sdk-core/source/client/AWSClient.cpp""",
        # Apply fix in https://github.com/aws/aws-sdk-cpp/commit/9669a1c1d9a96621cd0846679cbe973c648a64b3
        """sed -i.bak 's/Tags\\.entry/Tag/g' aws-cpp-sdk-sqs/source/model/TagQueueRequest.cpp""",
    ],
    sha256 = "758174f9788fed6cc1e266bcecb20bf738bd5ef1c3d646131c9ed15c2d6c5720",
    strip_prefix = "aws-sdk-cpp-1.7.336",
    urls = [
        "https://storage.googleapis.com/mirror.tensorflow.org/github.com/aws/aws-sdk-cpp/archive/1.7.336.tar.gz",
        "https://github.com/aws/aws-sdk-cpp/archive/1.7.336.tar.gz",
    ],
)

http_archive(
    name = "curl",
    build_file = "//third_party:curl.BUILD",
    sha256 = "ff3e80c1ca6a068428726cd7dd19037a47cc538ce58ef61c59587191039b2ca6",
    strip_prefix = "curl-7.49.1",
    urls = [
        "https://mirror.bazel.build/curl.haxx.se/download/curl-7.49.1.tar.gz",
    ],
)

http_archive(
    name = "zlib_archive",
    build_file = "//third_party:zlib.BUILD",
    sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
    strip_prefix = "zlib-1.2.11",
    urls = [
        "https://mirror.bazel.build/zlib.net/zlib-1.2.11.tar.gz",
    ],
)

#riegeli
http_archive(
    name = "com_google_riegeli",
    sha256 = "32f303a9b0b6e07101a7a95a4cc364fb4242f0f7431de5da1a2e0ee61f5924c5",
    strip_prefix = "riegeli-562f26cbb685aae10b7d32e32fb53d2e42a5d8c2",
    url = "https://github.com/google/riegeli/archive/562f26cbb685aae10b7d32e32fb53d2e42a5d8c2.zip",
)

#external deps for riegeli
http_archive(
    name = "org_brotli",
    sha256 = "dbeef368f7c5b779ed34f0a6e17e0a55084fb91c49053f91ea23d0680eb39e00",
    strip_prefix = "brotli-27dd7265403d8e8fed99a854b9c3e1db7d79525f",
    urls = ["https://github.com/google/brotli/archive/27dd7265403d8e8fed99a854b9c3e1db7d79525f.zip"],
)

http_archive(
    name = "net_zstd",
    build_file = "@com_google_riegeli//third_party:net_zstd.BUILD",
    sha256 = "b6c537b53356a3af3ca3e621457751fa9a6ba96daf3aebb3526ae0f610863532",
    strip_prefix = "zstd-1.4.5/lib",
    urls = ["https://github.com/facebook/zstd/archive/v1.4.5.zip"],
)

http_archive(
    name = "snappy",
    build_file = "@com_google_riegeli//third_party:snappy.BUILD",
    sha256 = "e170ce0def2c71d0403f5cda61d6e2743373f9480124bcfcd0fa9b3299d428d9",
    strip_prefix = "snappy-1.1.9",
    urls = ["https://github.com/google/snappy/archive/1.1.9.zip"],
)

http_archive(
    name = "highwayhash",
    build_file = "@com_google_riegeli//third_party:highwayhash.BUILD",
    sha256 = "cf891e024699c82aabce528a024adbe16e529f2b4e57f954455e0bf53efae585",
    strip_prefix = "highwayhash-276dd7b4b6d330e4734b756e97ccfb1b69cc2e12",
    urls = ["https://github.com/google/highwayhash/archive/276dd7b4b6d330e4734b756e97ccfb1b69cc2e12.zip"],
)

### glog
http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf",
    strip_prefix = "gflags-2.2.2",
    urls = ["https://github.com/gflags/gflags/archive/v2.2.2.tar.gz"],
)

http_archive(
    name = "com_github_google_glog",
    sha256 = "21bc744fb7f2fa701ee8db339ded7dce4f975d0d55837a97be7d46e8382dea5a",
    strip_prefix = "glog-0.5.0",
    urls = ["https://github.com/google/glog/archive/v0.5.0.zip"],
)

http_archive(
    name = "com_github_google_flatbuffers",
    sha256 = "ffd68aebdfb300c9e82582ea38bf4aa9ce65c77344c94d5047f3be754cc756ea",
    strip_prefix = "flatbuffers-2.0.0",
    urls = ["https://github.com/google/flatbuffers/archive/v2.0.0.zip"],
)

git_repository(
    name = "com_github_google_rpmpack",
    # Lastest commit in main branch as of 2021-11-29
    commit = "d0ed9b1b61b95992d3c4e83df3e997f3538a7b6c",
    remote = "https://github.com/google/rpmpack.git",
    shallow_since = "1637822718 +0200",
)

load("@com_github_google_rpmpack//:deps.bzl", "rpmpack_dependencies")

rpmpack_dependencies()
