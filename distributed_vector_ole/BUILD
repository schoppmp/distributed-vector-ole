#    Distributed Vector-OLE Generator
#    Copyright (C) 2019 Phillipp Schoppmann and Adria Gascon
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU Affero General Public License as
#    published by the Free Software Foundation, either version 3 of the
#    License, or (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU Affero General Public License for more details.
#
#    You should have received a copy of the GNU Affero General Public License
#    along with this program.  If not, see <https://www.gnu.org/licenses/>.

package(default_visibility = ["//visibility:public"])

DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS = [
    "-maes",
    "-msse4.2",
    "-fopenmp",
    "-Wno-ignored-attributes",
    "-Wno-attributes",
]

cc_library(
    name = "ggm_tree",
    srcs = [
        "ggm_tree.cpp",
    ],
    hdrs = [
        "ggm_tree.h",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
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
        "@mpc_utils//third_party/gperftools",
    ],
)

cc_library(
    name = "all_but_one_random_ot_internal",
    hdrs = [
        "internal/all_but_one_random_ot_internal.h",
    ],
    deps = [
        ":ggm_tree",
        ":ntl_helpers",
        "@abseil//absl/numeric:int128",
        "@abseil//absl/types:span",
        "@com_github_emp_toolkit_emp_ot//:emp_ot",
        "@mpc_utils//third_party/ntl",
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
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    linkopts = [
        "-lgomp",
    ],
    deps = [
        ":all_but_one_random_ot_internal",
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
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    linkopts = [
        "-lgomp",
    ],
    deps = [
        ":all_but_one_random_ot",
        ":ntl_helpers",
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
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
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
        "@mpc_utils//third_party/ntl",
    ],
)

cc_test(
    name = "spfss_known_index_test",
    srcs = [
        "spfss_known_index_test.cpp",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    deps = [
        ":ntl_helpers",
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
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    deps = [
        ":ntl_helpers",
        ":spfss_known_index",
        "@com_github_emp_toolkit_emp_ot//:emp_ot",
        "@com_github_google_benchmark//:benchmark_main",
        "@mpc_utils//mpc_utils/testing:comm_channel_test_helper",
        "@mpc_utils//third_party/gperftools",
    ],
)

cc_library(
    name = "cuckoo_hasher",
    srcs = [
        "cuckoo_hasher.cpp",
    ],
    hdrs = [
        "cuckoo_hasher.h",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    linkopts = [
        "-lgomp",
    ],
    deps = [
        ":all_but_one_random_ot",
        "@abseil//absl/memory",
        "@abseil//absl/strings",
        "@abseil//absl/types:optional",
        "@abseil//absl/types:span",
        "@boringssl//:crypto",
        "@mpc_utils//mpc_utils:status",
        "@mpc_utils//mpc_utils:statusor",
        "@mpc_utils//third_party/ntl",
    ],
)

cc_test(
    name = "cuckoo_hasher_test",
    srcs = [
        "cuckoo_hasher_test.cpp",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    deps = [
        ":cuckoo_hasher",
        "@abseil//absl/container:flat_hash_map",
        "@abseil//absl/container:flat_hash_set",
        "@googletest//:gtest_main",
        "@mpc_utils//mpc_utils:status_matchers",
        "@mpc_utils//mpc_utils/testing:test_deps",
    ],
)

cc_binary(
    name = "cuckoo_hasher_benchmark",
    srcs = [
        "cuckoo_hasher_benchmark.cpp",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    deps = [
        ":cuckoo_hasher",
        "@com_github_google_benchmark//:benchmark_main",
        "@mpc_utils//third_party/gperftools",
    ],
)

cc_library(
    name = "mpfss_known_indices",
    srcs = [
        "mpfss_known_indices.cpp",
    ],
    hdrs = [
        "mpfss_known_indices.h",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    deps = [
        ":cuckoo_hasher",
        ":scalar_vector_gilboa_product",
        ":spfss_known_index",
        "@abseil//absl/container:flat_hash_set",
        "@abseil//absl/memory",
        "@boringssl//:crypto",
    ],
)

cc_test(
    name = "mpfss_known_indices_test",
    srcs = [
        "mpfss_known_indices_test.cpp",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    deps = [
        ":mpfss_known_indices",
        "@abseil//absl/container:flat_hash_map",
        "@googletest//:gtest_main",
        "@mpc_utils//mpc_utils:status_matchers",
        "@mpc_utils//mpc_utils/testing:comm_channel_test_helper",
        "@mpc_utils//mpc_utils/testing:test_deps",
        "@mpc_utils//third_party/ntl",
    ],
)

cc_binary(
    name = "mpfss_known_indices_benchmark",
    srcs = [
        "mpfss_known_indices_benchmark.cpp",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    deps = [
        ":mpfss_known_indices",
        "@com_github_emp_toolkit_emp_ot//:emp_ot",
        "@com_github_google_benchmark//:benchmark_main",
        "@mpc_utils//mpc_utils/testing:comm_channel_test_helper",
        "@mpc_utils//third_party/gperftools",
    ],
)

cc_library(
    name = "ntl_helpers",
    srcs = [
        "internal/ntl_helpers.cpp",
    ],
    hdrs = [
        "internal/ntl_helpers.h",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    visibility = ["//visibility:private"],
    deps = [
        "@abseil//absl/meta:type_traits",
        "@abseil//absl/numeric:int128",
        "@abseil//absl/types:optional",
        "@mpc_utils//third_party/ntl",
    ],
)

cc_library(
    name = "gilboa_internal",
    hdrs = [
        "internal/gilboa_internal.h",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    visibility = ["//visibility:private"],
    deps = [
        ":ntl_helpers",
        "@abseil//absl/numeric:int128",
        "@abseil//absl/types:span",
        "@boringssl//:crypto",
        "@com_github_emp_toolkit_emp_ot//:emp_ot",
        "@mpc_utils//mpc_utils:comm_channel",
        "@mpc_utils//mpc_utils:comm_channel_emp_adapter",
        "@mpc_utils//third_party/ntl",
    ],
)

cc_library(
    name = "scalar_vector_gilboa_product",
    srcs = [
        "scalar_vector_gilboa_product.cpp",
    ],
    hdrs = [
        "scalar_vector_gilboa_product.h",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    deps = [
        ":gilboa_internal",
        "@abseil//absl/memory",
        "@com_github_emp_toolkit_emp_ot//:emp_ot",
        "@mpc_utils//mpc_utils:comm_channel",
        "@mpc_utils//mpc_utils:comm_channel_emp_adapter",
        "@mpc_utils//mpc_utils:openssl_uniform_bit_generator",
        "@mpc_utils//mpc_utils:statusor",
    ],
)

cc_test(
    name = "scalar_vector_gilboa_product_test",
    srcs = [
        "scalar_vector_gilboa_product_test.cpp",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    deps = [
        ":scalar_vector_gilboa_product",
        "@abseil//absl/memory",
        "@googletest//:gtest_main",
        "@mpc_utils//mpc_utils:comm_channel",
        "@mpc_utils//mpc_utils:status_matchers",
        "@mpc_utils//mpc_utils:statusor",
        "@mpc_utils//mpc_utils/testing:comm_channel_test_helper",
        "@mpc_utils//mpc_utils/testing:test_deps",
        "@mpc_utils//third_party/ntl",
    ],
)

cc_binary(
    name = "scalar_vector_gilboa_product_benchmark",
    srcs = [
        "scalar_vector_gilboa_product_benchmark.cpp",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    deps = [
        ":scalar_vector_gilboa_product",
        "@com_github_google_benchmark//:benchmark_main",
        "@mpc_utils//mpc_utils/testing:comm_channel_test_helper",
        "@mpc_utils//third_party/gperftools",
    ],
)

cc_library(
    name = "distributed_vector_ole",
    hdrs = [
        "distributed_vector_ole.h",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    deps = [
        ":mpfss_known_indices",
        ":scalar_vector_gilboa_product",
        "@mpc_utils//mpc_utils:openssl_uniform_bit_generator",
        "@mpc_utils//mpc_utils/boost_serialization:eigen",
        "@mpc_utils//third_party/eigen",
    ],
)

cc_test(
    name = "distributed_vector_ole_test",
    srcs = [
        "distributed_vector_ole_test.cpp",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    deps = [
        ":distributed_vector_ole",
        "@abseil//absl/memory",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@mpc_utils//mpc_utils:comm_channel",
        "@mpc_utils//mpc_utils:status_matchers",
        "@mpc_utils//mpc_utils:statusor",
        "@mpc_utils//mpc_utils/testing:comm_channel_test_helper",
        "@mpc_utils//mpc_utils/testing:test_deps",
    ],
)

cc_binary(
    name = "distributed_vector_ole_benchmark",
    srcs = [
        "distributed_vector_ole_benchmark.cpp",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS,
    deps = [
        ":distributed_vector_ole",
        "@com_github_google_benchmark//:benchmark_main",
        "@mpc_utils//mpc_utils/testing:comm_channel_test_helper",
        "@mpc_utils//third_party/gperftools",
    ],
)
