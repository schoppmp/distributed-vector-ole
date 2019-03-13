load(
    "@bazel_tools//tools/build_defs/repo:http.bzl",
    "http_archive",
)
load("@mpc_utils//mpc_utils:deps.bzl", "mpc_utils_deps")

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
    mpc_utils_deps(enable_oblivc = False)

    if "com_github_relic_toolkit_relic" not in native.existing_rules():
        http_archive(
            name = "com_github_relic_toolkit_relic",
            url = "https://github.com/relic-toolkit/relic/archive/b984e901ba78c83ea4093ea96addd13628c8c2d0.zip",
            sha256 = "34d66ea3e08e7b9496452c32941b7bc0e4c620d11f3f373d07e9ba1a2606f6ad",
            strip_prefix = "relic-b984e901ba78c83ea4093ea96addd13628c8c2d0",
            build_file_content = all_content,
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

    if "com_github_emp_toolkit_emp_tool" not in native.existing_rules():
        http_archive(
            name = "com_github_emp_toolkit_emp_tool",
            url = "https://github.com/emp-toolkit/emp-tool/archive/52495abff24d2510c203188c4953576590740dec.zip",
            sha256 = "198a2f67dc4de2157c22b87bf0f5df571cf1004bbf72aafeaeab5f11c263a7d5",
            strip_prefix = "emp-tool-52495abff24d2510c203188c4953576590740dec",
            build_file_content = all_content,
        )

    if "com_github_emp_toolkit_emp_ot" not in native.existing_rules():
        http_archive(
            name = "com_github_emp_toolkit_emp_ot",
            url = "https://github.com/adriagascon/emp-ot/archive/592c385e94f44fc942382b7c3d4b9f91ed84f33f.zip",
            sha256 = "bbe48c5414cdcc8dc23bce7dcca3c3c0c8a0a8c557110bea0615537af2cfc5cb",
            strip_prefix = "emp-ot-592c385e94f44fc942382b7c3d4b9f91ed84f33f",
            build_file = clean_dep("//third_party/emp:emp_ot.BUILD"),
        )
