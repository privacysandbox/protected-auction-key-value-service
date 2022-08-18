cc_binary(
    name = "test_serving_data_generator",
    srcs = ["test_serving_data_generator.cc"],
    deps = [
        "//public/data_loading:data_loading_fbs",
        "//public/data_loading:filename_utils",
        "//public/data_loading:records_utils",
        "//public/data_loading:riegeli_metadata_cc_proto",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/strings",
        "@com_google_riegeli//riegeli/bytes:ostream_writer",
        "@com_google_riegeli//riegeli/records:record_writer",
    ],
)
