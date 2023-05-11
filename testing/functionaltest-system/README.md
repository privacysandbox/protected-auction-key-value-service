# Privacy Sandbox Functional Testing Tools

This repository contains [Bazel Starlark Extentions](https://bazel.build/extending/concepts) and
tools used by the [Privacy Sandbox](https://github.com/privacysandbox) open-source ecosystem for
functional testing.

The bazel extensions provide support for testing RPC endpoints, using `bazel test` as the test
runner.

## Features at a glance

-   Testing RPC endpoints using either gRPC or HTTP
-   Declarative, "code-free" approach to integration, functional or load testing
-   diff testing of RPC endpoints
-   load testing of RPC endpoints

## Getting started

### Prerequisites

The `functionaltest-system` has been tested using Bazel 5.x. Check your Bazel version by running
`bazel --version`.

### Testing the functional test suite

To run the tests in this repo, you will need to a few tools available in your environment:

-   [golang](https://go.dev/) compiler &mdash we tested with golang v1.19
-   C++ compiler &mdash; we tested with clang v14
-   [Docker Compose v2](https://github.com/docker/compose#where-to-get-docker-compose)

Note: These dependencies are used specifically to build or run the example gRPC servers for use as
test subjects. If you do not run tests of this repo itself, there is no need to install any of these
tools.

### Creating a gRPC functional test suite

Usage of this repo is demonstrated in a workspace in the `examples/grpc_greeter` directory.

#### Request and reply

For a given gRPC functional test, a request and a reply will need to be specified using the format
`<test-name>.request.json` and `<test-name>.reply.json`.

#### jq filters

[jq](https://stedolan.github.io/jq) filters can be configured to run at different points in the
lifecycle of a functional test.

##### Pre-filter

Executed on the gRPC request before the functional test is invoked. The filter can be specified
using the format `<test-name>.pre-filter.jq`.

##### Post-filter

Executed on the response of the gRPC request before a diff test is performed. The filter can be
specified using the format `<test-name>.filter.jq`.

Post-filters can also be configured to use the jq `--slurp` option, which dictates that the entire
input stream be read into memory before running the filter. To use the slurp option with a
post-filter, the file needs to use the format `<test-name>.filter.slurp.jq`.

## Configuring the WORKSPACE

Add the following snippet to your `WORKSPACE` file to declare this repo as an external dependency of
your bazel workspace.

Note: The `http_archive` repository rule is one approach for defining the
`google_privacysandbox_functionaltest_system` external dependency. The entry below is incomplete and
will not work as-is. Instead, please contact the Potassium EngProd team
(<potassium-engprod@google.com>) for specific assistance prior to the availability of this repo on
GitHub.

```python
workspace(name = "functionaltest_system_grpc_greeter_example")

http_archive(
    name = "google_privacysandbox_functionaltest_system",
    urls = ["..."],
)

load(
    "@google_privacysandbox_functionaltest_system//:deps.bzl",
    functest_dependencies = "dependencies",
)

functest_dependencies()

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")

bazel_skylib_workspace()
```
