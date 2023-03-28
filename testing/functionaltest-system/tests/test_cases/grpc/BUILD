load(
    "//bazel:rpc_test.bzl",
    "grpc_diff_test_suite",
    "grpc_perf_test_suite",
)

rpcs = [
    ("greeter_tests", "GREETER_ENDPOINT", "helloworld.Greeter.SayHello", "//tests:helloworld_descriptor_set"),
    ("routeguide_getfeat_tests", "ROUTEGUIDE_ENDPOINT", "routeguide.RouteGuide.GetFeature", "//tests:routeguide_descriptor_set"),
    ("routeguide_listfeats_tests", "ROUTEGUIDE_ENDPOINT", "routeguide.RouteGuide.ListFeatures", "//tests:routeguide_descriptor_set"),
]

[
    grpc_diff_test_suite(
        name = "grpcurl-{}".format(test_suite_name),
        endpoint = endpoint,
        protoset = protoset,
        rpc = rpc,
        test_files_glob_spec = "{}/{}/**/*".format(endpoint, rpc),
        test_tags = [
            "external",
            "manual",
        ],
    )
    for test_suite_name, endpoint, rpc, protoset in rpcs
]

[
    grpc_perf_test_suite(
        name = "ghz-{}".format(test_suite_name),
        endpoint = endpoint,
        protoset = protoset,
        rpc = rpc,
        test_files_glob_spec = "{}/{}/**/*".format(endpoint, rpc),
        test_tags = [
            "exclusive",
            "external",
            "manual",
        ],
    )
    for test_suite_name, endpoint, rpc, protoset in rpcs
]
