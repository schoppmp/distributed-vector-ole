# Distributed Vector-OLE [![Build Status](https://travis-ci.org/schoppmp/distributed-vector-ole.svg?branch=master)](https://travis-ci.org/schoppmp/distributed-vector-ole)

A two-party generator for Vector-OLE correlations.
To build, first install the required dependencies:
```
# Arch Linux
sudo pacman -Sy base-devel bazel git python python2 cmake boost

# Ubuntu 18.10
sudo apt-get install build-essential git openjdk-11-jdk gnupg2 curl patch m4 cmake libboost-system-dev
echo "deb [arch=amd64] https://storage.googleapis.com/bazel-apt stable jdk1.8" | sudo tee /etc/apt/sources.list.d/bazel.list
curl https://bazel.build/bazel-release.pub.gpg | sudo apt-key add -
sudo apt-get update && sudo apt-get install bazel
```
Then, run
```
bazel build ...
```
to build, and 
```
bazel test ...
```
to test.
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
A usage example can be found in the [example](example) folder.