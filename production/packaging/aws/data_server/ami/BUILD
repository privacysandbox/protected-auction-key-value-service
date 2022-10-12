load("@rules_pkg//pkg:zip.bzl", "pkg_zip")

# nitro artifacts for the AMI
pkg_zip(
    name = "nitro_artifacts",
    srcs = [
        "//components/aws:sqs_lambda.tar",
        "//components/aws/proxy/source:proxy",
    ],
)

genrule(
    name = "copy_to_dist",
    srcs = [
        ":nitro_artifacts",
        "//components/aws:push_sqs",
    ],
    outs = ["copy_to_dist.bin"],
    cmd_bash = """cat << EOF > '$@'
mkdir -p dist/aws
cp $(location :nitro_artifacts) $(location //components/aws:push_sqs) dist/aws
chmod 755 dist/aws/push_sqs
EOF""",
    executable = True,
    local = True,
    message = "copying AMI artifacts to dist/aws directory",
)
