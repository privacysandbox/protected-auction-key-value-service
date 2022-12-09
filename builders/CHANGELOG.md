# Changelog

All notable changes to this project will be documented in this file. See [commit-and-tag-version](https://github.com/absolute-version/commit-and-tag-version) for commit guidelines.

## [0.5.0](https://team/kiwi-air-force-eng-team/build-system/compare/release-0.4.4...release-0.5.0) (2022-12-06)


### Features

* Pin versions in presubmit image


### Bug Fixes

* Avoid cache for tar image
* Update version pin for ca-certificates
* Use images subdirs for image list

### [0.4.4](https://team/kiwi-air-force-eng-team/build-system/compare/release-0.4.3...release-0.4.4) (2022-11-18)


### Bug Fixes

* Retain execute permissions when normalizing dist

### [0.4.3](https://team/kiwi-air-force-eng-team/build-system/compare/release-0.4.2...release-0.4.3) (2022-11-17)


### Bug Fixes

* Add gettext package for envsubst
* Avoid yum package version strings specific to CPU architecture
* Improve verbose output for get-builder-image-tagged
* Pin apt and yum package versions

### [0.4.2](https://team/kiwi-air-force-eng-team/build-system/compare/release-0.4.1...release-0.4.2) (2022-11-17)


### Bug Fixes

* Generate SHA within docker container

### [0.4.1](https://team/kiwi-air-force-eng-team/build-system/compare/release-0.4.0...release-0.4.1) (2022-11-15)


### Bug Fixes

* Reduce noise creating presubmit image
* Remove docker run --interactive flag

## [0.4.0](https://team/kiwi-air-force-eng-team/build-system/compare/release-0.3.1...release-0.4.0) (2022-11-14)


### Features

* Add buildozer to release image
* Add grpcurl helper script
* Substitute variables in EXTRA_DOCKER_RUN_ARGS
* Use specific version of GNU tar


### Bug Fixes

* Add /opt/bin/python link
* Add CC=clang env var
* Add xz to build-debian
* Explicitly add machine type and OS release to toolchains hash

### [0.3.1](https://team/kiwi-air-force-eng-team/build-system/compare/release-0.3.0...release-0.3.1) (2022-11-01)


### Bug Fixes

* Add OpenJDK 11 in build-amazonlinux2

## [0.3.0](https://team/kiwi-air-force-eng-team/build-system/compare/release-0.2.0...release-0.3.0) (2022-11-01)


### Features

* Add git to build-amazonlinux2 image
* Add google-java-format pre-commit hook
* Add OpenJDK 11
* Move python3 to /opt/bin/python3


### Bug Fixes

* Ensure builder::set_workspace does not overwrite WORKSPACE

## [0.2.0](https://team/kiwi-air-force-eng-team/build-system/compare/release-0.1.0...release-0.2.0) (2022-10-26)


### Features

* Add bazel support to register container-based python toolchain
* Add docker uri env var as override
* Add tools/terraform
* Use image etc files in workspace root


### Bug Fixes

* Avoid empty tar error if no workspace etc files


### Documentation

* Add Getting Started section to README.md

## 0.1.0 (2022-10-25)


### Features

* Add --env flag to cbuild
* Add arg processing to cbuild script
* Add aws-cli helper script
* Add bazel-debian helper script
* Add builders/utils docker image
* Add hadolint to lint Dockerfiles
* Add optional flags to cbuild tool
* Add preliminary support for commit-and-tag-version and copybara
* Add the release-please tool
* Add toolchain short hash to bazel output_user_root path
* Add tools/lib/builder.sh
* **build:** Add GitHub CLI tool https://cli.github.com/
* Determine workspace mount point from docker inspect if inside docker container
* Inject clang-version as a bazel action_env
* Migrate generation of builders/release container image to Dockerfile
* Move image directories to images/ top-level directory
* Overhaul building on amazonlinux2
* Remove python build dependencies from bazel
* Set BUILD_ARCH env var in docker images
* Update release image to node v18
* Upgrade to bazel 5.3.2
* Upgrade to clang v14 on bazel-debian
* Use Packer to build AMI.


### Bug Fixes

* Add builders/tools/normalize-dist to chmod/chgrp/chown dist/ directory tree
* Add get_workspace_mount function to encapsulate code block
* Add python version to action_env
* Add/remove basic pre-commit hooks
* Adopt shellcheck
* Avoid installing recommended debian packages
* Avoid use of git rev-parse to determine tools path
* Bump to latest version of bazelisk
* Clean bazel_root for smaller docker image
* Correct argument handling in cbuild script
* Correct errors generating Amazon Linux 2-based builder image
* Define python3 toolchain
* Drop packer from build-debian image
* Ensure /etc/gitconfig is readable by all
* Improve cbuild help text
* Install bazel version as specified in .bazelversion
* Invoke addlicense for all text files
* Modifications as indicated by shellcheck
* Mount $HOME/aws in aws-cli container
* Move bazel env vars from comments to help text
* Move builder-related configs to builders/etc
* Move WORKSPACE definition to cbuild script global
* Only propagate AWS env vars into amazonlinux2 build container
* Pin version of bazelisk
* Pin version of libc++-dev
* Pin version of python3.8
* Print pre-commit version rather than help
* Remove container when get-architecture exits
* Remove debugging statement
* Remove dockerfile linter ignore and correct ENTRYPOINT
* Remove pre-commit config from build-debian
* Remove shellcheck from build-debian
* Remove unused nitro_enclave_image bazel rule
* Set bazel output_base to accommodate distinct workspaces
* Set bazel output_user_root in image bazelrc
* Set locale in build-debian
* Set WORKSPACE correctly from submodule
* Set WORKSPACE variable
* Switch from hardcoded arch to using dpkg --print-architecture
* Update normalize-dist to function inside build container
* Update pre-commit to use cbuild
* Use PRE_COMMIT_TOOL env var
* Various path-related fixes


### Build System

* Add arch to docker image tags
* Add get_builder_image_tagged tool to determine a content-based tag
* Add get-architecture helper script
* Add missing imports into nitro BUILD
* Add tools/pre-commit
* Correct propagation of quoted args in gh wrapper
* Move commit-and-tag-version into tools dir
* Move gh into tools dir
* Optionally build the AMI
* Propagate status code in exit functions
* Reduce redundant installation commands
* Remove release-please tool
* Rename builders/bazel to build-debian
* Simplify use of npm image in container_run_and_commit()
* Support GH_TOKEN env var


### Documentation

* Add top-level README.md
* Improve prose in README.md
* Move docker build instructions to README.md
* Reformat and lint markdown
