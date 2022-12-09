# Changelog

All notable changes to this project will be documented in this file. See [commit-and-tag-version](https://github.com/absolute-version/commit-and-tag-version) for commit guidelines.

### [0.5.1](https://github.com/privacysandbox/fledge-key-value-service/compare/release-0.5.0...release-0.5.1) (2022-12-08)


### Bug Fixes

* Ignore builders/ when executing pre-commit
* Upgrade to build-system 0.5.0

## [0.5.0](https://github.com/privacysandbox/fledge-key-value-service/compare/release-0.4.0...release-0.5.0) (2022-11-28)


### Features

* Add basic smoke test
* Add builders/utils docker image
* Add hadolint to lint Dockerfiles
* Add toolchain short hash to bazel output_user_root path
* Add tools/lib/builder.sh
* Add utils for working with snapshot files.
* Adopt build-system release-0.2.0
* Allow AMI building to specify AWS region.
* Bump debian runtime to stable-20221004-slim
* Rename nitro_artifacts to aws_artifacts
* Set BUILD_ARCH env var in docker images
* Simplify use of --with-ami flag
* Tag small tests
* Upgrade build-debian to python3.9
* Upgrade to build-system 0.3.1
* Upgrade to build-system 0.4.3
* Upgrade to build-system 0.4.4
* Upgrade to clang v14 on bazel-debian


### Bug Fixes

* Add get_workspace_mount function to encapsulate code block
* Allow server script to accept any flags
* Avoid installing recommended debian packages
* Copy get_values_descriptor_set.pb to dist dir
* Correct shell quoting
* Correct workspace volume when tools/terraform is executed in a nested container
* Execute tests prior to copy_to_dist
* Guess user/group for files when running container as root bazel
* Ignore InvalidArgument error on completing lifecycle hook.
* include a backoff on errors to long poll 'push' notifications.
* Invoke addlicense for all text files
* Invoke unzip via utils image
* Migrate duration code into KV server.
* Minor improvements to shell scripts
* Modify normalize-dist to use builder::id function
* Mount $HOME/aws in aws-cli container
* Move builder-related configs to builders/etc
* Move WORKSPACE definition to cbuild script global
* multi-region support for sqs_lambda
* Propagate AWS env vars and $HOME/.aws into terraform container
* Propagate gcloud stderr
* Reduce noise from tools/collect-logs
* Remove build timestamp to afford stability of binary
* Remove debugging statement
* Remove docker flags -i and -t
* Remove dockerfile linter ignore and correct ENTRYPOINT
* Remove pre-commit config from build-debian
* Rename bazel image name debian-slim to runtime-debian
* Set architecture in container_image declaration
* Set bazel output_base to accommodate distinct workspaces
* Set WORKSPACE variable
* Support regions outside us-east-1
* unzip should overwrite files
* Update gazelle to v0.28.0
* Upgrade bazel-skylib to 1.3.0
* Upgrade rules_pkg to 0.8.0
* Use builder library functions


### Documentation

* Add error handling guidelines.
* Add submodule instructions
* Fix build command
* fix typo in aws doc
* recommend the use of native AWS CLI in documentation
* Remove an unnecessary step in server doc

## [0.4.0](https://github.com/privacysandbox/fledge-key-value-service/compare/release-0.3.0...release-0.4.0) (2022-10-11)


### Features

* Add //:buildifier rule as an alias to the pre-commit buildifier hook
* Add a debugging endpoint for Binary Http GetValues.
* Add aws-cli helper script
* Add csv reader and writer
* Add delta record reader based on riegeli stream io.
* Add library for reading and writing delta files
* Add utility for data generation (currently supports csv to delta and vice versa)
* Add version info to server binaries
* Determine workspace mount point from docker inspect if inside docker container
* Display pre-commit error log if it exists
* Implement API call to record lifecycle heartbeat.
* Log bazel build flags during server startup runtime.
* Overhaul building on amazonlinux2
* Repeating timer with callback implementation
* Set working dir to current workspace-relative path in tools/terraform


### Bug Fixes

* Add bazel rule to copy files to dist dir
* Add builders/tools/normalize-dist to chmod/chgrp/chown dist/ directory tree
* Add fetch git tags from remote prior to syncing repos
* Add files to subject to chown and chgrp
* Adjust chown/chgrp to be silent
* Adopt shellcheck
* Clean bazel_root for smaller docker image
* Correct the WORKSPACE path in production/packaging/aws/build_and_test
* Correct variable used to check for valid repo name
* Drop packer from build-debian image
* Fix a typo and improve some logging for DataOrchestrator's loading
* Improve cbuild help text
* Improve git push message to accommodate patch branches
* Increase SQS cleanup lamabda timeout
* Modifications as indicated by shellcheck
* Modify build and packaging for AWS SQS Lambda
* Move definition from header to cc to eliminate linker error
* Only propagate AWS env vars into amazonlinux2 build container
* pre-commit CLEANUP should default to zero
* Print pre-commit version rather than help
* Print timestamps in UTC timezone
* Remove container when get-architecture exits
* Remove duplicate text "instance:" in build flavor
* Remove shellcheck from build-debian
* Remove unused nitro_enclave_image bazel rule
* Set bazel output_user_root in image bazelrc
* Set locale in build-debian
* Strip commit hashes from CHANGELOG.md
* Switch from hardcoded arch to using dpkg --print-architecture
* Update author name
* Update pre-commit to use cbuild
* Use --with-ami flag to determine bazel flags instance/platform
* Use default health check grace priod (300s) now that we have heartbeats.
* Use git rather than bazel to determine workspace root
* Use PRE_COMMIT_TOOL env var


### Build System

* Add arch to docker image tags
* Add get_builder_image_tagged tool to determine a content-based tag
* Add get-architecture helper script
* Propagate status code in exit functions


### Documentation

* Correct command to load docker image locally
* Instructions to make changes to a dependency
* Sugggest use of python virtualenv
* Use concise form of passing env vars into docker container

## [0.3.0](https://github.com/privacysandbox/fledge-key-value-service/compare/release-0.2.0...release-0.3.0) (2022-09-14)


### Features

* Add --env flag to cbuild
* Update release image to node v18


### Bug Fixes

* Bump to latest version of bazelisk
* Consolidate bazel/ dir into third_party/ dir
* Ensure appropriate ownership of modified files
* fix local docker run command
* Improve shell string quoting
* Invoke bash via /usr/bin/env
* Propagate SKIP env var into pre-commit container

## [0.2.0](https://github.com/privacysandbox/fledge-key-value-service/compare/release-0.1.0...release-0.2.0) (2022-09-07)


### Features

* Add arg processing to cbuild script
* Add bazel-debian helper script
* Add black python formatter to pre-commit
* Add optional flags to cbuild tool
* Inject clang-version as a bazel action_env
* Migrate generation of builders/release container image to Dockerfile
* Remove python build dependencies from bazel
* Support installation of git pre-commit hook
* Support running the server container locally
* Use EC2 instance connect for ssh access.


### Bug Fixes

* Add public/ to presubmit tests
* Add python version to action_env
* Add require-ascii pre-commit hook
* Add/remove basic pre-commit hooks
* Allow for short(er) presubmit builds
* Change ownership of dist/ to user
* Correct git instructions at the end of cut_release
* Define python3 toolchain
* Ensure /etc/gitconfig is readable by all
* Install bazel version as specified in .bazelversion
* Move bazel env vars from comments to help text
* Pin version of bazelisk
* Pin version of libc++-dev
* Pin version of python3.8
* Remove check for uncommitted changes, tools/pre-commit exit status should suffice
* Tidy cut_release
* Upgrade zstd to v1.5.2


### Build System

* Add buildifier tool to pre-commit
* Add terraform fmt to pre-commit
* Add terraform to tools
* Add tools/pre-commit
* Adjust branch specification for mirroring to GitHub
* Move commit-and-tag-version into tools dir
* Move gh into tools dir
* Reduce redundant installation commands
* Reinstate use of cpplint, via pre-commit tool
* Remove buildifier from bazel build as it is redundant with pre-commit
* Remove release-please tool
* Rename builders/bazel to build-debian

### 0.1.0 (2022-08-18)


### Features

This is the initial release of the system. See [README](/README.md) for the current state of the system and more information.
