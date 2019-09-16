load("@io_bazel_rules_docker//cc:image.bzl", "cc_image")
load("@io_bazel_rules_docker//container:container.bzl", "container_bundle")

DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS = [
    "-maes",
    "-msse4.2",
    "-fopenmp",
    "-Wno-ignored-attributes",
    "-Wno-attributes",
]

cc_binary(
    name = "experiments",
    srcs = [
        "experiments.cpp",
    ],
    copts = DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS + [
        "-Wno-unused-variable",
        "-Wno-unused-function",
    ],
    deps = [
        "//distributed_vector_ole",
        "//distributed_vector_ole:scalar_vector_gilboa_product",
        "@abseil//absl/strings",
        "@mpc_utils//mpc_utils:benchmarker",
        "@mpc_utils//mpc_utils:comm_channel",
        "@mpc_utils//mpc_utils:mpc_config",
        "@mpc_utils//mpc_utils:status",
        "@mpc_utils//mpc_utils:statusor",
        "@mpc_utils//third_party/gperftools",
    ],
)

cc_image(
    name = "experiments_image",
    base = "@archlinux_base//image",
    binary = ":experiments",
)

container_bundle(
    name = "deploy",
    images = {
        "gitlab.informatik.hu-berlin.de:4567/ti/research/distributed-vector-ole/experiments": ":experiments_image",
    },
)
