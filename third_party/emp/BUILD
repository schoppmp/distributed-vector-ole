load("@rules_foreign_cc//tools/build_defs:cmake.bzl", "cmake_external")
load("@rules_foreign_cc//tools/build_defs:configure.bzl", "configure_make")

licenses(["notice"])  # MIT

cmake_external(
    name = "emp_tool_build",
    cache_entries = {
        "RELIC_INCLUDE_DIR": "$EXT_BUILD_DEPS/relic/include",
        "CMAKE_PREFIX_PATH": "$EXT_BUILD_DEPS/relic:$EXT_BUILD_DEPS/relic:",
        "EMP_USE_RANDOM_DEVICE": "1",
    },
    defines = [
        "EMP_USE_RANDOM_DEVICE=1",
    ],
    lib_source = "@com_github_emp_toolkit_emp_tool//:all",
    make_commands = [
        "make emp-tool",
        "make install/fast",
    ],
    shared_libraries = [
        "libemp-tool.so",
    ],
    deps = [
        "//third_party/relic",
        "@mpc_utils//third_party/gmp",
    ],
)

cmake_external(
    name = "emp_ot_build",
    cache_entries = {
        "CMAKE_PREFIX_PATH": "$EXT_BUILD_DEPS/emp_tool_build",
        "RELIC_INCLUDE_DIR": "$EXT_BUILD_DEPS/relic/include",
        "EMP_USE_RANDOM_DEVICE": "1",
    },
    headers_only = True,
    lib_source = "@com_github_emp_toolkit_emp_ot//:all",
    make_commands = [
        "make install/fast",
    ],
    deps = [
        ":emp_tool_build",
    ],
)

# We need libcrypto, but we can't include it in the deps to cmake_external due
# to a limitation of rules_foreign_cc. So we wrap both in a cc_library that
# depends on EMP and libcrypto.
cc_library(
    name = "emp_ot",
    visibility = ["//visibility:public"],
    deps = [
        ":emp_ot_build",
        "@boost//:system",
        "@boringssl//:crypto",
    ],
)

# We  only include a single test to ensure EMP links properly.
cc_test(
    name = "emp_ot_test",
    size = "small",
    srcs = [
        "emp_ot_test.cpp",
    ],
    # These copts need to be passed to anything that depends on EMP.
    copts = [
        "-maes",
        "-mavx",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":emp_ot",
        "@boost//:thread",
        "@com_github_emp_toolkit_emp_ot//:test_header",
        "@mpc_utils//mpc_utils/testing:test_deps",
    ],
)
