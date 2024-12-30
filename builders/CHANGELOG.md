# Changelog

All notable changes to this project will be documented in this file. See [commit-and-tag-version](https://github.com/absolute-version/commit-and-tag-version) for commit guidelines.

## 0.76.0 (2024-12-16)


### Features

* Remove support for Amazon Linux 2

## 0.75.1 (2024-11-22)


### Bug Fixes

* Revert to ubuntu:20

## 0.75.0 (2024-11-21)


### Features

* Add gVisor dependencies to build-debian image

## 0.74.0 (2024-11-21)


### Features

* Add cbuild flag --one-time to create a new container
* Print message when reusing a cbuild container

## 0.73.0 (2024-10-31)


### Features

* Create .clang.bazelrc


### Dependencies

* **deps:** Upgrade clang to v18

## 0.72.0 (2024-10-23)


### Features

* Update build-debian image to Ubuntu 22.04 LTS

## 0.71.0 (2024-10-18)


### Dependencies

* **deps:** Pin AmazonLinux2023 Nitro CLI versions
* **deps:** Update AmazonLinux base images to 2024-10-01
* **deps:** Upgrade pre-commit and pylint

## 0.70.0 (2024-10-10)


### Features

* add awk, crane and jq to build-* images for the oci_pull credential helper

## 0.69.1 (2024-09-19)

### Bug Fixes

* Use digest to ensure deterministic environment for build system


## 0.69.0 (2024-09-15)


### Features

* Pin build-debian ubuntu base image from 20.04 to focal-20240530

## 0.68.1 (2024-08-21)

### Bug Fixes

* Fix load bazel_tools import for Python deps

## 0.68.0 (2024-08-21)


### Features

* **deps:** Split python deps and registering toolchains
* **deps:** Update rules_python to 0.35.0

## 0.67.0 (2024-07-31)


### Bug Fixes

* Add EXTRA_CBUILD_ARGS to tools/bazel-* scripts


### Dependencies

* **deps:** Update buildozer to 6.1.1
* **deps:** Upgrade amazonlinux2023 to 5.20240722.0

## 0.66.1 (2024-06-24)


### Bug Fixes

* Add --compilation_mode=opt to build:profiler config

## 0.66.0 (2024-06-20)


### Features

* Add cpu-profiler flags to cbuild
* Add profiler config in .profiler.bazelrc

## 0.65.1 (2024-06-04)


### Bug Fixes

* Support multiple etc files in a single image

## 0.65.0 (2024-06-04)


### Features

* Add DOCKER_NETWORK env var for test-tools

## 0.64.1 (2024-05-29)


### Bug Fixes

* Support container reuse when --cmd not specified
* Use find to identify bazel symlinks

## 0.64.0 (2024-05-27)


### Features

* Support cmd-profiler mode with/without --cmd


### Bug Fixes

* cbuild should find container with exact name match
* Ensure normalize-bazel-symlinks is in the workspace dir


### Dependencies

* **deps:** Upgrade clang-format pre-commit hook

## 0.63.0 (2024-05-26)


### Features

* Support cmd-profiler mode with/without --cmd


### Bug Fixes

* Ensure normalize-bazel-symlinks is in the workspace dir


### Dependencies

* **deps:** Upgrade clang-format pre-commit hook

## 0.62.0 (2024-05-10)


### Features

* Add --dir flag to normalize-dist

## 0.61.1 (2024-05-10)


### Bug Fixes

* Add docker flags to container name
* Set 8h ttl for long-running build container

## 0.61.0 (2024-05-08)


### Features

* Add cbuild support for container reuse

## 0.60.0 (2024-05-07)


### Dependencies

* **deps:** Upgrade coverage-tools to ubuntu 24.04
* **deps:** Upgrade golang to 1.22.2

## 0.59.0 (2024-05-02)


### Bug Fixes

* **deps:** Update pre-commit hooks


### Dependencies

* **deps:** Upgrade alpine base image
* **deps:** Upgrade base images for Amazon Linux
* **deps:** Upgrade grpcurl to 1.9.1
* **deps:** Upgrade presubmit to ubuntu 24.04

## 0.58.0 (2024-04-26)


### Features

* add missing AWS env variable for CodeBuild

## 0.57.1 (2024-03-28)


### Bug Fixes

* Upgrade OpenSSF scorecard GitHub Action

## 0.57.0 (2024-03-10)


### Features

* Add a generic pylintrc
* Add clang-tidy to build-debian

## 0.56.0 (2024-02-29)


### Features

* Add pylint to presubmit


### Bug Fixes

* Clean bazel build and mod caches
* Pin okigan/awscurl to v0.29

## 0.55.2 (2024-02-23)


### Bug Fixes

* Add gmock to .clang-format

## 0.55.1 (2024-02-22)


### Bug Fixes

* Do not invoke normalize-bazel-symlinks for cbuild --cmd

## 0.55.0 (2024-02-22)


### Bug Fixes

* Normalize bazel symlinks to a resolved path
* Pass the correct path for normalize-bazel-symlinks

## 0.54.0 (2024-02-09)


### Features

* Set cbuild workdir to pwd relative to root workspace

## 0.53.0 (2024-01-25)


### Features

* Add support to collect-coverage tool for custom lcov report


### Bug Fixes

* Improve --cmd-profiler support

## 0.52.0 (2023-12-02)


### Features

* add python3.9 dev to bazel-debian

## 0.51.0 (2023-11-30)


### Bug Fixes

* Clean go build cache at the end of image build script


### Dependencies

* **deps:** Upgrade bazelisk to 1.19.0

## 0.50.0 (2023-11-06)


### Features

* Add openssh-client to build-debian image

## 0.49.1 (2023-10-30)


### Bug Fixes

* Add tools/wrk2 wrapper script

## 0.49.0 (2023-10-27)


### Features

* Add wrk2 to test-tools image
* Extend normalize-bazel-symlink to normalize within containers


### Dependencies

* **deps:** Update versions in test-tools image

## 0.48.0 (2023-10-11)


### Features

* Add tools/bazel-amazonlinux2023


### Bug Fixes

* Add lldb symlink

## 0.47.0 (2023-10-05)


### Features

* Add --cmd-profiler flag to tools/cbuild
* Add google-pprof to coverage-tools image
* Add libprofiler.so to build-debian image


### Bug Fixes

* Indicate default image in help text

## 0.46.1 (2023-10-04)


### Bug Fixes

* cbuild exits normally even if bazel symlinks aren't normalized
* Revert to clang 15.x
* Use bash when normalize-dist runs inside docker
* Use DOCKER_NETWORK if already set

## 0.46.0 (2023-09-22)


### Bug Fixes

* Hide normalize-bazel-symlinks in cbuild


### Dependencies

* **deps:** Update amazonlinux images to Aug 2023
* **deps:** Upgrade clang to v16 in build-debian

## 0.45.0 (2023-09-19)


### Features

* Invoke normalize-bazel-symlinks at cbuild exit

## 0.44.0 (2023-09-11)


### Features

* Add coverage bazel config and collect-coverage tool

## 0.43.0 (2023-08-15)


### Features

* Allow terraform version to be specified


### Bug Fixes

* Move feat(deps) earlier for precedence over feat

## 0.42.1 (2023-08-14)


### Bug Fixes

* Revert override of /usr/bin/python links in build-amazonlinux2

## 0.42.0 (2023-08-07)


### Features

* Add amazonlinux2 support to convert-docker-to-nitro
* Add bsdmainutils for hexdump
* Add build-amazonlinux2023 image
* Add support for amazonlinux2023 in builder.sh
* Configure python3.9 for python/python3 links
* Remove jdk from build-amazonlinux2 image

### Dependencies

* **deps:** Upgrade amazonlinux2 to 20230719


### Bug Fixes

* Empty bazel-* arg list not an error condition

## 0.41.1 (2023-08-04)


### Bug Fixes

* Remove debug statement

## 0.41.0 (2023-08-03)


### Features

* Create links for lld and ld.lld in build-debian

## 0.40.0 (2023-08-03)


### Features

* Add Dependencies section for release notes
* **deps:** Upgrade rules_python to 0.24.0
* Ensure bazel-* scripts handle non-bazel args too

## 0.39.0 (2023-08-02)


### Features

* Run bazel containers in seccomp=unconfined mode

## 0.38.1 (2023-08-02)


### Bug Fixes

* Ensure python3.9 is found first in PATH

## 0.38.0 (2023-07-28)


### Features

* Add cbuild flag --seccomp-unconfined

## 0.37.0 (2023-07-27)


### Features

* Add patch tool to build-debian image

## 0.36.1 (2023-07-25)


### Bug Fixes

* Rename convert_docker_to_nitro to convert-docker-to-nitro

## 0.36.0 (2023-07-24)


### Features

* Add convert_docker_to_nitro

## 0.35.0 (2023-07-21)


### Features

* Add LICENSE file

## 0.34.0 (2023-07-21)


### Features

* Add OSSF Scorecard badge to top-level README

## 0.33.0 (2023-07-20)


### Features

* Install python libclang 15 in build-debian
* Add OSSF Scorecard GitHub Action

## 0.32.0 (2023-07-14)


### Features

* Set PYTHON_BIN_PATH/PYTHON_LIB_PATH in build-amazonlinux2

## 0.31.0 (2023-07-12)


### Features

* Add cbuild --docker-network flag
* Add coverage-tool image plus lcov scripts
* Mount gcloud config dir into container


### Bug Fixes

* Add hash for coverage-tools
* Add hash for coverage-tools
* Improve error handling for flag values
* Print build log path on error condition
* Specify latest image tag explicitly
* Upgrade pre-commit hooks

## 0.30.1 (2023-06-27)


### Bug Fixes

* Use = for --env flag
* Use = for --env flag for all tools

## 0.30.0 (2023-06-26)


### Features

* Install numpy for python3.9
* Set PYTHON_BIN_PATH/PYTHON_LIB_PATH in build-debian
* Upgrade AmazonLinux2 to 20230530
* Upgrade packer to v1.9.1


### Bug Fixes

* Add links for llvm-{cov,profdata}

## 0.29.0 (2023-06-05)


### Features

* Update pre-commit hook versions


### Bug Fixes

* Catch error when shifting multiple args
* Remove golang from test-tools image
* Resolve WORKSPACE using realpath
* Use correct exit code in --fast mode

## 0.28.0 (2023-05-24)


### Features

* Update ca-certificates


### Bug Fixes

* Downgrade to clang v15
* Use builders version.txt for tarfile tag

## 0.27.0 (2023-05-23)


### Features

* Add buf to presubmit image


### Documentation

* Add CONTRIBUTING.md

## 0.26.0 (2023-05-16)


### Features

* Remove zlib-dev package
* Upgrade clang to v16
* Upgrade go to v1.20.4

## 0.25.0 (2023-05-11)


### Features

* Update default bazel version to 5.4.1
* Upgrade rules_python to 0.21.0


### Bug Fixes

* Add file utility to build-debian image

## 0.24.0 (2023-05-05)


### Features

* Add --build-images flag to tests/run-tests
* Reuse tar image if available


### Bug Fixes

* Address linter warnings
* Address linter warnings for tools
* Correct mangled usage text
* Pin pre-commit to 3.x
* Remove .gz suffix from tar file
* Remove function keyword for busybox sh script
* Remove Release in changelog title
* Upgrade pre-commit hooks

## 0.23.0 (2023-04-13)


### Features

* Add wrapper for commit-and-tag-version
* Upgrade curl to version 8
* Upgrade to amazonlinux 2.0.20230320.0


### Bug Fixes

* Use commit-and-tag-version wrapper

## 0.22.0 (2023-04-03)


### Features

* Add awscurl wrapper script
* Add tests for misc CLI wrappers
* Correctly quote bash args
* Extend test-tool to support the release image
* Use login shell for interactive container


### Documentation

* Add section on tools to README
* Remove section on building images directly

## 0.21.1 (2023-03-07)


### Bug Fixes

* Relax pinned version for apache2-utils

## 0.21.0 (2023-03-06)


### Features

* Add wrapper scripts for utils

## 0.20.0 (2023-03-01)


### Features

* Add docker buildkit plugin
* Add tests for CLI wrapper scripts
* Permit testing of a single image
* Relax pinned versions in build-debian, presubmit and test-tools

## 0.19.0 (2023-03-01)


### Features

* Add jq wrapper


### Bug Fixes

* Relax pinned version of openjdk to 11.0.*

## 0.18.0 (2023-02-23)


### Features

* Relax pinned versions for apk and yum packages to semver

## 0.17.0 (2023-02-21)


### Features

* Upgrade black to 23.1.0
* Upgrade tar to 1.34-r1
* Upgrade to buildozer 6.0.1


### Bug Fixes

* Minor code cleanup in images/presubmit/install_apps
* Upgrade ghz to 0.114.0

## 0.16.0 (2023-02-05)


### Features

* Run test tools in docker interactive mode to admit std streams

## 0.15.1 (2023-02-04)


### Bug Fixes

* Return value from get_docker_workspace_mount()

## 0.15.0 (2023-02-03)


### Features

* Use WORKSPACE_MOUNT if set


### Bug Fixes

* Pin commit-and-tag-version to v10.1.0

## 0.14.0 (2023-01-27)


### Features

* Improve verbose output for get-builder-image-tagged

## 0.13.1 (2023-01-26)


### Bug Fixes

* Upgrade software-properties-common

## 0.13.0 (2023-01-23)


### Features

* Add ab tool
* Add cassowary http load testing tool
* Add h2load tool
* Add slowhttptest tool
* Adjust get-builder-image-tagged verbose output
* Upgrade to packer v1.8.5


### Bug Fixes

* Re-pin dependencies in test-tools image
* Relax version pins to semver
* Upgrade amazonlinux2 base image
* Upgrade git on amazonlinux2

## 0.12.0 (2023-01-10)


### Features

* Modify ghz wrapper for generic use. Add curl
* Use test-tools image for grpcurl

## 0.11.0 (2023-01-09)


### Features

* Add chrpath


### Bug Fixes

* Clean up tmpdir via RETURN trap

## 0.10.0 (2023-01-06)


### Features

* Drop ubuntu package version minor for curl

## 0.9.0 (2023-01-04)


### Features

* Add zlib1g-dev to build-debian per scp build dependency
* Update hook versions


### Bug Fixes

* Correct non-zero error message and drop sourcing of tools/builder.sh
* Elide warnings from bazel info
* Revert from clang-format v15 to v14

## 0.8.0 (2022-12-29)


### Features

* Add bash and jq to test-tools image
* Add script to normalize bazel- symlinks
* Ensure run-tests includes all images
* Skip symlinks that resolve in normalize-bazel-symlink

## 0.7.0 (2022-12-27)


### Features

* Add ghz wrapper script
* Add test-tools image

## 0.6.0 (2022-12-12)


### Features

* Add EXTRA_DOCKER_RUN_ARGS support in aws-cli


### Bug Fixes

* Emit docker build output only on non-zero exit
* Remove tempfile before exiting

## 0.5.0 (2022-12-06)


### Features

* Pin versions in presubmit image


### Bug Fixes

* Avoid cache for tar image
* Update version pin for ca-certificates
* Use images subdirs for image list

## 0.4.4 (2022-11-18)


### Bug Fixes

* Retain execute permissions when normalizing dist

## 0.4.3 (2022-11-17)


### Bug Fixes

* Add gettext package for envsubst
* Avoid yum package version strings specific to CPU architecture
* Improve verbose output for get-builder-image-tagged
* Pin apt and yum package versions

## 0.4.2 (2022-11-17)


### Bug Fixes

* Generate SHA within docker container

## 0.4.1 (2022-11-15)


### Bug Fixes

* Reduce noise creating presubmit image
* Remove docker run --interactive flag

## 0.4.0 (2022-11-14)


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

## 0.3.1 (2022-11-01)


### Bug Fixes

* Add OpenJDK 11 in build-amazonlinux2

## 0.3.0 (2022-11-01)


### Features

* Add git to build-amazonlinux2 image
* Add google-java-format pre-commit hook
* Add OpenJDK 11
* Move python3 to /opt/bin/python3


### Bug Fixes

* Ensure builder::set_workspace does not overwrite WORKSPACE

## 0.2.0 (2022-10-26)


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
