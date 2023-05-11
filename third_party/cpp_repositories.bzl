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
load("@google_privacysandbox_servers_common//:cpp_deps.bzl", shared_cpp_dependencies = "cpp_dependencies")

def cpp_repositories():
    """Entry point for all external repositories used for C++/C dependencies."""

    shared_cpp_dependencies()

    http_archive(
        name = "aws-checksums",
        build_file = "//third_party:aws_checksums.BUILD",
        sha256 = "6e6bed6f75cf54006b6bafb01b3b96df19605572131a2260fddaf0e87949ced0",
        strip_prefix = "aws-checksums-0.1.5",
        urls = [
            "https://github.com/awslabs/aws-checksums/archive/v0.1.5.tar.gz",
        ],
    )

    http_archive(
        name = "aws-c-common",
        build_file = "//third_party:aws_c_common.BUILD",
        sha256 = "01c2a58553a37b3aa5914d9e0bf7bf14507ff4937bc5872a678892ca20fcae1f",
        strip_prefix = "aws-c-common-0.4.29",
        urls = [
            "https://github.com/awslabs/aws-c-common/archive/v0.4.29.tar.gz",
        ],
    )

    http_archive(
        name = "aws-c-event-stream",
        build_file = "//third_party:aws_c_event_stream.BUILD",
        sha256 = "31d880d1c868d3f3df1e1f4b45e56ac73724a4dc3449d04d47fc0746f6f077b6",
        strip_prefix = "aws-c-event-stream-0.1.4",
        urls = [
            "https://github.com/awslabs/aws-c-event-stream/archive/v0.1.4.tar.gz",
        ],
    )

    http_archive(
        name = "aws_sdk_cpp",
        build_file = "//third_party:aws_sdk_cpp.BUILD",
        patch_cmds = [
            """sed -i.bak 's/UUID::RandomUUID/Aws::Utils::UUID::RandomUUID/g' aws-cpp-sdk-core/source/client/AWSClient.cpp""",
            # Apply fix in https://github.com/aws/aws-sdk-cpp/commit/9669a1c1d9a96621cd0846679cbe973c648a64b3
            """sed -i.bak 's/Tags\\.entry/Tag/g' aws-cpp-sdk-sqs/source/model/TagQueueRequest.cpp""",
        ],
        sha256 = "749322a8be4594472512df8a21d9338d7181c643a00e08a0ff12f07e831e3346",
        strip_prefix = "aws-sdk-cpp-1.8.186",
        urls = [
            "https://github.com/aws/aws-sdk-cpp/archive/1.8.186.tar.gz",
        ],
    )

    http_archive(
        name = "curl",
        build_file = "//third_party:curl.BUILD",
        sha256 = "ff3e80c1ca6a068428726cd7dd19037a47cc538ce58ef61c59587191039b2ca6",
        strip_prefix = "curl-7.49.1",
        urls = ["https://mirror.bazel.build/curl.haxx.se/download/curl-7.49.1.tar.gz"],
    )

    http_archive(
        name = "zlib_archive",
        build_file = "//third_party:zlib.BUILD",
        sha256 = "91844808532e5ce316b3c010929493c0244f3d37593afd6de04f71821d5136d9",
        strip_prefix = "zlib-1.2.12",
        urls = ["https://mirror.bazel.build/zlib.net/zlib-1.2.12.tar.gz"],
    )

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
