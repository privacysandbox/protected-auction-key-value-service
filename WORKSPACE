workspace(name = "google_privacysandbox_kv_server")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

local_repository(
    name = "google_privacysandbox_functionaltest_system",
    path = "testing/functionaltest-system",
)

load("//builders/bazel:deps.bzl", "python_deps")

python_deps("//builders/bazel")

http_archive(
    name = "google_privacysandbox_servers_common",
    # commit 473a15815d94cb29a928e14f03c506872500e352 2023-10-06
    sha256 = "2525b6d76609ca1b10a04ba99bcdc771b027ee9e84f2ad8049c0e1c3e0eff653",
    strip_prefix = "data-plane-shared-libraries-473a15815d94cb29a928e14f03c506872500e352",
    urls = [
        "https://github.com/privacysandbox/data-plane-shared-libraries/archive/473a15815d94cb29a928e14f03c506872500e352.tar.gz",
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
    "//third_party:cpp_repositories.bzl",
    "cpp_repositories",
    "emscripten_repositories",
)

cpp_repositories()

EMSCRIPTEN_VER = emscripten_repositories()

load("//third_party:container_deps.bzl", "container_deps")

container_deps()

load("//third_party:emscripten_deps1.bzl", "emscripten_deps1")

emscripten_deps1()

load("//third_party:emscripten_deps2.bzl", "emscripten_deps2")

emscripten_deps2(EMSCRIPTEN_VER)

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

load("//third_party:python_deps.bzl", "python_repositories")

python_repositories()

# Load the starlark macro, which will define your dependencies.
load("@word2vec//:requirements.bzl", "install_deps")

# Call it to define repos for your requirements.
install_deps()

load("//third_party:rules_closure_repositories.bzl", "rules_closure_repositories")

rules_closure_repositories()

load("//third_party:rules_closure_deps.bzl", "rules_closure_deps")

rules_closure_deps()
