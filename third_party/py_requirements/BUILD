load("@rules_python//python:pip.bzl", "compile_pip_requirements")

compile_pip_requirements(
    name = "base",
    extra_args = [
        "--allow-unsafe",
    ],
    local = True,
    requirements_in = "base.in",
    requirements_txt = "base.txt",
    tags = [
        "no-cache",
    ],
    visibility = ["//visibility:public"],
)

exports_files([
    "base.in",
])
