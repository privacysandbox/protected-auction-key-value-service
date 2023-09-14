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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def cpp_repositories():
    """Entry point for all external repositories used for C++/C dependencies."""

    #riegeli
    http_archive(
        name = "com_google_riegeli",
        repo_mapping = {
            "@org_brotli": "@brotli",
        },
        sha256 = "32f303a9b0b6e07101a7a95a4cc364fb4242f0f7431de5da1a2e0ee61f5924c5",
        strip_prefix = "riegeli-562f26cbb685aae10b7d32e32fb53d2e42a5d8c2",
        url = "https://github.com/google/riegeli/archive/562f26cbb685aae10b7d32e32fb53d2e42a5d8c2.zip",
    )

    #external deps for riegeli
    http_archive(
        name = "net_zstd",
        build_file = "//third_party:zstd.BUILD",
        sha256 = "7c42d56fac126929a6a85dbc73ff1db2411d04f104fae9bdea51305663a83fd0",
        strip_prefix = "zstd-1.5.2/lib",
        urls = ["https://github.com/facebook/zstd/releases/download/v1.5.2/zstd-1.5.2.tar.gz"],
    )

    http_archive(
        name = "snappy",
        build_file = "@com_google_riegeli//third_party:snappy.BUILD",
        sha256 = "e170ce0def2c71d0403f5cda61d6e2743373f9480124bcfcd0fa9b3299d428d9",
        strip_prefix = "snappy-1.1.9",
        urls = ["https://github.com/google/snappy/archive/1.1.9.zip"],
    )

    http_archive(
        name = "highwayhash",
        build_file = "//third_party:highwayhash.BUILD",
        sha256 = "cf891e024699c82aabce528a024adbe16e529f2b4e57f954455e0bf53efae585",
        strip_prefix = "highwayhash-276dd7b4b6d330e4734b756e97ccfb1b69cc2e12",
        urls = ["https://github.com/google/highwayhash/archive/276dd7b4b6d330e4734b756e97ccfb1b69cc2e12.zip"],
    )

    http_archive(
        name = "com_github_google_flatbuffers",
        sha256 = "80af25a873bebba60067a1529c03edcc5fc5486c3402354c03a80a5279da5dca",
        strip_prefix = "flatbuffers-2.0.8",
        urls = ["https://github.com/google/flatbuffers/archive/v2.0.8.zip"],
    )

    http_archive(
        name = "sqlite3",
        build_file = "//third_party:sqlite3.BUILD",
        sha256 = "9c99955b21d2374f3a385d67a1f64cbacb1d4130947473d25c77ad609c03b4cd",
        strip_prefix = "sqlite-amalgamation-3390400",
        urls = [
            "https://www.sqlite.org/2022/sqlite-amalgamation-3390400.zip",
        ],
    )

    ### Google Benchmark
    http_archive(
        name = "com_google_benchmark",
        sha256 = "aeec52381284ec3752505a220d36096954c869da4573c2e1df3642d2f0a4aac6",
        strip_prefix = "benchmark-1.7.1",
        urls = [
            "https://github.com/google/benchmark/archive/refs/tags/v1.7.1.zip",
        ],
    )

EMSCRIPTEN_VER = "3.1.44"

def emscripten_repositories():
    http_archive(
        name = "emsdk",
        sha256 = "48c0dd06539011a62ec1069a6c69e283731dabdb20454c45fb344dede421b5ad",
        strip_prefix = "emsdk-{ver}/bazel".format(ver = EMSCRIPTEN_VER),
        url = "https://github.com/emscripten-core/emsdk/archive/refs/tags/{ver}.zip".format(ver = EMSCRIPTEN_VER),
    )
    return EMSCRIPTEN_VER
