# Changelog

All notable changes to this project will be documented in this file. See [commit-and-tag-version](https://github.com/absolute-version/commit-and-tag-version) for commit guidelines.

## Release 0.10.0 (2023-05-04)


### Features

* Add --fast option for running functional tests
* Add a multi-KV SUT
* Add baseline for SSP
* Add bazel macro to generate OpenSSL certificates
* Add brotli compression support
* Add envoy for KV server in baseline SUT
* Add envoy frontend in baseline SUT
* Add optional filename arg to tools/collect-logs
* Add SUT without UDF
* Add terraform support for sharding
* Drop data meant for other shards
* Include tink_cc repository
* Set timestamp for generated delta files for SUTs
* Support selection of single SUT for testing
* Upgrade gRPC to v1.52.1
* Upgrade to functionaltest-system 0.5.1
* Use 443 for http1.1 and 8443 for http2.
* Use scp deps exposed by data-plane-shared-libraries repo
* Use shared control plane skylark functions for transitive dependencies


### Bug Fixes

* Address linter issues in production dir
* Address linter issues in tools
* Avoid running prettier on PCR0 files
* Capture test logs into zip archive
* Collect logs from functional testing
* Configure docker compose to pull images quietly
* Correct container deps in multiple-kv-servers
* Defer to data-plane-shared cpp deps
* Remove exposed ports as not needed within docker network
* Remove unused code in tools/get_workspace_status
* Remove workspace parent dirs from logs archive
* Upgrade pre-commit hooks


### Documentation

* Add Protected Audience API rename banner

### 0.1.0 (2022-08-18)


### Features

This is the initial release of the system. See [README](/README.md) for the current state of the system and more information.
