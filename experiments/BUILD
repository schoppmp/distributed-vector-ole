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

load("@io_bazel_rules_docker//cc:image.bzl", "cc_image")
load("@io_bazel_rules_docker//container:container.bzl", "container_bundle")
load("@io_bazel_rules_docker//container:new_push.bzl", "new_container_push")
load("//experiments:docker_deploy_tag_setting.bzl", "docker_deploy_tag_setting")

DISTRIBUTED_VECTOR_OLE_DEFAULT_COPTS = [
    "-fopenmp",
    "-Wno-ignored-attributes",  # Needed for std::vector<emp::block>
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
        "@com_google_absl//absl/strings",
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
    base = "@distroless_base//image",
    binary = ":experiments",
)

# Rules for deploying to the HU registry
docker_deploy_tag_setting(
    name = "docker_deploy_tag",
    build_setting_default = "test",
)

genrule(
    name = "gen_docker_deploy_tag_file",
    outs = ["docker_deploy_tag_file"],
    cmd = "echo $(DOCKER_DEPLOY_TAG) > \"$@\"",
    toolchains = [":docker_deploy_tag"]
)

new_container_push(
    name = "deploy_hu",
    format = "Docker",
    registry = "gitlab.informatik.hu-berlin.de:4567",
    repository = "ti/research/distributed-vector-ole/experiments",
    image = ":experiments_image",
    tag_file = "docker_deploy_tag_file"
)