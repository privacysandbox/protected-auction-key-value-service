# Changelog

All notable changes to this project will be documented in this file. See [commit-and-tag-version](https://github.com/absolute-version/commit-and-tag-version) for commit guidelines.

### [0.3.2](https://github.com/privacysandbox/fledge-key-value-service/compare/release-0.3.1...release-0.3.2) (2022-09-20)


### Bug Fixes

* Strip commit hashes from CHANGELOG.md

### [0.3.1](https://github.com/privacysandbox/fledge-key-value-service/compare/release-0.3.0...release-0.3.1) (2022-09-20)


### Bug Fixes

* Modify build and packaging for AWS SQS Lambda

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
