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

def functional_test_files_for(
        glob_spec,
        request_suffix = ".request.json",
        reply_suffix = ".reply.json",
        pre_filter_suffix = ".pre-filter.jq",
        post_filter_suffix = ".filter.jq",
        post_filter_slurp_suffix = ".filter.slurp.jq"):
    file_types = ("request", "reply", "pre-filter", "post-filter", "post-filter-slurp")
    suffixes = (request_suffix, reply_suffix, pre_filter_suffix, post_filter_suffix, post_filter_slurp_suffix)
    files = {
        (test_name, filetype): fullpath
        for filetype, suffix in zip(file_types, suffixes)
        for fullpath in native.glob(["{}{}".format(glob_spec, suffix)])
        for test_name in [fullpath.removesuffix(suffix).rpartition("/")[2]]
    }
    test_files = {
        test_name: {
            file_type: files.get((test_name, file_type), "")
            for file_type in file_types
        }
        for test_name in {tname: 0 for tname, _ in files}
    }
    return test_files

def rpc_diff_test(
        name,
        request,
        golden_reply,
        endpoint,
        rpc,
        protoset = "",
        jq_pre_filter = "",
        jq_post_filter = "",
        jq_post_slurp = False,
        tags = [],
        **kwargs):
    """Generates a diff test for a grpc request/reply.

    Args:
      name: test suite name
      request: label of request file
      golden_reply: label of reply file
      endpoint: struct for endpoint defining the protocol, host, port etc
      rpc: gRPC qualified rpc name
      protoset: protobuf descriptor set label or file
      jq_pre_filter: jq filter program as string to apply to the rpc request
      jq_post_filter: jq filter program as string to apply to the rpc response
      jq_post_slurp: boolean to indicate use of jq --slurp for the rpc response
      tags: tag list for the tests
      **kwargs: additional test args
    """
    if endpoint.endpoint_type == "grpc":
        runner = Label("//bazel:grpcurl_diff_test_runner")
    elif endpoint.endpoint_type == "http":
        runner = Label("//bazel:curl_diff_test_runner")
    else:
        fail("[rpc_diff_test] unsupported endpoint type:", endpoint.endpoint_type)
    args = [
        "--endpoint-hostport",
        "{}:{}".format(endpoint.host, endpoint.port),
        "--endpoint-type",
        endpoint.endpoint_type,
        "--rpc",
        rpc,
        "--protoset",
        "$(rootpath {})".format(protoset),
        "--request",
        "$(execpath {})".format(request),
        "--reply",
        "$(execpath {})".format(golden_reply),
    ]
    if endpoint.docker_network:
        args.extend([
            "--docker-network",
            endpoint.docker_network,
        ])

    data = [
        request,
        golden_reply,
        protoset,
    ]
    if jq_pre_filter:
        args.extend(["--jq-pre-filter", "$(execpath {})".format(jq_pre_filter)])
        data.append(jq_pre_filter)
    if jq_post_filter:
        args.extend(["--jq-post-filter", "$(execpath {})".format(jq_post_filter)])
        data.append(jq_post_filter)
    if jq_post_slurp:
        args.extend(["--jq-post-slurp"])
    native.sh_test(
        name = name,
        srcs = [runner],
        args = args,
        data = data,
        tags = tags,
        **kwargs
    )

def rpc_diff_test_suite(
        name,
        endpoint,
        rpc,
        test_files_glob_spec,
        protoset = "",
        test_tags = [],
        **kwargs):
    """Generate a test suite for test cases within the specified directory tree.

    Args:
      name: test suite name
      test_files_glob_spec: glob spec for test files, passed to function functional_test_files_for()
      protoset: protobuf descriptor set label or file
      test_tags: tag list for the tests
      **kwargs: additional args
    """
    test_files = functional_test_files_for(glob_spec = test_files_glob_spec)
    if not test_files:
        print("no test files found for glob spec")
        return

    test_labels = []
    for test_name, testcase_files in test_files.items():
        qual_test_name = "{}-{}".format(name, test_name)
        test_labels.append(":{}".format(qual_test_name))
        extra_kwargs = dict(kwargs)
        if testcase_files["pre-filter"]:
            extra_kwargs["jq_pre_filter"] = ":{}".format(testcase_files["pre-filter"])
        if testcase_files["post-filter"]:
            extra_kwargs["jq_post_filter"] = ":{}".format(testcase_files["post-filter"])
        elif testcase_files["post-filter-slurp"]:
            extra_kwargs["jq_post_filter"] = ":{}".format(testcase_files["post-filter-slurp"])
            extra_kwargs["jq_post_slurp"] = True
        rpc_diff_test(
            name = qual_test_name,
            endpoint = endpoint,
            golden_reply = ":{}".format(testcase_files["reply"]),
            protoset = protoset,
            request = ":{}".format(testcase_files["request"]),
            rpc = rpc,
            tags = test_tags,
            **extra_kwargs
        )
    native.test_suite(
        name = name,
        tests = test_labels,
        tags = test_tags,
    )

def rpc_perf_test(
        name,
        request,
        endpoint,
        rpc,
        protoset,
        tags = [],
        **kwargs):
    """Generate a ghz report for a grpc request.

    Args:
      name: test suite name
      request: label of request file
      endpoint: struct for endpoint defining the protocol, host, port etc
      rpc: gRPC qualified rpc name
      protoset: protobuf descriptor set label or file
      tags: tag list for the tests
      **kwargs: additional test args
    """
    if endpoint.endpoint_type == "grpc":
        runner = Label("//bazel:ghz_test_runner")
        #elif endpoint.endpoint_type == "http":
        #    runner = Label("//bazel:ab_test_runner")

    else:
        fail("[rpc_perf_test] unsupported endpoint type:", endpoint.endpoint_type)

    args = [
        "--endpoint-hostport",
        "{}:{}".format(endpoint.host, endpoint.port),
        "--rpc",
        rpc,
        "--protoset",
        "$(rootpath {})".format(protoset),
        "--request",
        "$(execpath {})".format(request),
    ]
    if endpoint.docker_network:
        args.extend([
            "--docker-network",
            endpoint.docker_network,
        ])
    native.sh_test(
        name = name,
        srcs = [runner],
        args = args,
        data = [
            request,
            protoset,
        ],
        tags = tags,
        **kwargs
    )

def rpc_perf_test_suite(
        name,
        endpoint,
        rpc,
        test_files_glob_spec,
        protoset,
        test_tags = [],
        **kwargs):
    """Generates a test suite for test cases within the specified directory tree.

    Args:
      name: test suite name
      test_files_glob_spec: glob spec for test files, passed to function functional_test_files_for()
      protoset: protobuf descriptor set label or file
      test_tags: tag list for the tests
      **kwargs: additional args
    """
    test_files = functional_test_files_for(glob_spec = test_files_glob_spec)
    if not test_files:
        print("no test files found for glob spec")
        return

    test_labels = []
    for test_name, testcase_files in test_files.items():
        qual_test_name = "{}-{}".format(name, test_name)
        test_labels.append(":{}".format(qual_test_name))
        extra_kwargs = dict(kwargs)
        rpc_perf_test(
            name = qual_test_name,
            endpoint = endpoint,
            protoset = protoset,
            request = ":{}".format(testcase_files["request"]),
            rpc = rpc,
            tags = test_tags,
            **extra_kwargs
        )

    native.test_suite(
        name = name,
        tests = test_labels,
        tags = test_tags,
    )
