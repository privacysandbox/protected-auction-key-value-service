# Changelog

All notable changes to this project will be documented in this file. See [commit-and-tag-version](https://github.com/absolute-version/commit-and-tag-version) for commit guidelines.

## [0.2.0](https://team/potassium-engprod-team/functionaltest-system/compare/v0.1.0...v0.2.0) (2023-04-03)


### Features

* Add test_tags to test suites ([a9011d2]( ))
* Create annotated tag in addition to branch for releases ([f5b847f]( ))
* Switch from cpu:arm64 to cpu:aarch64 ([2457ca6]( ))
* Upgrade build-system to release-0.21.1 ([8a8dda6]( ))


### Bug Fixes

* Ensure changelog notes use specific version ([b960648]( ))
* improve usage message for --endpoint-env-var flag in internal bazel grpcurl_diff_test_runner script ([02da2b4]( ))
* Remove debug output ([18551b0]( ))
* Remove exit-status flag for post-filter jq ([e9e24e2]( ))

## 0.1.0 (2023-03-07)


### Features

* Add endpoint and rpc as grpcurl_diff_test_suite args
* add experimental script for generating performance perfgate benchmarks
* Add ghz load-testing support
* Add java formatter pre-commit hook
* Add local docker-based deployment and testing
* Add pre-rpc jq filter
* Add release scripts
* Add test for importing this repo through bazel workspace
* Add test_suites
* Basic grpc service diff testing
* Improve test file glob support in grpcurl_diff_test_suite
* Move glob for test files into BUILD from bzl function
* Reduce direct dependencies in the use_repo workspace
* refactors ab_to_perfgate_rundata to only output benchmark key in generated quickstore input file
* Support jq --slurp filters
* Update to build-system 0.14.0
* Upgrade black to 23.1.0
* Upgrade build-system to 0.21.0
* Upgrade build-system to release-0.18.0
* Upgrade build-system to release-0.20.0
* Upgrade build-system to v0.7.0
* Upgrade build-system to v0.8.0
* Upgrade to build-system 0.16.0
* Upgrade to build-system 0.17.0


### Bug Fixes

* Add @ for bazel repo
* Refactor all bazel third-party repositories into deps.bzl
* Require tools image env var TEST_TOOLS_IMAGE
* Use Label()


### Documentation

* Add Getting started to README
