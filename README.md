# Distributed Vector-OLE

A two-party generator for Vector-OLE correlations. The only dependency is [Bazel](https://bazel.build), all other dependencies will be built from source.
To build and test, run
```
$ bazel build ...
$ bazel test ...
```
To use this library in your own code, include it in your WORKSPACE file like this:
```
load(
    "@bazel_tools//tools/build_defs/repo:http.bzl",
    "http_archive",
)

http_archive(
    name = "distributed_vector_ole",
    strip_prefix = "distributed-vector-ole-master",
    url = "https://github.com/schoppmp/distributed-vector-ole/archive/master.zip",
)
```
An usage example can be found in the [example](example) folder.