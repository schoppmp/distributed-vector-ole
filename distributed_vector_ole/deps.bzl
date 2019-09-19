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

load(
    "@bazel_tools//tools/build_defs/repo:http.bzl",
    "http_archive",
)
load("@mpc_utils//mpc_utils:deps.bzl", "mpc_utils_deps")
load(
    "@rules_foreign_cc//:workspace_definitions.bzl",
    "rules_foreign_cc_dependencies",
)
load(
    "@io_bazel_rules_docker//repositories:repositories.bzl",
    container_repositories = "repositories",
)
load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_pull",
)

all_content = """
filegroup(
  name = "all",
  srcs = glob(["**"]),
  visibility = ["//visibility:public"],
)
"""

# Sanitize a dependency so that it works correctly from code that includes
# this workspace as a git submodule.
def clean_dep(dep):
    return str(Label(dep))

def distributed_vector_ole_deps():
    # Initialize transitive dependencies.
    rules_foreign_cc_dependencies()

    # Load EMP-OT before mpc_utils_deps() because we need a patched version.
    # TODO(adria) merge this version upstream or put the patch in this repo.
    if "com_github_emp_toolkit_emp_ot" not in native.existing_rules():
        http_archive(
            name = "com_github_emp_toolkit_emp_ot",
            url = "https://github.com/adriagascon/emp-ot/archive/90319da396e731d0b24e90e1ccaa3b76fa73f641.zip",
            sha256 = "8cc43964b0d1880429acf6dd972f1e080923c71d25127e74607f628b7cdc6c30",
            strip_prefix = "emp-ot-90319da396e731d0b24e90e1ccaa3b76fa73f641",
            build_file = clean_dep("@mpc_utils//third_party:emp_ot.BUILD"),
        )
    mpc_utils_deps(enable_oblivc = False)

    container_repositories()

    # Something needs a recend GlibC.
    container_pull(
        name = "archlinux_base",
        digest = "sha256:29ef3558fb2f91376782e46415299cf85618aad3d2859fb4ce342a63087bc6a3",
        registry = "index.docker.io",
        repository = "archlinux/base",
    )

    if "org_gmplib" not in native.existing_rules():
        http_archive(
            name = "org_gmplib",
            urls = [
                "https://gmplib.org/download/gmp/gmp-6.1.2.tar.xz",
                "https://ftp.gnu.org/gnu/gmp/gmp-6.1.2.tar.xz",
            ],
            strip_prefix = "gmp-6.1.2",
            build_file_content = all_content,
            sha256 = "87b565e89a9a684fe4ebeeddb8399dce2599f9c9049854ca8c0dfbdea0e21912",
        )

    if "boringssl" not in native.existing_rules():
        http_archive(
            name = "boringssl",
            sha256 = "1188e29000013ed6517168600fc35a010d58c5d321846d6a6dfee74e4c788b45",
            strip_prefix = "boringssl-7f634429a04abc48e2eb041c81c5235816c96514",
            urls = [
                "https://mirror.bazel.build/github.com/google/boringssl/archive/7f634429a04abc48e2eb041c81c5235816c96514.tar.gz",
                "https://github.com/google/boringssl/archive/7f634429a04abc48e2eb041c81c5235816c96514.tar.gz",
            ],
        )

    if "com_github_google_benchmark" not in native.existing_rules():
        http_archive(
            name = "com_github_google_benchmark",
            url = "https://github.com/google/benchmark/archive/bf4f2ea0bd1180b34718ac26eb79b170a4f6290e.zip",
            sha256 = "e474a7f0112b9f2cd7e26ccd03c39d1c68114d3cad8f292021143b548fb00db7",
            strip_prefix = "benchmark-bf4f2ea0bd1180b34718ac26eb79b170a4f6290e",
        )
