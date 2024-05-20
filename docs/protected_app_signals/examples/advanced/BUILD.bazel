# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@bazel_skylib//rules:run_binary.bzl", "run_binary")
load("@rules_python//python:defs.bzl", "py_binary")

py_binary(
    name = "data_generator",
    srcs = ["data_generator.py"],
    deps = [
        "@word2vec_gensim//:pkg",
        "@word2vec_nltk//:pkg",
        "@word2vec_scipy//:pkg",
    ],
)

run_binary(
    name = "generate_csv",
    outs = [
        "categories.csv",
        "embeddings.csv",
    ],
    args = [
        "--categories-path",
        "$(location categories.csv)",
        "--embeddings-path",
        "$(location embeddings.csv)",
    ],
    tags = ["manual"],
    tool = ":data_generator",
)

[
    run_binary(
        name = "generate_{}_delta".format(name),
        srcs = [
            ":categories.csv",
            ":embeddings.csv",
        ],
        outs = [
            "DELTA_000000000000000{}".format(num),
        ],
        args = [
            "format_data",
            "--input_file",
            "$(location {}.csv)".format(name),
            "--input_format",
            "CSV",
            "--output_file",
            "$(location DELTA_000000000000000{})".format(num),
            "--output_format",
            "DELTA",
        ],
        tags = ["manual"],
        tool = "//tools/data_cli",
    )
    for num, name in enumerate(
        ("categories", "embeddings"),
        1,
    )
]

delta_out = "DELTA_0000000000000003"

run_binary(
    name = "udf_delta",
    srcs = [
        ":udf.js",
    ],
    outs = [
        delta_out,
    ],
    args = [
        "--udf_file_path",
        "$(location :udf.js)",
        "--output_path",
        "$(location {})".format(delta_out),
    ],
    tool = "//tools/udf/udf_generator:udf_delta_file_generator",
)
