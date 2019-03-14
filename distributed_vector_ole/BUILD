cc_library(
    name = "ggm_tree",
    srcs = [
        "ggm_tree.cpp",
    ],
    hdrs = [
        "ggm_tree.h",
    ],
    deps = [
        "@abseil//absl/memory",
        "@abseil//absl/types:span",
        "@boringssl//:crypto",
        "@mpc_utils//mpc_utils:statusor",
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
        "@com_github_google_benchmark//:benchmark_main",
        "@mpc_utils//mpc_utils/testing:test_deps",
    ],
)
