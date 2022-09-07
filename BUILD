load("@bazel_skylib//rules:common_settings.bzl", "string_flag")
load("@bazel_tools//tools/python:toolchain.bzl", "py_runtime_pair")
load("@rules_python//python:defs.bzl", "py_runtime")

package(default_visibility = ["//:__subpackages__"])

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

# define the python3 runtime.
# this path must exist in the bazel build environment ie. the build container images must install python3.8 in this path
PY3_PATH = "/usr/bin/python3.8"

py_runtime(
    name = "py_runtime",
    interpreter_path = PY3_PATH,
    python_version = "PY3",
    visibility = ["//visibility:public"],
)

py_runtime_pair(
    name = "py_runtime_pair",
    py2_runtime = None,
    py3_runtime = ":py_runtime",
)

toolchain(
    name = "py_toolchain",
    toolchain = ":py_runtime_pair",
    toolchain_type = "@bazel_tools//tools/python:toolchain_type",
)

genrule(
    name = "update-deps",
    outs = ["update_deps.bin"],
    cmd = """cat << EOF > '$@'
tools/pre-commit autoupdate
EOF""",
    executable = True,
    local = True,
)

genrule(
    name = "precommit-hooks",
    outs = ["run_precommit_hooks.bin"],
    cmd = """cat << EOF > '$@'
tools/pre-commit
EOF""",
    executable = True,
    local = True,
)
