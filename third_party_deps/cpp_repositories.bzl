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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")

def cpp_repositories():
    """Entry point for all external repositories used for C++/C dependencies."""

    #riegeli
    http_archive(
        name = "com_google_riegeli",
        repo_mapping = {
            "@org_brotli": "@brotli",
        },
        patch_args = ["-p1"],
        patches = ["//third_party_deps:riegeli.patch"],
        sha256 = "856788984a1c13916dde67aed7a26bca7b4b92b9c092974203660502d7c92ffc",
        strip_prefix = "riegeli-d6eed6e5673c946af8976869e4dee647b065774f",
        url = "https://github.com/google/riegeli/archive/d6eed6e5673c946af8976869e4dee647b065774f.zip",
    )

    #external deps for riegeli
    http_archive(
        name = "net_zstd",
        build_file = "//third_party_deps:zstd.BUILD",
        sha256 = "8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1",
        strip_prefix = "zstd-1.5.6/lib",
        urls = ["https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz"],
    )

    http_archive(
        name = "snappy",
        build_file = "@com_google_riegeli//third_party:snappy.BUILD",
        sha256 = "7ee7540b23ae04df961af24309a55484e7016106e979f83323536a1322cedf1b",
        strip_prefix = "snappy-1.2.0",
        urls = ["https://github.com/google/snappy/archive/1.2.0.zip"],
    )

    http_archive(
        name = "highwayhash",
        build_file = "//third_party_deps:highwayhash.BUILD",
        sha256 = "8be5e0af6ede048c54c8355e0d7bb87305531021d19b1f6334d5c16edb290c81",
        strip_prefix = "highwayhash-5ad3bf8444cfc663b11bf367baaa31f36e7ff7c8",
        urls = ["https://github.com/google/highwayhash/archive/5ad3bf8444cfc663b11bf367baaa31f36e7ff7c8.zip"],
    )

    http_archive(
        name = "com_github_google_flatbuffers",
        sha256 = "80af25a873bebba60067a1529c03edcc5fc5486c3402354c03a80a5279da5dca",
        strip_prefix = "flatbuffers-2.0.8",
        urls = ["https://github.com/google/flatbuffers/archive/v2.0.8.zip"],
    )

    http_archive(
        name = "sqlite3",
        build_file = "//third_party_deps:sqlite3.BUILD",
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

    http_archive(
        name = "avro",
        build_file = "//third_party_deps:avro.BUILD",
        sha256 = "c849ef7f7af58ce66e7b999b8d9815507d69ae434e7e058a44f7818515455a03",
        strip_prefix = "avro-release-1.10.2/lang/c++",
        add_prefix = "third_party/avro",
        urls = [
            "https://github.com/apache/avro/archive/release-1.10.2.tar.gz",
        ],
    )

    ### Roaring Bitmaps
    http_archive(
        name = "roaring_bitmap",
        build_file = "//third_party_deps:roaring.BUILD",
        sha256 = "c7b0e36dfeaca0d951b2842a747ddf6fec95355abba5970511bb68d698e10a90",
        strip_prefix = "CRoaring-3.0.1",
        urls = [
            "https://github.com/RoaringBitmap/CRoaring/archive/refs/tags/v3.0.1.zip",
        ],
    )

    http_file(
        name = "otel_collector_aarch64",
        url = "https://aws-otel-collector.s3.amazonaws.com/amazon_linux/arm64/v0.40.0/aws-otel-collector.rpm",
        sha256 = "c1860bac86d2c8b21a7448bb41b548589f3a65507f7768be94a9bf36ec188801",
    )

    http_file(
        name = "otel_collector_amd64",
        url = "https://aws-otel-collector.s3.amazonaws.com/amazon_linux/amd64/v0.40.0/aws-otel-collector.rpm",
        sha256 = "3d3837ad0b0a32b905b94713ab3534eb58c377cf211a9c75d89d39f35b0f4152",
    )
