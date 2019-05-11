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
            url = "https://github.com/bazelbuild/rules_foreign_cc/archive/3831409fd50e77a4c029bc42b2c77eb294f5e0f5.zip",
            strip_prefix = "rules_foreign_cc-3831409fd50e77a4c029bc42b2c77eb294f5e0f5",
            sha256 = "2573cee302935fb7de01a2b766ac808ba6cc2e3f318ea48bf398bf51299b344b",
        )

    # New dependencies.
    if "mpc_utils" not in native.existing_rules():
        http_archive(
            name = "mpc_utils",
            url = "https://github.com/schoppmp/mpc-utils/archive/57b9ca4907aa59849ea348dadd51bb6a48c33c30.zip",
            sha256 = "7bf04fbaa1a91a9cea09a87775e848e5192b3a97eccf2fa718f2b638f30e8153",
            strip_prefix = "mpc-utils-57b9ca4907aa59849ea348dadd51bb6a48c33c30",
        )
