load("@rules_cc//cc:defs.bzl", "cc_binary")

package(default_visibility = [
    "//tools:__subpackages__",
])

cc_binary(
    name = "data_cli",
    srcs = ["data_cli.cc"],
    deps = [
        "//public/data_loading:filename_utils",
        "//public/data_loading:records_utils",
        "//tools/data_cli/commands:command",
        "//tools/data_cli/commands:generate_data_command",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
    ],
)
