# Changelog

All notable changes to this project will be documented in this file. See [commit-and-tag-version](https://github.com/absolute-version/commit-and-tag-version) for commit guidelines.

### [0.3.1](https://github.com/privacysandbox/fledge-key-value-service/compare/release-0.3.0...release-0.3.1) (2022-09-20)


### Bug Fixes

* Modify build and packaging for AWS SQS Lambda (f02e211)

## [0.3.0](https://github.com/privacysandbox/fledge-key-value-service/compare/release-0.2.0...release-0.3.0) (2022-09-14)


### Features

* Add --env flag to cbuild (1d85a36)
* Update release image to node v18 (7fb4a33)


### Bug Fixes

* Bump to latest version of bazelisk (82441c3)
* Consolidate bazel/ dir into third_party/ dir (7c703d2)
* Ensure appropriate ownership of modified files (950324f)
* fix local docker run command (587a556)
* Improve shell string quoting (53e5940)
* Invoke bash via /usr/bin/env (c098fff)
* Propagate SKIP env var into pre-commit container (cf5a5b5)

## [0.2.0](https://github.com/privacysandbox/fledge-key-value-service/compare/release-0.1.0...release-0.2.0) (2022-09-07)


### Features

* Add arg processing to cbuild script (b45ca62)
* Add bazel-debian helper script (c0b38b4)
* Add black python formatter to pre-commit (6d079d9)
* Add optional flags to cbuild tool (6b68d11)
* Inject clang-version as a bazel action_env (40af734)
* Migrate generation of builders/release container image to Dockerfile (abb70a8)
* Remove python build dependencies from bazel (81dcb7a)
* Support installation of git pre-commit hook (b7c565a)
* Support running the server container locally (8370503)
* Use EC2 instance connect for ssh access. (604e3b5)


### Bug Fixes

* Add public/ to presubmit tests (2d0056a)
* Add python version to action_env (8e0e074)
* Add require-ascii pre-commit hook (50eb751)
* Add/remove basic pre-commit hooks (f8e46cd)
* Allow for short(er) presubmit builds (8656da5)
* Change ownership of dist/ to user (d7282a2)
* Correct git instructions at the end of cut_release (76c19de)
* Define python3 toolchain (fa83525)
* Ensure /etc/gitconfig is readable by all (f0c82c2)
* Install bazel version as specified in .bazelversion (00d6f63)
* Move bazel env vars from comments to help text (32b2f6f)
* Pin version of bazelisk (face7df)
* Pin version of libc++-dev (08b8d7a)
* Pin version of python3.8 (ad9c61f)
* Remove check for uncommitted changes, tools/pre-commit exit status should suffice (b96636c)
* Tidy cut_release (51f1b0a)
* Upgrade zstd to v1.5.2 (ba7bda1)


### Build System

* Add buildifier tool to pre-commit (1904652)
* Add terraform fmt to pre-commit (0ba7623)
* Add terraform to tools (96ef5ee)
* Add tools/pre-commit (6083417)
* Adjust branch specification for mirroring to GitHub (a004730)
* Move commit-and-tag-version into tools dir (73e0c42)
* Move gh into tools dir (d6a15c1)
* Reduce redundant installation commands (59b93c8)
* Reinstate use of cpplint, via pre-commit tool (be0b5e5)
* Remove buildifier from bazel build as it is redundant with pre-commit (db6b139)
* Remove release-please tool (6bcd54a)
* Rename builders/bazel to build-debian (71b81b0)

### 0.1.0 (2022-08-18)


### Features

This is the initial release of the system. See [README](/README.md) for the current state of the system and more information.
