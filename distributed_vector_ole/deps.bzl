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
load("@mpc_utils//third_party:repo.bzl", "third_party_http_archive")
load(
    "@rules_foreign_cc//:workspace_definitions.bzl",
    "rules_foreign_cc_dependencies",
)
load(
    "@io_bazel_rules_docker//repositories:repositories.bzl",
    container_repositories = "repositories",
)
load(
    "@io_bazel_rules_docker//cc:image.bzl",
    _cc_image_repos = "repositories",
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
        third_party_http_archive(
            name = "com_github_emp_toolkit_emp_ot",
            build_file = "@mpc_utils//third_party:emp_ot.BUILD",
            link_files = {
                "@mpc_utils//third_party/emp-tool:FindBoost.cmake": "cmake/FindBoost.cmake",
            },
            sha256 = "8cc43964b0d1880429acf6dd972f1e080923c71d25127e74607f628b7cdc6c30",
            strip_prefix = "emp-ot-90319da396e731d0b24e90e1ccaa3b76fa73f641",
            url = "https://github.com/adriagascon/emp-ot/archive/90319da396e731d0b24e90e1ccaa3b76fa73f641.zip",
        )
    mpc_utils_deps(enable_oblivc = False)

    container_repositories()
    _cc_image_repos()

    # Something needs a recent GlibC, which is not included in standard distroless images.
    container_pull(
        name = "distroless_base",
        digest = "sha256:6d25761ba94c2b94db2dbea59390eaca65ff39596d64ec940d74140e1dc8872a",
        registry = "index.docker.io",
        repository = "schoppmp/distroless-arch",
    )
