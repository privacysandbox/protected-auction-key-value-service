load("@rules_pkg//pkg:zip.bzl", "pkg_zip")
load(
    "//bazel:rpc_test.bzl",
    "rpc_diff_test_suite",
    "rpc_perf_test_suite",
)
load("//bazel:ssl_cert.bzl", "generate_ssl_certificate")
load(
    "//sut:endpoint.bzl",
    "endpoint",
)

generate_ssl_certificate("test_cert", "test_domain")

generate_ssl_certificate("test1a_cert", "test_domain1")

generate_ssl_certificate("test1b_cert", "test_domain1")

generate_ssl_certificate("test2_cert", "test_domain2")

generate_ssl_certificate("test3_cert", "test_domain3")

generate_ssl_certificate("test4_cert", "test_domain4")

generate_ssl_certificate("test5_cert", "test_domain5")

greeter_envoy_endpoints = [
    endpoint(
        docker_network = "functest-local",
        endpoint_type = endpoint_type,
        host = "greeter-envoy",
        port = 50060,
        rpcs = [
            (
                "greeter-sayhello-envoy",
                "helloworld.Greeter.SayHello",
                "//tests/servers:helloworld_descriptor_set",
            ),
        ],
        test_files_glob_spec_prefix = "test_cases/GREETER_ENVOY_{}".format(endpoint_type.upper()),
    )
    for endpoint_type in ("http", "grpc")
]

server_endpoints = [
    endpoint(
        docker_network = "functest-local",
        endpoint_type = "grpc",
        host = "greeter",
        port = 50051,
        rpcs = [
            (
                "greeter-sayhello",
                "helloworld.Greeter.SayHello",
                "//tests/servers:helloworld_descriptor_set",
            ),
        ],
        test_files_glob_spec_prefix = "test_cases/GREETER_ENDPOINT",
    ),
    endpoint(
        docker_network = "functest-local",
        endpoint_type = "grpc",
        host = "routeguide",
        port = 50051,
        rpcs = [
            (
                "routeguide-getfeature",
                "routeguide.RouteGuide.GetFeature",
                "//tests/servers:routeguide_descriptor_set",
            ),
            (
                "routeguide-listfeature",
                "routeguide.RouteGuide.ListFeatures",
                "//tests/servers:routeguide_descriptor_set",
            ),
        ],
        test_files_glob_spec_prefix = "test_cases/ROUTEGUIDE_ENDPOINT",
    ),
]

exposed_endpoints = greeter_envoy_endpoints + server_endpoints

[
    rpc_diff_test_suite(
        name = "{}-{}-diff".format(
            test_suite_name,
            endpoint.endpoint_type,
        ),
        endpoint = endpoint,
        protoset = protoset,
        rpc = rpc,
        test_files_glob_spec = "{}/{}/**/*".format(
            endpoint.test_files_glob_spec_prefix,
            rpc,
        ),
        test_tags = [
            "external",
            "manual",
        ],
    )
    for endpoint in exposed_endpoints
    for test_suite_name, rpc, protoset in endpoint.rpcs
]

[
    rpc_perf_test_suite(
        name = "{}-{}-perf".format(
            test_suite_name,
            endpoint.endpoint_type,
        ),
        endpoint = endpoint,
        protoset = protoset,
        rpc = rpc,
        test_files_glob_spec = "{}/{}/**/*".format(
            endpoint.test_files_glob_spec_prefix,
            rpc,
        ),
        test_tags = [
            "exclusive",
            "external",
            "manual",
        ],
    )
    for endpoint in exposed_endpoints
    for test_suite_name, rpc, protoset in endpoint.rpcs
    if endpoint.endpoint_type == "grpc"
]

test_types = {
    "grpc": [
        "diff",
        "perf",
    ],
    "http": ["diff"],
}

test_suite(
    name = "all_tests",
    tags = [
        "manual",
    ],
    tests = [
        "{}-{}-{}".format(
            test_suite_name,
            endpoint.endpoint_type,
            test_type,
        )
        for endpoint in exposed_endpoints
        for test_suite_name, rpc, protoset in endpoint.rpcs
        for test_type in test_types[endpoint.endpoint_type]
    ],
)

pkg_zip(
    name = "sut_data",
    srcs = [
        ":envoy_greeter.yaml",
        ":test_cert",
        "//tests/servers:helloworld_descriptor_set",
        "//tests/servers:routeguide_descriptor_set",
    ],
)

genrule(
    name = "copy_to_dist",
    srcs = [
        ":sut_data",
    ],
    outs = ["copy_to_dist.bin"],
    cmd_bash = """cat << EOF > '$@'
mkdir -p dist
cp $(execpath :sut_data) dist
builders/tools/normalize-dist
EOF""",
    executable = True,
    local = True,
    message = "copying sut data and configs to the dist directory",
)
