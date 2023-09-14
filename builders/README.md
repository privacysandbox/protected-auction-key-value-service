# Privacy Sandbox Builders

Build tools and docker images used by the [Privacy Sandbox](https://github.com/privacysandbox)
open-source ecosystem. The docker images generated using the Privacy Sandbox Builders are used as
the build environment(s) for other Privacy Sandbox software. These docker images can be used as part
of CI/CD workflows.

## Getting Started

This repo is designed to be used via `git submodule` by other Privacy Sandbox repos. This is not a
requirement per se, it can be used standalone or using other approaches like `git subtree`.

To use Privacy Sandbox Builders, you need:

-   `docker` with [BuildKit](https://docs.docker.com/build/buildkit/).

## Building Docker Images

To build a docker image directly, you can use the `tools/get-builder-image-tagged` tool.

## Tools

The `tools` directory contains wrapper scripts along with some non-wrapper scripts. The wrapper
scripts, such as `terraform` and `curl` execute the corresponding tool installed in one of the build
images.

For example, to execute `curl`:

```sh
tools/curl --help
```

---

[![OpenSSF Scorecard](https://api.securityscorecards.dev/projects/github.com/privacysandbox/build-system/badge)](https://securityscorecards.dev/viewer/?uri=github.com/privacysandbox/build-system)
