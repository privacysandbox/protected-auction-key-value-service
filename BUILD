load("@bazel_skylib//rules:common_settings.bzl", "string_flag")

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
    name = "instance",
    build_setting_default = "aws",
    values = [
        "aws",
        "local",
    ],
)

config_setting(
    name = "aws_instance",
    flag_values = {
        ":instance": "aws",
    },
    visibility = ["//visibility:private"],
)

config_setting(
    name = "local_instance",
    flag_values = {
        ":instance": "local",
    },
    visibility = ["//visibility:private"],
)

exports_files(
    [".bazelversion"],
)

genrule(
    name = "update-deps",
    outs = ["update_deps.bin"],
    cmd = """cat << EOF > '$@'
builders/tools/pre-commit autoupdate
EOF""",
    executable = True,
    local = True,
)

genrule(
    name = "precommit-hooks",
    outs = ["run_precommit_hooks.bin"],
    cmd = """cat << EOF > '$@'
builders/tools/pre-commit
EOF""",
    executable = True,
    local = True,
)

genrule(
    name = "buildifier",
    outs = ["run_buildifier.bin"],
    cmd = """cat << EOF > '$@'
builders/tools/pre-commit buildifier
EOF""",
    executable = True,
    local = True,
)

genrule(
    name = "collect-logs",
    outs = ["collect_logs.bin"],
    cmd_bash = """cat << EOF > '$@'
tools/collect-logs
EOF""",
    executable = True,
    local = True,
    message = "copy bazel build and test logs",
)
