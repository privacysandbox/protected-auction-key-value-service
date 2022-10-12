# Changelog

All notable changes to this project will be documented in this file. See [commit-and-tag-version](https://github.com/absolute-version/commit-and-tag-version) for commit guidelines.

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
