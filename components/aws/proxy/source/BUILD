cc_library(
    name = "proxy_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
        exclude = [
            "proxy.cc",
            "preload.cc",
            "proxify.cc",
        ],
    ),
    copts = [
        "-std=c++17",
    ],
)

# This is used by tests to verify the functionality of this file. Not built to
# binaries.
cc_library(
    name = "preload",
    srcs = [
        ":preload.cc",
        ":protocol.cc",
        ":protocol.h",
    ],
    copts = [
        "-std=c++17",
    ],
)

cc_binary(
    name = "proxy",
    srcs = [":proxy.cc"],
    copts = [
        "-std=c++17",
    ],
    linkopts = ["-pthread"],
    deps = [
        ":proxy_lib",
    ],
)

cc_binary(
    name = "proxy_preload",
    srcs = [
        ":preload.cc",
        ":protocol.cc",
        ":protocol.h",
    ],
    copts = [
        "-fvisibility=hidden",
        "-std=c++17",
    ],
    linkopts = ["-ldl"],
    linkshared = True,
    visibility = [
        "//components/tools:__subpackages__",
        "//production/packaging:__subpackages__",
    ],
)

cc_binary(
    name = "proxify",
    srcs = [":proxify.cc"],
    copts = [
        "-std=c++17",
    ],
    visibility = [
        "//components/tools:__subpackages__",
        "//production/packaging:__subpackages__",
    ],
)
