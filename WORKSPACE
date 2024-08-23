workspace(name = "google_privacysandbox_kv_server")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

local_repository(
    name = "google_privacysandbox_functionaltest_system",
    path = "testing/functionaltest-system",
)

load("//builders/bazel:deps.bzl", "python_deps", "python_register_toolchains")

python_deps()

python_register_toolchains("//builders/bazel")

http_archive(
    name = "google_privacysandbox_servers_common",
    # commit 9c5c93e 2024-08-23
    sha256 = "915e837fcfeab97341ccb628b2bd7481559921909aa5f7271b95160b4f724b19",
    strip_prefix = "data-plane-shared-libraries-9c5c93ef58d3fb174f785c0cdc0802c7e72bf4fb",
    urls = [
        "https://github.com/privacysandbox/data-plane-shared-libraries/archive/9c5c93ef58d3fb174f785c0cdc0802c7e72bf4fb.zip",
    ],
)

load(
    "@google_privacysandbox_servers_common//third_party:cpp_deps.bzl",
    data_plane_shared_deps_cpp = "cpp_dependencies",
)

data_plane_shared_deps_cpp()

load("@google_privacysandbox_servers_common//third_party:deps1.bzl", data_plane_shared_deps1 = "deps1")

data_plane_shared_deps1()

load("@google_privacysandbox_servers_common//third_party:deps2.bzl", data_plane_shared_deps2 = "deps2")

data_plane_shared_deps2(go_toolchains_version = "1.19.9")

load("@google_privacysandbox_servers_common//third_party:deps3.bzl", data_plane_shared_deps3 = "deps3")

data_plane_shared_deps3()

load("@google_privacysandbox_servers_common//third_party:deps4.bzl", data_plane_shared_deps4 = "deps4")

data_plane_shared_deps4()

load(
    "//third_party_deps:cpp_repositories.bzl",
    "cpp_repositories",
)

cpp_repositories()

http_archive(
    name = "io_bazel_rules_docker",
    sha256 = "b1e80761a8a8243d03ebca8845e9cc1ba6c82ce7c5179ce2b295cd36f7e394bf",
    urls = ["https://github.com/bazelbuild/rules_docker/releases/download/v0.25.0/rules_docker-v0.25.0.tar.gz"],
)

load("@io_bazel_rules_docker//repositories:repositories.bzl", container_repositories = "repositories")

container_repositories()

load("@io_bazel_rules_docker//repositories:deps.bzl", io_bazel_rules_docker_deps = "deps")

io_bazel_rules_docker_deps()

load("//third_party_deps:container_deps.bzl", "container_deps")

container_deps()

load("@io_bazel_rules_docker//go:image.bzl", go_image_repos = "repositories")

go_image_repos()

# googleapis
http_archive(
    name = "com_google_googleapis",  # master branch from 26.04.2022
    sha256 = "3cbe0fcdad3ad7b2fdc58b0f297190c1e05b47b7c10fd14e3364501baa14177e",
    strip_prefix = "googleapis-f91b6cf82e929280f6562f6110957c654bd9e2e6",
    urls = ["https://github.com/googleapis/googleapis/archive/f91b6cf82e929280f6562f6110957c654bd9e2e6.tar.gz"],
)

http_archive(
    name = "distributed_point_functions",
    sha256 = "19cd27b36b0ceba683c02fc6c80e61339397afc3385b91d54210c5db0a254ef8",
    strip_prefix = "distributed_point_functions-45da5f54836c38b73a1392e846c9db999c548711",
    urls = ["https://github.com/google/distributed_point_functions/archive/45da5f54836c38b73a1392e846c9db999c548711.tar.gz"],
)

http_archive(
    name = "libcbor",
    build_file = "//third_party_deps:libcbor.BUILD",
    patch_args = ["-p1"],
    patches = ["//third_party_deps:libcbor.patch"],
    sha256 = "9fec8ce3071d5c7da8cda397fab5f0a17a60ca6cbaba6503a09a47056a53a4d7",
    strip_prefix = "libcbor-0.10.2/src",
    urls = ["https://github.com/PJK/libcbor/archive/refs/tags/v0.10.2.zip"],
)

# Dependencies for Flex/Bison build rules
http_archive(
    name = "rules_m4",
    sha256 = "10ce41f150ccfbfddc9d2394ee680eb984dc8a3dfea613afd013cfb22ea7445c",
    urls = ["https://github.com/jmillikin/rules_m4/releases/download/v0.2.3/rules_m4-v0.2.3.tar.xz"],
)

load("@rules_m4//m4:m4.bzl", "m4_register_toolchains")

m4_register_toolchains(version = "1.4.18")

http_archive(
    name = "rules_bison",
    sha256 = "2279183430e438b2dc77cacd7b1dbb63438971b2411406570f1ddd920b7c9145",
    urls = ["https://github.com/jmillikin/rules_bison/releases/download/v0.2.2/rules_bison-v0.2.2.tar.xz"],
)

load("@rules_bison//bison:bison.bzl", "bison_register_toolchains")

bison_register_toolchains(version = "3.3.2")

http_archive(
    name = "rules_flex",
    sha256 = "8929fedc40909d19a4b42548d0785f796c7677dcef8b5d1600b415e5a4a7749f",
    urls = ["https://github.com/jmillikin/rules_flex/releases/download/v0.2.1/rules_flex-v0.2.1.tar.xz"],
)

load("@rules_flex//flex:flex.bzl", "flex_register_toolchains")

flex_register_toolchains(version = "2.6.4")

load("//third_party_deps:python_deps.bzl", "python_repositories")

python_repositories()

# Load the starlark macro, which will define your dependencies.
load("@latency_benchmark//:requirements.bzl", latency_benchmark_install_deps = "install_deps")
load("@word2vec//:requirements.bzl", word2vec_install_deps = "install_deps")

# Call it to define repos for your requirements.
latency_benchmark_install_deps()

word2vec_install_deps()

# Use nogo to run `go vet` with bazel
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains(nogo = "@//:kv_nogo")

# setup container_structure_test
http_archive(
    name = "container_structure_test",
    sha256 = "2da13da4c4fec9d4627d4084b122be0f4d118bd02dfa52857ff118fde88e4faa",
    strip_prefix = "container-structure-test-1.16.0",
    urls = ["https://github.com/GoogleContainerTools/container-structure-test/archive/v1.16.0.zip"],
)

load("@container_structure_test//:repositories.bzl", "container_structure_test_register_toolchain")

container_structure_test_register_toolchain(name = "cst")
