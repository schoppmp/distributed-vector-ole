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
            urls = ["https://github.com/abseil/abseil-cpp/archive/ccdd1d57b6386ebc26fb0c7d99b604672437c124.zip"],
            strip_prefix = "abseil-cpp-ccdd1d57b6386ebc26fb0c7d99b604672437c124",
            sha256 = "a463a791e1b5eaad461956495401357efb792fcdbf47b5737ec420bb54c804b6",
        )
    if "com_github_nelhage_rules_boost" not in native.existing_rules():
        http_archive(
            name = "com_github_nelhage_rules_boost",
            url = "https://github.com/nelhage/rules_boost/archive/a1dd05e7e9178f8aad86e39f3a5b377902eae5b2.zip",
            sha256 = "f9afc8e2d3ef2cca277767745af02666d139dd285d0b820d0ce92238a457bac4",
            strip_prefix = "rules_boost-a1dd05e7e9178f8aad86e39f3a5b377902eae5b2",
        )
    if "com_github_schoppmp_rules_oblivc" not in native.existing_rules():
        http_archive(
            name = "com_github_schoppmp_rules_oblivc",
            sha256 = "bd338f66667cc959d32ed9a048b7d0b9d3d8f554fd36a7bc6b7102a4a355ab30",
            url = "https://github.com/schoppmp/rules_oblivc/archive/99aeebf6732f209aa85af8b069220e18082b92fe.zip",
            strip_prefix = "rules_oblivc-99aeebf6732f209aa85af8b069220e18082b92fe",
        )
    if "rules_foreign_cc" not in native.existing_rules():
        http_archive(
            name = "rules_foreign_cc",
            url = "https://github.com/bazelbuild/rules_foreign_cc/archive/16ddc00bd4e1b3daf3faee1605a168f5283326fa.zip",
            strip_prefix = "rules_foreign_cc-16ddc00bd4e1b3daf3faee1605a168f5283326fa",
            sha256 = "54ef1b6a31f7cd0f1c707efb1dc670ab86d2d7238af845108f31ed9e6d0fdf01",
        )

    # Transitive dependency of io_bazel_rules_docker
    if "bazel_skylib" not in native.existing_rules():
        skylib_version = "0.8.0"
        http_archive(
            name = "bazel_skylib",
            type = "tar.gz",
            url = "https://github.com/bazelbuild/bazel-skylib/releases/download/{}/bazel-skylib.{}.tar.gz".format(skylib_version, skylib_version),
            sha256 = "2ef429f5d7ce7111263289644d233707dba35e39696377ebab8b0bc701f7818e",
        )
    if "bazel_gazelle" not in native.existing_rules():
        http_archive(
            name = "bazel_gazelle",
            sha256 = "3c681998538231a2d24d0c07ed5a7658cb72bfb5fd4bf9911157c0e9ac6a2687",
            urls = ["https://github.com/bazelbuild/bazel-gazelle/releases/download/0.17.0/bazel-gazelle-0.17.0.tar.gz"],
        )
    if "io_bazel_rules_go" not in native.existing_rules():
        http_archive(
            name = "io_bazel_rules_go",
            sha256 = "f04d2373bcaf8aa09bccb08a98a57e721306c8f6043a2a0ee610fd6853dcde3d",
            urls = [
                "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/0.18.6/rules_go-0.18.6.tar.gz",
                "https://github.com/bazelbuild/rules_go/releases/download/0.18.6/rules_go-0.18.6.tar.gz",
            ],
        )

    # New dependencies.
    if "mpc_utils" not in native.existing_rules():
        http_archive(
            name = "mpc_utils",
            url = "https://github.com/schoppmp/mpc-utils/archive/2022b8d8162e450434e72d036118af59abe74b02.zip",
            sha256 = "a419392c9c5c0e12185929f84abf4f150936fa6437c9fe8c134bddfcd7a56e0b",
            strip_prefix = "mpc-utils-2022b8d8162e450434e72d036118af59abe74b02",
        )
    if "io_bazel_rules_docker" not in native.existing_rules():
        http_archive(
            name = "io_bazel_rules_docker",
            sha256 = "87fc6a2b128147a0a3039a2fd0b53cc1f2ed5adb8716f50756544a572999ae9a",
            strip_prefix = "rules_docker-0.8.1",
            urls = ["https://github.com/bazelbuild/rules_docker/archive/v0.8.1.tar.gz"],
        )
