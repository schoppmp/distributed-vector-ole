load(
    "@bazel_tools//tools/build_defs/repo:http.bzl",
    "http_archive",
)
load("@mpc_utils//mpc_utils:deps.bzl", "mpc_utils_deps")
load(
    "@rules_foreign_cc//:workspace_definitions.bzl",
    "rules_foreign_cc_dependencies",
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
    mpc_utils_deps(enable_oblivc = False)

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
            url = "https://github.com/google/benchmark/archive/df7c7ee1d37dda0fb597586b4624515166a778d0.zip",
            sha256 = "7f41a125c859da0115f144fb228e9b0a9ba404aeca76e5ca51e0bfe250cc0bb5",
            strip_prefix = "benchmark-df7c7ee1d37dda0fb597586b4624515166a778d0",
        )
