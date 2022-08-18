package(default_visibility = [
    "//components:__subpackages__",
])

cc_library(
    name = "data_orchestrator",
    srcs = [
        "data_orchestrator.cc",
    ],
    hdrs = [
        "data_orchestrator.h",
    ],
    deps = [
        "//components/data:blob_storage_change_notifier",
        "//components/data:blob_storage_client",
        "//components/data:delta_file_notifier",
        "//components/data:riegeli_stream_io",
        "//components/data_server/cache",
        "//components/errors:retry",
        "//public:constants",
        "//public/data_loading:data_loading_fbs",
        "//public/data_loading:filename_utils",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "data_orchestrator_test",
    srcs = [
        "data_orchestrator_test.cc",
    ],
    deps = [
        ":data_orchestrator",
        "//components/data:mocks",
        "//components/data_server/cache:mocks",
        "//components/test_util:proto_matcher",
        "//public/data_loading:filename_utils",
        "//public/data_loading:records_utils",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
