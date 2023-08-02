# Changelog

All notable changes to this project will be documented in this file. See [commit-and-tag-version](https://github.com/absolute-version/commit-and-tag-version) for commit guidelines.

## 0.11.1 (2023-08-02)

## 0.11.0 (2023-07-11)


### Features

* [Breaking change] Use UserDefinedFunctionsConfig instead of KVs for loading UDFs.
* [Sharding] Add hpke for s2s communication
* [Sharding] Allow for partial data lookups
* [Sharding] Making downstream requests in parallel
* Add bazel build flag --announce_rc
* Add bool parameter to allow routing V1 requests through V2.
* Add buf format pre-commit hook
* Add build time directive for reentrant parser.
* Add functions to retrieve instance information.
* Add internal run query client and server.
* Add JS hook for set query.
* Add lookup client and server for communication with shards
* Add MessageQueue for the request simulation system
* Add query grammar and interface for set queries.
* Add rate limiter for the request simulation system
* Add second map to store key value set and add set value update interfaces
* Add shard metadata for supporting sharded files
* Add simple microbenchmarks for key value cache
* Add UDF support for format data command.
* Add unit tests for query lexer.
* Adding cluster mappings manager
* Adding padding
* Apply custom lockings on the cache
* Connect InternalRunQuery to the parser
* Extend and simplify collect-logs to capture test outputs
* Extend use of scp deps via data-plane-shared repo
* Implement shard manager
* Move sharding function to public so it's available for file sharding
* Register a logging hook with the UDF.
* Register run query hook with udf framework.
* Sharding - realtime updates
* Sharding read flow fixes
* Simplify work done in set operations. Set operations can be passed by
* Snapshot files support UDF configs.
* Support reading and writing set queries to data files.
* Support reading and writing set values for csv files
* Support reading/writing DataRecords. Requires new DELTA format.
* Support writing sharded files
* Update data_loading.fb to support UDF code updates.
* Update pre-commit hook versions
* Update shard manager mappings continuously
* Upgrade build-system to release-0.28.0
* Upgrade build-system to v0.30.1
* Upgrade scp to 0.72.0
* Use Unix domain socket for internal lookup server.
* Utilize AWS deps via data-plane-shared repo


### Bug Fixes

* Add internal lookup client deadline.
* Catch error if insufficient args specified
* Fix aggregation logic for set values.
* Fix ASAN potential deadlock errors in key_value_cache_test
* Proper memory management of callback hook wrappers.
* Specify 2 workers for UDF execution.
* Upgrade pre-commit hooks
* Use shared pointer for UDF absl::Notification.


### Build Tools: Fixes

* **build:** Add scope-based sections in release notes


### Documentation

* Add docs for data loading capabilities
* Add explanation that access control is managed by IAM for writes.
* Point readme to a new sharding public explainer

## 0.10.0 (2023-05-04)


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

## 0.9.0 (2023-04-10)


### Features

* Add a total realtime QPS metric
* Add aws supplied e2e latency
* Add basic UDF functional tests for v2
* Add error counters for realtime updates
* Add functional test stubs for v2
* Add target to generate delta for sample udf.js
* Add test data artifacts to dist/test_data/deltas
* Add UDF delta file generator tool.
* Add UDF delta file upload through terraform config.
* Add udf.js delta file to test set
* Upgrade to build-system 0.22.0 and functionaltest-system 0.2.0


### Bug Fixes

* Add a dashboard for environments
* Add documentation for editing dashboards
* Change envoy log level to debug
* Check that recovery function is valid before calling it.
* Enable docker network cleanup
* Ensure changelog notes use specific version
* ignore interestGroupNames argument
* MetricsRecorder no longer a singleton.
* MetricsRecorder now optional for retry templates.
* Return missing key error status from internal lookup server.
* Upgrade gRPC and make lookup client a singleton
* Use dynamic_cast to get metric_sdk::MeterProvider provider.


### Terraform

* Add us-west-1 terraform


### Documentation

* Add documentation on roma child processes.
* Add instructions on realtime updates
* Add note to use grpcurl for v2 since http has a bug.
* Add v2 request JSON schema
* AWS realtime update capabilities
* Correct udf target name
* Update documentation for building data cli.
* Update realtime metrics querying docs

## 0.8.0 (2023-03-28)


### Features

* add AWS SQS ReceiveMessage latency histogram
* Add command line flags for parameters
* Add configurable thread count for realtime updater
* Add functional test
* Adding e2e latency measurement for realtime updates
* Allow specifying explicit histogram bucket boundaries
* Allow the blob_storage_util cp command to work with local files.
* Allow the DeltaFileRecordChangeNotifier to read local files as well as from S3
* Batch delete SQS messages
* Build delta files from csv
* clean up realtime queues
* Configure AWS hosted Prometheus.
* Disable the use of exceptions
* Enhance/Simplify local export of telemetry with OTLP.
* Functional testing of local server with delta files
* make AwsSnsSqsManager thread safe
* Make the blob_storage_change_watcher tool work for local files
* Make the blob_storage_util cat and rm commands work for local files
* Make the blob_storage_util ls command work for local files and refactor out common parts from the AWS binary
* Make the delta_file_watcher tool work for local files
* Move the platform specific server configuration logic to a separate file
* multi-threaded realtime notifier
* realtime tester in a container
* Reuse SQS client
* Speed up test updates publisher
* Tools for generating and inserting realtime test data.
* Upgrade build-system to release-0.18.0
* Upgrade build-system to release-0.20.0
* Upgrade debian runtime images to 15 Feb 2023
* Upgrade to build-system 0.17.0
* Use a PlatformInitializer so the data_cli will compile for --platform=local


### Bug Fixes

* Add ability to interrupt a SleepFor Duration.
* Add minimum shard size threshold for concurrent reader.
* Launch Envoy first before all other processes.
* Make MetricsRecorder a shared global instance.
* Only run functional tests against local server
* Remove functionaltest/run-server, update docs accordingly
* Remove submodule section from docs
* Run server in background, and reduce noise
* Run server using delta files in run-server-docker
* Update generate_load_test_data to use bazel-debian
* Use symlink for identical test replies
* Use VLOG for concurrent reader debugging logs.
* Wait for envoy to respond before launching enclave service.


### Documentation

* Add a playbook template for new alerts
* Add description for backup_poll_frequency_secs
* Add docs about how to run the server locally
* fix /tmp/deltas discrepancy
* remove obsolete service
* Updating instructions on how to copy `eif` manually.


### Terraform

* Convert tfvar files to json
* Support Prometheus service running in a different region

## 0.7.0 (2023-02-16)


### Features

* Add --platform flag to build_and_test_all_in_docker
* Add a concurrent record reader for data files.
* Add a helper service for protocol testing
* Add a stub class for reading local blob files
* add automated PCR0 updates to kokoro continuous
* Add base streambuf with seeking support for reading blobs.
* Add delta writer and custom audience data parser
* add github personal access token validation for release scripts
* Add instance id to metrics.
* Add seeking to S3 blob reader.
* Add support for Zipkin exports for local builds.
* Add terraform logic to create SNS for real time updates
* Check timestamps for cache update
* Implement BinaryHTTP version of V2 API
* Implement delta file record change notifier to retrieve high priority updates
* Implement test OHTTP V2 query handling
* Integrating high priority updates in data server
* Memory cleanup for delete timestamps in the cache
* Record metrics for all RetryUntilOk events.  Export them to stdout or
* Upgrade black to 23.1.0
* Upgrade to build-system 0.13.0
* Upgrade to build-system 0.14.0
* Upgrade to build-system 0.16.0
* Use concurrent reader for reading snapshot and delta files.


### Bug Fixes

* Add docker compose config for testing locally.
* add empty bug id to automated PCR0 CL
* Add unit test for delta file backup poll.  Fix bug where we can't
* Don't ListBlobs to poll Delta files on notifications that don't
* Don't use default number of cores for small test files.
* fetch git remote for automated pcr0 updates
* Fix typos and remove unreachable branches.
* flaky delta_file_notifier test.
* Listing non-delta files from bucket shouldn't cause state change.
* Only read the most recent snapshot file.
* path for local envoy
* Prefer github release artifacts over archive artifacts
* remove duplicate open telemetry entry.
* remove spaces from automated PCR0 CL commit footer
* Switch jaeger over to using OTLP directly.  Jaeger otel component is
* Upgrade to rules_buf 0.1.1
* Uprev Otel to pull in semantic resource convensions.  Use them
* Use shared libraries for proxy


### Build System

* Hide build stdout/stderr for third_party


### Documentation

* Add docs for data loading library.

## 0.6.0 (2023-01-10)


### Features

* Add --no-precommit flag to build_and_test_all_in_docker
* Add command to generate snapshots to data cli
* Add support for reading snapshots during server startup.
* Implement a snapshot writer.
* Produce PCR0.json for server EIF
* Remove VCS commit info from server --buildinfo
* Reorg build_and_test* scripts
* Store and validate arch-specific PCR0 hash
* update dev to staging copybara to include github workflows
* update GitHub presubmit workflow to trigger on pull request
* Upgrade to build-system 0.10.0
* Upgrade to build-system 0.5.0
* Upgrade to build-system 0.6.0
* Upgrade to gRPC v1.51.1


### Bug Fixes

* add missing "xray" to vpc_interface_endpoint_services references.
* Adjust git global config
* Attach initial_launch_hook to autoscaling group.
* Avoid non-zero exit on PCR0 hash mismatch
* Correct documentation on endpoint to test
* Fix the region doc for local development.
* Ignore builders/ when executing pre-commit
* LifecycleHeartbeat only Finish once.  Fixed unit test.
* Upgrade to addlicense v1.1
* Use absolute path for kokoro_release.sh
* Use bazel-debian to build and run test_serving_data_generator


### Build System

* Add presubmit GitHub workflow
* Upgrade to bazel 5.4.0


### Documentation

* Add a default AWS region to push command
* Correct command to run server locally
* Update ECR format and improve the AWS doc order

## 0.5.0 (2022-11-28)


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

## 0.4.0 (2022-10-11)


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

## 0.3.0 (2022-09-14)


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

## 0.2.0 (2022-09-07)


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
