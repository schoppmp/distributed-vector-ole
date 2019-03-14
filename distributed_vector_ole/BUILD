cc_library(
    name = "ggm_tree",
    srcs = [
        "ggm_tree.cpp",
    ],
    hdrs = [
        "ggm_tree.h",
    ],
    copts = [
        "-fopenmp",
    ],
    linkopts = [
        "-lgomp",
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
    size = "small",
    srcs = [
        "ggm_tree_test.cpp",
    ],
    deps = [
        ":ggm_tree",
        "@mpc_utils//mpc_utils/testing:test_deps",
    ],
)

cc_binary(
    name = "ggm_tree_benchmark",
    srcs = [
        "ggm_tree_benchmark.cpp",
    ],
    deps = [
        ":ggm_tree",
        "@com_github_google_benchmark//:benchmark_main",
        "@googletest//:gtest_main",
    ],
)
