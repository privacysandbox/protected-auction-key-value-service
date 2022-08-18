package(default_visibility = ["//:__subpackages__"])

load("@com_github_bazelbuild_buildtools//buildifier:def.bzl", "buildifier", "buildifier_test")
load("@bazel_skylib//rules:common_settings.bzl", "string_flag")
load("@pip_deps//:requirements.bzl", "requirement")
load("@rules_python//python:defs.bzl", "py_binary")

buildifier(
    name = "buildifier",
)

buildifier_test(
    name = "buildifier_test",
    srcs = glob(
        [
            "*.BUILD",
            "*.bzl",
            "BUILD",
            "WORKSPACE",
        ],
    ),
    verbose = True,
)

# Config settings to determine which platform the system will be built to run on
# Example:
# bazel build components/... --//:platform=aws
string_flag(
    name = "platform",
    build_setting_default = "aws",
    values = [
        "aws",
        "local",
    ],
)

config_setting(
    name = "aws_platform",
    flag_values = {
        ":platform": "aws",
    },
    visibility = ["//visibility:private"],
)

config_setting(
    name = "local_platform",
    flag_values = {
        ":platform": "local",
    },
    visibility = ["//visibility:private"],
)

string_flag(
    name = "parameters",
    build_setting_default = "aws",
    values = [
        "aws",
        "local",
    ],
)

config_setting(
    name = "aws_parameters",
    flag_values = {
        ":parameters": "aws",
    },
    visibility = ["//visibility:private"],
)

config_setting(
    name = "local_parameters",
    flag_values = {
        ":parameters": "local",
    },
    visibility = ["//visibility:private"],
)

exports_files(
    [".bazelversion"],
)

py_binary(
    name = "pre-commit",
    srcs = [
        requirement("pre-commit"),
    ],
    args = [
        "run",
        "--config",
        "$(location .pre-commit-config.yaml)",
        "--all-files",
    ],
    data = [".pre-commit-config.yaml"],
    main = "main.py",
    deps = [
        requirement("pre-commit"),
    ],
)

py_binary(
    name = "pre-commit-autoupdate",
    srcs = [
        requirement("pre-commit"),
    ],
    args = [
        "autoupdate",
    ],
    main = "main.py",
    deps = [
        requirement("pre-commit"),
    ],
)

genrule(
    name = "update-deps",
    outs = ["update_deps.bin"],
    cmd = """cat << EOF > '$@'
bazel run //third_party/py_requirements:base.update
bazel run //:pre-commit-autoupdate
EOF""",
    executable = True,
    local = True,
)

genrule(
    name = "precommit-hooks",
    outs = ["run_precommit_hooks.bin"],
    cmd = """cat << EOF > '$@'
bazel run //:buildifier
bazel run //:pre-commit
EOF""",
    executable = True,
    local = True,
)
