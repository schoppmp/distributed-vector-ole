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
            urls = ["https://github.com/abseil/abseil-cpp/archive/7c7754fb3ed9ffb57d35fe8658f3ba4d73a31e72.zip"],  # 2019-03-14
            strip_prefix = "abseil-cpp-7c7754fb3ed9ffb57d35fe8658f3ba4d73a31e72",
            sha256 = "71d00d15fe6370220b6685552fb66e5814f4dd2e130f3836fc084c894943753f",
        )
    if "com_github_nelhage_rules_boost" not in native.existing_rules():
        http_archive(
            name = "com_github_nelhage_rules_boost",
            url = "https://github.com/nelhage/rules_boost/archive/417642961150e987bc1ac78c7814c617566ffdaa.zip",
            sha256 = "b24dd149c0cc9f7ff5689d91f99aaaea0d340baf9911439b573f02074148533a",
            strip_prefix = "rules_boost-417642961150e987bc1ac78c7814c617566ffdaa",
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
            url = "https://github.com/bazelbuild/rules_foreign_cc/archive/8ccd83504b2221b670fc0b83d78fcee5642f4cb1.zip",
            strip_prefix = "rules_foreign_cc-8ccd83504b2221b670fc0b83d78fcee5642f4cb1",
            sha256 = "e5e8289b236bf57cfed2e76a20756ddff90fec7c8a4633e6a028e65899ecb6c7",
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
            url = "https://github.com/schoppmp/mpc-utils/archive/1b97fad04b78d9d3e0b6780a4b782b4c017ebe5c.zip",
            sha256 = "8d81c56cfdbd757d5e4b4885bc0dff9c7b57f44c8b439bc340e61ccf24c48a87",
            strip_prefix = "mpc-utils-1b97fad04b78d9d3e0b6780a4b782b4c017ebe5c",
        )
    if "io_bazel_rules_docker" not in native.existing_rules():
        http_archive(
            name = "io_bazel_rules_docker",
            sha256 = "87fc6a2b128147a0a3039a2fd0b53cc1f2ed5adb8716f50756544a572999ae9a",
            strip_prefix = "rules_docker-0.8.1",
            urls = ["https://github.com/bazelbuild/rules_docker/archive/v0.8.1.tar.gz"],
        )
