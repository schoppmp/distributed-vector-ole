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

# Dependencies that need to be defined before :deps.bzl can be loaded.
# Copy those in a similar preload.bzl file in any workspace that depends on
# this one.
def distributed_vector_ole_preload():
    # Transitive dependencies of mpc_utils.
    if "com_google_absl" not in native.existing_rules():
        http_archive(
            name = "com_google_absl",
            sha256 = "fbfdd54308759ec1465d387dda21a768285311845554da6ada5ffb5de7c4c08b",
            strip_prefix = "abseil-cpp-af8f994af5d25d9bebf542d6e6bf60edc40fd25d",
            urls = ["https://github.com/abseil/abseil-cpp/archive/af8f994af5d25d9bebf542d6e6bf60edc40fd25d.zip"],
        )
        http_archive(
            name = "com_github_nelhage_rules_boost",
            sha256 = "ea88159d5b91a852de0cd8ccdc78c9ca42c54538241aa7ff727de3544da7f051",
            strip_prefix = "rules_boost-fe9a0795e909f10f2bfb6bfa4a51e66641e36557",
            url = "https://github.com/nelhage/rules_boost/archive/fe9a0795e909f10f2bfb6bfa4a51e66641e36557.zip",
        )
    if "com_github_schoppmp_rules_oblivc" not in native.existing_rules():
        http_archive(
            name = "com_github_schoppmp_rules_oblivc",
            sha256 = "bd338f66667cc959d32ed9a048b7d0b9d3d8f554fd36a7bc6b7102a4a355ab30",
            url = "https://github.com/schoppmp/rules_oblivc/archive/99aeebf6732f209aa85af8b069220e18082b92fe.zip",
            strip_prefix = "rules_oblivc-99aeebf6732f209aa85af8b069220e18082b92fe",
        )
        http_archive(
            name = "rules_foreign_cc",
            sha256 = "63e285e86a380b993f27fc50f7c6af200e78243e167f52b16aadec80ab8ff06a",
            strip_prefix = "rules_foreign_cc-879846e228fd70f3b3fef0de4f6baa0b29730c22",
            url = "https://github.com/schoppmp/rules_foreign_cc/archive/879846e228fd70f3b3fef0de4f6baa0b29730c22.zip",
        )

    # Transitive dependency of io_bazel_rules_docker
    if "bazel_skylib" not in native.existing_rules():
        http_archive(
            name = "bazel_skylib",
            sha256 = "2ea8a5ed2b448baf4a6855d3ce049c4c452a6470b1efd1504fdb7c1c134d220a",
            strip_prefix = "bazel-skylib-0.8.0",
            urls = ["https://github.com/bazelbuild/bazel-skylib/archive/0.8.0.tar.gz"],
        )
    if "bazel_gazelle" not in native.existing_rules():
        http_archive(
            name = "bazel_gazelle",
            urls = [
                "https://storage.googleapis.com/bazel-mirror/github.com/bazelbuild/bazel-gazelle/releases/download/0.18.2/bazel-gazelle-0.18.2.tar.gz",
                "https://github.com/bazelbuild/bazel-gazelle/releases/download/0.18.2/bazel-gazelle-0.18.2.tar.gz",
            ],
            sha256 = "7fc87f4170011201b1690326e8c16c5d802836e3a0d617d8f75c3af2b23180c4",
        )
    if "io_bazel_rules_go" not in native.existing_rules():
        http_archive(
            name = "io_bazel_rules_go",
            urls = [
                "https://storage.googleapis.com/bazel-mirror/github.com/bazelbuild/rules_go/releases/download/0.19.4/rules_go-0.19.4.tar.gz",
                "https://github.com/bazelbuild/rules_go/releases/download/0.19.4/rules_go-0.19.4.tar.gz",
            ],
            sha256 = "ae8c36ff6e565f674c7a3692d6a9ea1096e4c1ade497272c2108a810fb39acd2",
        )

    # New dependencies.
    if "mpc_utils" not in native.existing_rules():
        http_archive(
            name = "mpc_utils",
            sha256 = "4180384dfd804e67023e028e40c893068247b86ae8e45e59e3686a986defb4e3",
            strip_prefix = "mpc-utils-99b3b7e4fa596cd3b3dd5ef8856abbc0780533ed",
            url = "https://github.com/schoppmp/mpc-utils/archive/99b3b7e4fa596cd3b3dd5ef8856abbc0780533ed.zip",
        )
    if "io_bazel_rules_docker" not in native.existing_rules():
        http_archive(
            name = "io_bazel_rules_docker",
            sha256 = "9ff889216e28c918811b77999257d4ac001c26c1f7c7fb17a79bc28abf74182e",
            strip_prefix = "rules_docker-0.10.1",
            urls = ["https://github.com/bazelbuild/rules_docker/releases/download/v0.10.1/rules_docker-v0.10.1.tar.gz"],
        )
