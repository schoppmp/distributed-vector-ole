cc_library(
    name = "ggm_tree",
    srcs = [
        "ggm_tree.cpp",
    ],
    hdrs = [
        "ggm_tree.h",
    ],
    deps = [
        "@abseil//absl/types:span",
        "@boringssl//:crypto",
    ],
)

cc_test(
    name = "ggm_tree_test",
    srcs = [
        "ggm_tree_test.cpp",
    ],
    deps = [
        ":ggm_tree",
        "@boost//:asio",
        "@boost//:thread",
        "@mpc_utils//mpc_utils/testing:test_deps",
    ],
)
