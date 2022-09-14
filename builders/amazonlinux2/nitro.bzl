# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Rule for building an AWS Nitro Enclave image.

This implementation borrows heavily from the container_run_and_extract rule
in the rules_docker repo:
https://github.com/bazelbuild/rules_docker/blob/master/docker/util/run.bzl

nitro-cli is a command-line tool for AWS Nitro Enclaves. Refer to the reference
for the nitro-cli build-enclave:
https://docs.aws.amazon.com/enclaves/latest/user/cmd-nitro-build-enclave.html
"""

load("@io_bazel_rules_docker//skylib:docker.bzl", "docker_path")

_nitro_enclave_image_attrs = {
    "aws_nitro_image": attr.label(
        executable = True,
        doc = "The image to run the commands in. It must contain the AWS CLI tools.",
        mandatory = True,
        allow_single_file = True,
        cfg = "target",
    ),
    "docker_run_flags": attr.string_list(
        doc = "Extra flags to pass to the docker run command.",
        mandatory = False,
    ),
    "extra_deps": attr.label_list(
        doc = "Extra dependencies to be passed as inputs",
        mandatory = False,
        allow_files = True,
    ),
    "source_image": attr.label(
        executable = True,
        doc = "The container image to convert into a Nitro enclave image (EIF).",
        mandatory = True,
        allow_single_file = True,
        cfg = "target",
    ),
    "target_eif_basename": attr.string(
        doc = "Base filename for the generated EIF image.",
        mandatory = True,
    ),
    "_extract_image_id": attr.label(
        default = Label("@io_bazel_rules_docker//contrib:extract_image_id"),
        cfg = "exec",
        executable = True,
        allow_files = True,
    ),
    "_extract_tpl": attr.label(
        default = Label("//builders/amazonlinux2:generate_nitro.sh.tpl"),
        allow_single_file = True,
    ),
}

def _nitro_enclave_image_impl(
        ctx,
        name = "",
        aws_nitro_image = None,
        source_image = None,
        target_eif_basename = "",
        docker_run_flags = [],
        build_script_file = "",
        commands_script_file = "",
        out_eif = "",
        out_eif_json = "",
        extra_deps = None):
    name = name or ctx.attr.name
    aws_nitro_image = aws_nitro_image or ctx.file.aws_nitro_image
    docker_run_flags = docker_run_flags or ctx.attr.docker_run_flags
    build_script = build_script_file or ctx.outputs.build_script
    commands_script = commands_script_file or ctx.outputs.commands_script
    source_image = source_image or ctx.file.source_image
    out_eif = out_eif or ctx.outputs.out_eif
    out_eif_json = out_eif_json or ctx.outputs.out_eif_json
    extra_deps = extra_deps or ctx.files.extra_deps

    basic_docker_run_flags = [
        "-v /var/run/docker.sock:/var/run/docker.sock",
    ]

    aws_image_tagged = "bazel/{}:{}".format(aws_nitro_image.owner.package, aws_nitro_image.owner.name)
    source_image_tagged = "bazel/{}:{}".format(source_image.owner.package, source_image.owner.name)

    toolchain_info = ctx.toolchains["@io_bazel_rules_docker//toolchains/docker:toolchain_type"].info

    # Generate a shell script to execute the run statement
    ctx.actions.expand_template(
        template = ctx.file._extract_tpl,
        output = build_script,
        substitutions = {
            "%{aws_image_tagged}": aws_image_tagged,
            "%{aws_nitro_image_tar}": aws_nitro_image.path,
            "%{commands}": commands_script.path,
            "%{docker_flags}": " ".join(toolchain_info.docker_flags),
            "%{docker_run_flags}": " ".join(basic_docker_run_flags + docker_run_flags),
            "%{docker_tool_path}": docker_path(toolchain_info),
            "%{image_id_extractor_path}": ctx.executable._extract_image_id.path,
            "%{out_eif_json}": out_eif_json.path,
            "%{out_eif}": out_eif.path,
            "%{source_image_tagged}": source_image_tagged,
            "%{source_image_tar}": source_image.path,
        },
        is_executable = True,
    )
    ctx.actions.run(
        inputs = extra_deps if extra_deps else [],
        outputs = [
            out_eif,
            out_eif_json,
            commands_script,
        ],
        tools = [
            source_image,
            aws_nitro_image,
            ctx.executable._extract_image_id,
        ],
        executable = build_script,
        use_default_shell_env = True,
    )
    return []

_extract_outputs = {
    "build_script": "%{name}.build",
    "commands_script": "%{name}.run.sh",
    "out_eif": "%{target_eif_basename}.eif",
    "out_eif_json": "%{target_eif_basename}.json",
}

nitro_enclave_image = rule(
    attrs = _nitro_enclave_image_attrs,
    doc = ("This rule runs a set of commands in a given image, waits" +
           "for the commands to finish, and then extracts a given file" +
           " from the container to the bazel-out directory."),
    outputs = _extract_outputs,
    implementation = _nitro_enclave_image_impl,
    toolchains = ["@io_bazel_rules_docker//toolchains/docker:toolchain_type"],
)
