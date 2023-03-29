load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_image",
    "container_layer",
)
load(
    "@rules_pkg//pkg:mappings.bzl",
    "pkg_attributes",
    "pkg_files",
)
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("@rules_proto//proto:defs.bzl", "proto_descriptor_set")

package(
    default_visibility = ["//visibility:private"],
)

alias(
    name = "helloworld_server",
    actual = "@com_github_grpc_grpc//examples/cpp/helloworld:greeter_server",
)

alias(
    name = "keyvaluestore_server",
    actual = "@com_github_grpc_grpc//examples/cpp/keyvaluestore:keyvaluestore_server",
)

alias(
    name = "route_guide_server",
    actual = "@com_github_grpc_grpc//examples/cpp/route_guide:route_guide_server",
)

proto_descriptor_set(
    name = "helloworld_descriptor_set",
    visibility = ["//visibility:public"],
    deps = ["@com_github_grpc_grpc//examples/protos:helloworld_proto"],
)

proto_descriptor_set(
    name = "routeguide_descriptor_set",
    visibility = ["//visibility:public"],
    deps = ["@com_github_grpc_grpc//examples/protos:route_guide_proto"],
)

proto_descriptor_set(
    name = "keyvaluestore_descriptor_set",
    visibility = ["//visibility:public"],
    deps = ["@com_github_grpc_grpc//examples/protos:keyvaluestore_proto"],
)

pkg_files(
    name = "server_executables",
    srcs = [
        ":helloworld_server",
        ":keyvaluestore_server",
        ":route_guide_server",
    ],
    attributes = pkg_attributes(mode = "0755"),
    prefix = "/server/bin",
)

pkg_files(
    name = "server_data",
    srcs = [
        "@com_github_grpc_grpc//examples/cpp/route_guide:route_guide_db.json",
    ],
    attributes = pkg_attributes(mode = "0644"),
    prefix = "/server/data",
)

pkg_tar(
    name = "server_binaries_tar",
    srcs = [
        ":server_data",
        ":server_executables",
    ],
)

container_layer(
    name = "server_binary_layer",
    directory = "/",
    tars = [
        ":server_binaries_tar",
    ],
)

container_image(
    name = "grpc_examples_docker_image",
    architecture = select({
        "@platforms//cpu:arm64": "arm64",
        "@platforms//cpu:x86_64": "amd64",
    }),
    base = select({
        "@platforms//cpu:arm64": "@runtime-debian-arm64//image",
        "@platforms//cpu:x86_64": "@runtime-debian-amd64//image",
    }),
    entrypoint = ["/bin/bash"],
    layers = [
        ":server_binary_layer",
    ],
)
