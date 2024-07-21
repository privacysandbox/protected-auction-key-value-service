# Changelog

All notable changes to this project will be documented in this file. See [commit-and-tag-version](https://github.com/absolute-version/commit-and-tag-version) for commit guidelines.

## 0.17.0 (2024-07-08)


### Features

* Add a set wrapper around bitset for storing uint32 values
* Add a thread safe wrapper around hash map
* Add b&a e2e test env
* Add data loading support for uint32 sets
* Add health check to AWS mesh.
* Add hook for running set query using uint32 sets as input
* Add interestGroupNames to V1 API
* Add latency metrics for cache uint32 sets functions
* Add latency without custom code execution metric
* Add option to use existing network on AWS.
* Add padding to responses
* Add request log context to request context
* Add runsetqueryint udf hook
* Add set operation functions for bitsets
* Add support for int32_t sets to key value cache
* Add support for reading and writing int sets to csv files
* Add udf hook for running int sets set query (local lookup)
* Allow pas request to pass consented debug config and log context
* Implement sharded RunSetQueryInt rpc for lookup client
* Implement uint32 sets sharded lookup support
* Load consented debug token from server parameter
* Pass LogContext and ConsentedDebugConfig to internal lookup server in sharded case
* Plumb the safe path log context in the cache update execution path
* Set verbosity level for PS_VLOG
* Simplify thread safe hash map and use a single map for node storage
* Support uint32 sets for query parsing and evaluation
* Support uint32 sets in InternalLookup rpc
* Switch absl log for PS_LOG and PS_VLOG for unsafe code path
* Switch absl log to PS_LOG for safe code path
* Switch absl vlog to PS_VLOG for safe code path
* Update AWS coordinators public prod endpoint from GG to G3P


### Bug Fixes

* Add missing include/library deps
* Augment UDF loading info message
* Correct copts build config.
* Correct verbosity flag for gcp validator.
* Effectively lock the key in the set map cleanup
* Fix detached head of continuous e2e branch.
* Properly initialize runSetQueryInt hook
* Remove ignore interestGroupNames from envoy
* Remove test filter to allow all unit tests run in the build
* Simplify request context and pass it as shared pointer to the hooks
* Upgrade common repo version
* Use kms_binaries tar target from common repo
* Use structured initializer for clarity


### Dependencies

* **deps:** Upgrade build-system to 0.62.0
* **deps:** Upgrade data-plane-shared-libraries to 52239f15 2024-05-21
* **deps:** Upgrade pre-commit hooks


### GCP: Features

* **GCP:** Switch to internal lb for the otlp collector
* **GCP:** Switch to internal lb for the otlp collector with bug fixes


### Documentation

* Add debugging playbook
* Correct commands for sample_word2vec getting_started example
* KV onboarding guide
* Update to the ads retrieval explainer
* Update word2vec example
* Use aws_platform bazel config
* Use local_{platform,instance} bazel configs

### Image digests and PCR0s

GCP: sha256:d09d5a6d340a8829df03213b71b74d4b431e4d5a138525c77269c347a367b004
AWS: {"PCR0":"1e28ac4b72600ea40d61e1756e14f453a3d923a1bf94c360ae48d9777bff0714923d9322ed380823591859e357d2f825"}

## 0.16.0 (2024-04-05)


### Features

* Add cache hit or miss metrics
* Add coorindator specific terraform parameters
* Add data loading prefix allowlist parameter
* Add default PAS UDF
* Add E2E latency for GetKeyValues and GetKeyValueSet in sharded lookup
* Add file groups and file group reader logic
* Add go fmt to pre-commit
* Add key prefix support to blob storage client
* Add LogContext and ConsentedDebugConfiguration proto to v2 API and internal lookup API
* Add prod and nonprod build flag
* Add request context to wrap metrics context
* Add support for configuring directory allowlist
* Add wiring for prefix allowlist (actual impl in follow up cl)
* Allow overrides for coordinators endpoints in nonprod mode
* Allow to disable v1 key not found entry in response
* Create separate metrics context map for internal lookup server
* Deprecate metrics recorder for internal lookup
* Deprecate metrics recorder for internal server
* Deprecate metrics recorder for sharded lookup
* deprecate metrics recorder for V1 server and handler
* Deprecate metrics recorder from cache
* Enable simulation system send realtime udpates
* Enable TCMalloc for KV Server and benchmarks
* Explicitly enable core dumps
* Implement deletion cutoff max timestamp per directory
* Load data files and allow notifications from configured prefix
* Load prefix files on startup and handle prefix blob notifications
* Log common request metrics
* Migrate from glog to absl log
* Partition data loading metrics by delta file name
* Pass request context from hooks to downstream components
* Pass request context to udf hooks
* Read telemetry config from cloud parameter
* Revamp AWS metrics dashboard
* Revamp GCP metrics dashboard
* Set udf_min_log_level from parameter store.
* Support content type proto for v2 api
* Support content type proto for v2 api response
* Update cache interface and blob data location to pass prefix
* Update start_after to use a map from prefix to start_after
* Use file groups for loading snapshots
* Write logs to an Otel endpoint


### Bug Fixes

* Actually load all files in a snapshot file group
* **AWS:** Filter out unavailable zones.
* Correct an error in kokoro_release.
* Correct format for image tag.
* Correct typo for internal dev's service_mesh_address.
* Correct typos in GCP deployment guide.
* Crash server if default UDF fails to load.
* Delete non-active certificate before creating a new one.
* Fix filtering logic for prefixed blobs
* Fix permissions for data-loading-blob-prefix-allowlist
* Make GCP nat optional.
* Parse delta filename from notification before validating it
* Remove glog dependency for record_utils
* Remove temp dir only if it's successfully created.
* Rename class to ThreadManager
* Set retain_initial_value_of_delta_metric flag for aws metrics exporter
* Update a outdated hyperlink.
* Update common repo to pick up the AWS metrics dimension fix
* Update GCP Terraform with ability to delete unhealthy instance.
* Update tf variables to use gorekore instead of kelvingorekore
* Use blob key instead of prefixed basename


### GCP: Fixes

* **GCP:** Make sure server is connected to otel collector before reaching to ready state


### GCP: Features

* **GCP:** Applying Terraform pulls docker image with new tag.
* **GCP:** Make service mesh address configurable.
* **GCP:** Make subnet ip cidr configurable.
* **GCP:** Make xlb/envoy optional.


### Documentation

* Add ad retrieval explainer.
* Add docs for directory support
* Add PA and PAS folders
* Add PAS developer guide
* Add public docs for file groups
* Ads retreival explainer update.


### Dependencies

* **deps:** Add clang-tidy bazel config
* **deps:** Add cpp_nowarn bazel config
* **deps:** Upgrade bazel to 6.5.0
* **deps:** Upgrade build-system to 0.55.1
* **deps:** Upgrade build-system to 0.55.2
* **deps:** Upgrade build-system to 0.57.0
* **deps:** Upgrade data-plane-shared repo
* **deps:** Upgrade data-plane-shared repo to 1684674 2024-02-09
* **deps:** Upgrade data-plane-shared-libraries to 1fbac46
* **deps:** Upgrade pre-commit hooks

## 0.15.0 (2024-01-23)


### Features

* Add an ability to add default tags
* Add AWS cpu utilization and memory utilization dashboards
* Add bazel configs prod and non_prod
* Add change notifier error count to the AWS metrics dashboard
* Add delta based realtime updates publisher
* Add delta record limited file writer
* Add GCP system metrics dashboard for kv server
* Add gcp_project_id flag when --platform=gcp is specified
* add go script for pushing gcp docker image
* Add optional logical_commit_time param to wasm macros.
* Add sharded realtime message batcher
* Add sharding data locality terraform parameters
* Add sharding support for publishing engine
* Add system metrics to log cpu and memory utilization
* Add UDF timeout parameter value to terraform
* Allow prefix path segments for gRPC http paths.
* Allow setting logging verbosity level through a parameter
* Data locality support
* Enable prettier pre-commit hook for JavaScript
* Expose http_api_paths tf variable to allow prefix matching
* Log error metrics for the change notifier
* Log metrics in the retries with new Telemetry API
* Migrate realtime metrics to new Telemetry API
* Remove envoy health checks
* Remove metrics recorder from blob storage client
* Remove metrics recorder from data orchestrator
* Remove metrics recorder from data reader
* Set UDF timeout from parameters
* Support passing in shard metadata via data_cli flags
* Update default tretyakov aws env settings
* Update v1 to return status per key


### Bug Fixes

* Add all record values to switch cases
* add data-bucket-id back as a flag suffix.
* Add run_all_tests bazel config
* bazel config for emscripten no longer needs tool chain resolution
* Check grpc channel connection before sending requests
* **deps:** Upgrade data-plane-shared-libraries to 44d1d64 2024-01-08
* envoy.yaml permission and pre-commit fix
* Integrate RomaService changes from common repo
* Make some cache logging more precise.
* Missing absl deps
* Prevent Config from being copied into RomaService
* rectify kv_service_port in terraform.
* Remove jaeger as it has been deprecated
* Remove local platform for common repo and fix CloudPlatform import
* Update 'X-allow-fledge' header to 'Ad-Auction-Allowed' according to the spec.
* update GCP tentb environment
* Update UDFs to fix breakages due to FunctionBindingObjectV2
* Upgrade otel collector to the latest version


### Dependencies

* **deps:** Upgrade data-plane-shared to commit f0d0b89 2023-11-14


### GCP: Features

* **GCP:** Add external load balancer and envoy.
* **GCP:** store Terraform state remotely in GCS bucket


### Documentation

* [Sharding] Add info about NUMA clusters
* Add docs for server cpu and memory profiling
* Add info on default wasm macro linkopts and memory limits.
* Add state transition sequence diagrams for aws/gcp sharded/nonsharded configurations
* Data locality
* Doc for aws private communication setup
* Sharding -- add a link to a tool that sets shard id in the delta metadata
* Specify --init flag for docker run commands

## 0.14.0 (2023-11-07)


### Features

* [API breaking change] Rename subkey to hostname.
* [Sharding] Add a tool to validate records
* [Sharding] Generate shard specific delta files
* Add base64 encoding to CSV delta cli tool
* Add safe metric definitions
* Add unsafe metric definitions
* Fully statically link the server binary
* Simplified UDF tester.


### Bug Fixes

* Add error message when key not found.
* Add proxy visibility back to aws_artifacts
* allow internal ingress for otlp
* Clear errors in driver prior to parsing.
* Fix bash script flag for -v
* Fully qualify RegisterBenchmark calls.
* Package proxify layer on container_image
* Remove unused import httpbody.proto
* Update visibility target to public target in common repo
* Use set for cache key lookups


### GCP: Features

* **GCP:** add capability to use existing service_mesh


### Documentation

* Add a top level getting_started directory.
* Add docs about AWS AMI structure
* Add instructions on how to call UDF APIs from C++ WASM.
* Update sharding docs with GCP-specific info


### Dependencies

* **deps:** Update data-plane-shared to b463f16
* **deps:** Utilize rules_closure deps via data-plane-shared

## 0.13.0 (2023-10-10)


### Features

* Add bazel config for code coverage
* Add proto definition for UDF input.
* Add string set support in udf delta tester.
* Add support to output graphviz dot file when playing with queries
* Move server_docker_image.tar to dist/
* Set bazel version to 6.3.2
* Set shard id label for metrics
* Update to latest PublicKeyFetcher


### Bug Fixes

* Add seccomp flag to functional tests for local runs
* Allow changelog file to contain non-ascii chars
* bump the terraform version to v1.2.3
* Correct bazel config_setting visibility
* Fix blob_storage_client_gcp's polling frequency
* Fix excessive logging of unmock methods which cause flaky test in local
* Move server_docker_image.tar into dist dir


### GCP: Features

* **GCP:** add GCP blob storage client.


### Dependencies

* **deps:** Upgrade build-system to 0.45.0


### Documentation

* [Realtime updates] Update docs to reflect GCP details
* Modify some sentences that I found hard to read.
* Sharding
* Sharding -- metrics

## 0.12.0 (2023-09-14)


### Features

* [Coordinators] moving /server/bin to /
* [GCP] Add realtime notifier tool
* [sharding] Add metrics for set query
* A delta based request generator that creates KV requests from delta files
* A generic grpc client that sends a request and returns a response
* Add ability to point to real coordinators through parameters
* Add bazel configs for roma legacy vs sandboxed
* Add bazel macro to generate UDF delta file from wasm binary and JS
* Add delta based request generator to the simulation system
* Add delta file notifier and loader to the request simulation system
* Add Dependencies section to release notes
* Add functions for serializing and deserializing shard mapping records
* Add GCP features and fixes sections in release notes
* add GCP platform and instance
* Add GCP realtime notifier
* Add GCP terraform config for metrics collector
* Add logical sharding config schema and constants
* Add metrics collector endpoint to the parameter
* Add metrics collector to periodically print and publish metrics
* Add NAT gateway for public internet access
* Add OSSF Scorecard badge to top-level README
* Add OSSF Scorecard GitHub Action
* add service mesh to GCP platform
* Add support to execute queries over sharded sets.
* Add tool to convert C++ to JS with inline WASM
* add unit tests to gcp parameter client
* Add version to UdfConfig.
* Adding GCP version of the message service
* Build AMI for request simulation system
* Coordinators: Add missing permission
* Deploy otel with request simulation system to AWS
* Enable metrics and tune the performance for request simulation system
* GCP terraform and parameter client refactor
* Generate synthetic requests at fixed rate
* grpc client worker to send requests at configurable QPS
* Integrating kv value server with the KeyFetcherManagerInterface
* Register get values hook for string and binary output format
* Run request simulation system in local
* Skip delta and snapshot files that belong to other shards
* Support writing shard mapping configs from csv
* Upgrade build-system to v0.33.0
* Upgrade data-plane-shared-libraries to 2023-07-12 commit.
* Upgrade data-plane-shared-libraries to 2023-07-21 commit.
* Upgrade data-plane-shared-libraries to 2023-07-26 commit.
* Upgrade data-plane-shared-libraries to 2023-08-16 commit.
* use local parameter client for gcp platform's local instance


### Bug Fixes

* Acquire read lock on the set before iterating over it.
* Add action_env for asan bazel config
* Add noexcept bazel config
* Add seccomp-unconfined flag to build_and_test_all_in_docker
* Check that fb strings are not nullptr.
* Check the metrics collector connection during telemetry initialization
* Do not pass metrics collector endpoint for local and aws instance
* Don't move references in sample udf code.
* Explicitly cast return values of set operations to r-value references
* fix another asan error in the test
* Fix data loading num threads in param client local test
* Fix grpc client error
* Fix the error messages printed in the unit test logs
* Fix UDF function handler name
* logMessage should set an output string.
* make the response outlive grpc client call
* Rearrange bazel config for clarity
* Reduce the number of client workers to 2 in the unit tests to limit the number of threads created
* remove GRPC 4mb payload limit
* remove local variant of component tools.
* Remove redundant docker security-opt
* Set bazel workspace name
* Write data record for set data in the delta test file generator


### Dependencies

* **deps:** Upgrade build-system to 0.42.1
* **deps:** Upgrade build-system to v0.41.1
* **deps:** Upgrade emscripten to 3.1.44


### GCP: Features

* **GCP:** Add realtime thread pool manager


### Documentation

* Add docs on getValuesBinary API
* Realtime directory is not optional for local dev
* Update docker run instructions to include security-opt flag
* Update inline WASM docs with instructions on how to test it
* Update screenshot of delta file

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
