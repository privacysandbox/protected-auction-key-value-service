package(
    default_visibility = ["//visibility:public"],
)

alias(
    name = "grpc_cli",
    actual = "@com_github_grpc_grpc//test/cpp/util:grpc_cli",
)

genrule(
    name = "copy_to_dist",
    srcs = [
        ":grpc_cli",
    ],
    outs = ["copy_to_dist.bin"],
    cmd_bash = """cat << EOF > '$@'
mkdir -p dist/aws
cp $(execpath :grpc_cli) dist/debian
chmod 755 dist/debian/grpc_cli
EOF""",
    executable = True,
    local = True,
    message = "copying grpc_cli to dist/debian/ directory",
)
