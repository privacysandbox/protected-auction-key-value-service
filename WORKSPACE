load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

local_repository(
    name = "google_privacysandbox_functionaltest_system",
    path = "testing/functionaltest-system",
)

http_archive(
    name = "boringssl",
    sha256 = "0cd64ecff9e5f757988b84b7685e968775de08ea9157656d0b9fee0fa62d67ec",
    strip_prefix = "boringssl-c2837229f381f5fcd8894f0cca792a94b557ac52",
    urls = ["https://github.com/google/boringssl/archive/c2837229f381f5fcd8894f0cca792a94b557ac52.tar.gz"],
)

http_archive(
    name = "bazel_skylib",
    sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
        "https://github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
    ],
)

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")

bazel_skylib_workspace()

http_archive(
    name = "com_google_protobuf",
    sha256 = "e51cc8fc496f893e2a48beb417730ab6cbcb251142ad8b2cd1951faa5c76fe3d",  # Last updated 2022-09-29
    strip_prefix = "protobuf-3.20.3",
    urls = ["https://github.com/protocolbuffers/protobuf/releases/download/v3.20.3/protobuf-cpp-3.20.3.tar.gz"],
)

http_archive(
    name = "io_bazel_rules_go",
    sha256 = "16e9fca53ed6bd4ff4ad76facc9b7b651a89db1689a2877d6fd7b82aa824e366",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.34.0/rules_go-v0.34.0.zip",
        "https://github.com/bazelbuild/rules_go/releases/download/v0.34.0/rules_go-v0.34.0.zip",
    ],
)

load("//builders/bazel:deps.bzl", "python_deps")

python_deps("//builders/bazel")

http_archive(
    name = "bazel_gazelle",
    sha256 = "448e37e0dbf61d6fa8f00aaa12d191745e14f07c31cabfa731f0c8e8a4f41b97",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.28.0/bazel-gazelle-v0.28.0.tar.gz",
        "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.28.0/bazel-gazelle-v0.28.0.tar.gz",
    ],
)

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")
load("@io_bazel_rules_go//go:deps.bzl", "go_rules_dependencies")

go_rules_dependencies()

### go_register_toolchains will be called by grpc_extra_deps
# go_register_toolchains(go_version = "1.18")
### gRPC
http_archive(
    name = "com_github_grpc_grpc",
    sha256 = "ec125d7fdb77ecc25b01050a0d5d32616594834d3fe163b016768e2ae42a2df6",
    strip_prefix = "grpc-1.52.1",
    urls = [
        "https://github.com/grpc/grpc/archive/v1.52.1.tar.gz",
    ],
)

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")

grpc_extra_deps()

### gazelle deps must be loaded after go toolchains registered
gazelle_dependencies()

http_archive(
    name = "rules_pkg",
    sha256 = "eea0f59c28a9241156a47d7a8e32db9122f3d50b505fae0f33de6ce4d9b61834",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/0.8.0/rules_pkg-0.8.0.tar.gz",
        "https://github.com/bazelbuild/rules_pkg/releases/download/0.8.0/rules_pkg-0.8.0.tar.gz",
    ],
)

load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")

rules_pkg_dependencies()

### rules_buf (https://docs.buf.build/build-systems/bazel)
http_archive(
    name = "rules_buf",
    sha256 = "523a4e06f0746661e092d083757263a249fedca535bd6dd819a8c50de074731a",
    strip_prefix = "rules_buf-0.1.1",
    urls = ["https://github.com/bufbuild/rules_buf/archive/refs/tags/v0.1.1.zip"],
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

load("@io_bazel_rules_docker//repositories:deps.bzl", docker_container_deps = "deps")

docker_container_deps()

load("//third_party:container_deps.bzl", "container_deps")

container_deps()

http_archive(
    name = "google_privacysandbox_servers_common",
    sha256 = "4158164f52e719e5948e5b43bae01b111e5f1cc38e66516d35e37927b0316ff1",
    strip_prefix = "data-plane-shared-libraries-5f9c6fc89e32f944ca208ed4ee1a2c71777cc483",
    urls = [
        "https://github.com/privacysandbox/data-plane-shared-libraries/archive/5f9c6fc89e32f944ca208ed4ee1a2c71777cc483.zip",
    ],
)

load("//third_party:cpp_repositories.bzl", "cpp_repositories")

cpp_repositories()

load("@google_privacysandbox_servers_common//third_party:scp_deps.bzl", "scp_deps")

scp_deps()

load("@google_privacysandbox_servers_common//third_party:scp_deps2.bzl", "scp_deps2")

scp_deps2()

load("@v8_python_deps//:requirements.bzl", install_v8_python_deps = "install_deps")

install_v8_python_deps()

load("//third_party:quiche.bzl", "quiche_dependencies")

quiche_dependencies()

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

# Load OpenTelemetry dependencies after load.
load("//third_party:open_telemetry.bzl", "open_telemetry_dependencies")

open_telemetry_dependencies()

# emscripten

http_archive(
    name = "emsdk",
    sha256 = "d55e3c73fc4f8d1fecb7aabe548de86bdb55080fe6b12ce593d63b8bade54567",
    strip_prefix = "emsdk-3891e7b04bf8cbb3bc62758e9c575ae096a9a518/bazel",
    url = "https://github.com/emscripten-core/emsdk/archive/3891e7b04bf8cbb3bc62758e9c575ae096a9a518.tar.gz",
)

load("@emsdk//:deps.bzl", emsdk_deps = "deps")

emsdk_deps()

load("@emsdk//:emscripten_deps.bzl", emsdk_emscripten_deps = "emscripten_deps")

emsdk_emscripten_deps(emscripten_version = "2.0.31")

# googleapis
http_archive(
    name = "com_google_googleapis",  # master branch from 26.04.2022
    sha256 = "3cbe0fcdad3ad7b2fdc58b0f297190c1e05b47b7c10fd14e3364501baa14177e",
    strip_prefix = "googleapis-f91b6cf82e929280f6562f6110957c654bd9e2e6",
    urls = ["https://github.com/googleapis/googleapis/archive/f91b6cf82e929280f6562f6110957c654bd9e2e6.tar.gz"],
)

load("@io_opentelemetry_cpp//bazel:repository.bzl", "opentelemetry_cpp_deps")

opentelemetry_cpp_deps()

load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

http_archive(
    name = "distributed_point_functions",
    sha256 = "19cd27b36b0ceba683c02fc6c80e61339397afc3385b91d54210c5db0a254ef8",
    strip_prefix = "distributed_point_functions-45da5f54836c38b73a1392e846c9db999c548711",
    urls = ["https://github.com/google/distributed_point_functions/archive/45da5f54836c38b73a1392e846c9db999c548711.tar.gz"],
)
