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
        "@abseil//absl/numeric:int128",
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
        "@googletest//:gtest_main",
        "@mpc_utils//mpc_utils:status_matchers",
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
    ],
)

cc_library(
    name = "all_but_one_random_ot",
    srcs = [
        "all_but_one_random_ot.cpp",
    ],
    hdrs = [
        "all_but_one_random_ot.h",
    ],
    copts = [
        "-maes",
        "-msse4.2",
        "-fopenmp",
    ],
    linkopts = [
        "-lgomp",
    ],
    deps = [
        ":ggm_tree",
        "@abseil//absl/types:span",
        "@com_github_emp_toolkit_emp_ot//:emp_ot",
        "@mpc_utils//mpc_utils:benchmarker",
        "@mpc_utils//mpc_utils:comm_channel",
        "@mpc_utils//mpc_utils:comm_channel_emp_adapter",
        "@mpc_utils//third_party/ntl",
    ],
)

cc_test(
    name = "all_but_one_random_ot_test",
    srcs = [
        "all_but_one_random_ot_test.cpp",
    ],
    copts = [
        "-maes",
        "-msse4.2",
    ],
    deps = [
        ":all_but_one_random_ot",
        "@abseil//absl/memory",
        "@googletest//:gtest_main",
        "@mpc_utils//mpc_utils:comm_channel",
        "@mpc_utils//mpc_utils:comm_channel_emp_adapter",
        "@mpc_utils//mpc_utils:status_matchers",
        "@mpc_utils//mpc_utils:statusor",
        "@mpc_utils//mpc_utils/testing:comm_channel_test_helper",
        "@mpc_utils//mpc_utils/testing:test_deps",
    ],
)

cc_library(
    name = "spfss_known_index",
    srcs = [
        "spfss_known_index.cpp",
    ],
    hdrs = [
        "spfss_known_index.h",
    ],
    copts = [
        "-maes",
        "-msse4.2",
        "-fopenmp",
    ],
    linkopts = [
        "-lgomp",
    ],
    deps = [
        ":all_but_one_random_ot",
        "@abseil//absl/types:span",
        "@mpc_utils//mpc_utils:benchmarker",
        "@mpc_utils//mpc_utils:comm_channel",
        "@mpc_utils//mpc_utils/boost_serialization:abseil",
        "@mpc_utils//mpc_utils/boost_serialization:ntl",
    ],
)

cc_test(
    name = "spfss_known_index_test",
    srcs = [
        "spfss_known_index_test.cpp",
    ],
    copts = [
        "-maes",
        "-msse4.2",
    ],
    deps = [
        ":spfss_known_index",
        "@abseil//absl/memory",
        "@googletest//:gtest_main",
        "@mpc_utils//mpc_utils:comm_channel",
        "@mpc_utils//mpc_utils:status_matchers",
        "@mpc_utils//mpc_utils:statusor",
        "@mpc_utils//mpc_utils/testing:comm_channel_test_helper",
        "@mpc_utils//mpc_utils/testing:test_deps",
    ],
)

cc_binary(
    name = "spfss_known_index_benchmark",
    srcs = [
        "spfss_known_index_benchmark.cpp",
    ],
    copts = [
        "-maes",
        "-msse4.2",
        "-fopenmp",
    ],
    linkopts = [
        "-lgomp",
    ],
    deps = [
        ":spfss_known_index",
        "@com_github_emp_toolkit_emp_ot//:emp_ot",
        "@com_github_google_benchmark//:benchmark_main",
        "@mpc_utils//mpc_utils/testing:comm_channel_test_helper",
        "@mpc_utils//third_party/gperftools",
    ],
)
