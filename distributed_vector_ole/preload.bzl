load(
    "@bazel_tools//tools/build_defs/repo:http.bzl",
    "http_archive",
)

# Dependencies that need to be defined before :deps.bzl can be loaded.
# Copy those in a similar preload.bzl file in any workspace that depends on
# this one.
def distributed_vector_ole_preload():
    # Transitive dependencies of mpc_utils.
    if "com_github_nelhage_rules_boost" not in native.existing_rules():
        http_archive(
            name = "com_github_nelhage_rules_boost",
            url = "https://github.com/nelhage/rules_boost/archive/691a53dd7dc4fb8ab70f61acad9b750a1bf10dc6.zip",
            sha256 = "5837d6bcf96c60dc1407126e828287098f91a8c69d8c2ccf8ebb0282ed35b401",
            strip_prefix = "rules_boost-691a53dd7dc4fb8ab70f61acad9b750a1bf10dc6",
        )
    if "com_github_schoppmp_rules_oblivc" not in native.existing_rules():
        http_archive(
            name = "com_github_schoppmp_rules_oblivc",
            sha256 = "52a1370d6ee47d62791a8a3b61f0ef0c93d3465d774f41fa5516c1e2f3541e0d",
            url = "https://github.com/schoppmp/rules_oblivc/archive/14282988a60fbaa1aa428b1678e07b9d94b8be62.zip",
            strip_prefix = "rules_oblivc-14282988a60fbaa1aa428b1678e07b9d94b8be62",
        )
    if "rules_foreign_cc" not in native.existing_rules():
        http_archive(
            name = "rules_foreign_cc",
            url = "https://github.com/bazelbuild/rules_foreign_cc/archive/8648b0446092ef2a34d45b02c8dc4c35c3a8df79.zip",
            strip_prefix = "rules_foreign_cc-8648b0446092ef2a34d45b02c8dc4c35c3a8df79",
            sha256 = "20b47a5828d742715e1a03a99036efc5acca927700840a893abc74480e91d4f9",
        )

    # New dependencies.
    if "mpc_utils" not in native.existing_rules():
        http_archive(
            name = "mpc_utils",
            url = "https://github.com/schoppmp/mpc-utils/archive/b36649ef92518268fb21fb6a62e81cdd7a16d17d.zip",
            sha256 = "9f1cdc1d9b9c476fd0954591d1b83d00f179d5fa7eb54c19408613b1b5b60cca",
            strip_prefix = "mpc-utils-b36649ef92518268fb21fb6a62e81cdd7a16d17d",
        )
