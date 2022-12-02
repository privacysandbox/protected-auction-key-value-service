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

Alternatively, you can use `docker buildx build` to create the image using a command like this:

```sh
tar --create --dereference --gzip --directory=images/build-debian . | \
  docker buildx build - --tag privacysandbox/builders/build-debian:latest`
```
