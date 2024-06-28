# AWS Key Value Server Terraform vars documentation

-   **add_missing_keys_v1**

    Add missing keys v1.

-   **autoscaling_desired_capacity**

    Number of Amazon EC2 instances that should be running in the autoscaling group

-   **autoscaling_max_size**

    Maximum size of the Auto Scaling Group

-   **autoscaling_min_size**

    Minimum size of the Auto Scaling Group

-   **backup_poll_frequency_secs**

    Interval between attempts to check if there are new data files on S3, as a backup to listening
    to new data files.

-   **certificate_arn**

    If you want to create a public AWS ACM certificate for a domain from scratch, follow
    [these steps to request a public certificate](https://docs.aws.amazon.com/acm/latest/userguide/gs-acm-request-public.html).
    If you want to import an existing public certificate into ACM, follow these steps to
    [import the certificate](https://docs.aws.amazon.com/acm/latest/userguide/import-certificate.html).

-   **consented_debug_token**

    Consented debug token to enable the otel collection of consented logs. Empty token means no-op
    and no logs will be collected for consented requests. The token in the request's consented debug
    configuration needs to match this debug token to make the server treat the request as consented.

-   **data_loading_blob_prefix_allowlist**

    A comma separated list of prefixes (i.e., directories) where data is loaded from.

-   **data_loading_file_format**

    Data file format for blob storage and realtime updates. See /public/constants.h for possible
    values.

-   **data_loading_num_threads**

    the number of concurrent threads used to read and load a single delta or snapshot file from blob
    storage.

-   **enable_consented_log**

    Enable the logging of consented requests. If it is set to true, the consented debug token
    parameter value must not be an empty string.

-   **enable_external_traffic**

    Whether to serve external traffic. If disabled, only internal traffic under existing VPC will be
    served.

-   **enclave_cpu_count**

    Set how many CPUs the server will use.

-   **enclave_enable_debug_mode**

    If you enable debug mode, you can view the enclave's console in read-only mode using the
    nitro-cli console command. Enclaves booted in debug mode generate attestation documents with
    PCRs that are made up entirely of zeros (000000000000000000000000000000000000000000000000). More
    info: <https://docs.aws.amazon.com/enclaves/latest/user/cmd-nitro-run-enclave.html>

-   **enclave_memory_mib**

    Set how much RAM the server will use.

-   **environment**

    The value can be any arbitrary unique string (there is a length limit of ~10), and for example,
    strings like `staging` and `prod` can be used to represent the environment that the Key/Value
    server will run in.

-   **existing_vpc_environment**

    Environment of the existing VPC. Ingored if use_existing_vpc is false.

-   **existing_vpc_operator**

    Operator of the existing VPC. Ingored if use_existing_vpc is false.

-   **healthcheck_grace_period_sec**

    Amount of time to wait for service inside enclave to start up before starting health checks, in
    seconds.

-   **healthcheck_healthy_threshold**

    Consecutive health check successes required to be considered healthy

-   **healthcheck_interval_sec**

    Amount of time between health check intervals in seconds.

-   **healthcheck_timeout_sec**

    Amount of time to wait for a health check response in seconds.

-   **healthcheck_unhealthy_threshold**

    Consecutive health check failures required to be considered unhealthy.

-   **http_api_paths**

    URL paths the load balancer will forward to the server. By default the load balancer will
    forward requests with `/v1/*`, `/v2/*`, and `/healthcheck`.

-   **instance_ami_id**

    Set the value to the AMI ID that was generated when the image was built.

-   **instance_type**

    Set the instance type. Use instances with at least four vCPUs. Learn more about which types are
    supported from the
    [AWS article](https://docs.aws.amazon.com/enclaves/latest/user/nitro-enclave.html).

-   **logging_verbosity_level**

    Logging verbosity level

-   **metrics_collector_endpoint**

    The open telemetry metrics collector endpoint, for AWS it will be empty string and open
    telemetry will default to local grpc endpoint because otel is running on the same EC2 machine

-   **metrics_export_interval_millis**

    Export interval for metrics in milliseconds.

-   **metrics_export_timeout_millis**

    Export timeout for metrics in milliseconds.

-   **num_shards**

    Total number of shards

-   **primary_coordinator_account_identity**

    Primary coordinator account identity.

-   **primary_coordinator_private_key_endpoint**

    Primary coordinator private key endpoint.

-   **primary_coordinator_region**

    Primary coordinator region.

-   **prometheus_service_region**

    Specifies which region to find Prometheus service and use. Not all regions have Prometheus
    service. (See <https://docs.aws.amazon.com/general/latest/gr/prometheus-service.html> for
    supported regions). If this region does not have Prometheus service, it must be created
    beforehand either manually or by deploying this system in that region. At this time Prometheus
    service is needed. In the future it can be refactored to become optional.

-   **prometheus_workspace_id**

    Only required if the region does not have its own Amazon Prometheus workspace, in which case an
    existing workspace id from another region should be provided. It is expected that the workspace
    from that region is created before this terraform file is applied. That can be done by running
    the Key Value service terraform file in that region.

-   **public_key_endpoint**

    Public key endpoint. Can only be overriden in non-prod mode.

-   **realtime_updater_num_threads**

    The number of threads to process real time updates.

-   **region**

    The region that the Key/Value server will operate in. Each terraform file specifies one region.

-   **root_domain**

    Set the root domain for the server. If your domain is managed by
    [AWS Route 53](https://aws.amazon.com/route53/), then you can simply set your domain value to
    the `root_domain` property in the Terraform configuration that will be described in the next
    section. If your domain is not managed by Route 53, and you do not wish to migrate your domain
    to Route 53, you can
    [delegate subdomain management to Route 53](https://docs.aws.amazon.com/Route53/latest/DeveloperGuide/CreatingNewSubdomain.html).

-   **root_domain_zone_id**

    Set the hosted zone ID. The ID can be found in the details of the hosted zone in Route 53.

-   **route_v1_requests_to_v2**

    Whether to route V1 requests through V2

-   **run_server_outside_tee**

    Whether to run the server outside the TEE.

-   **s3_delta_file_bucket_name**

    Set a name for the bucket that the server will read data from. The bucket name must be globally
    unique. This bucket is different from the one that was manually created for Terraform states
    earlier.

-   **s3client_max_connections**

    S3 Client max connections for reading data files.

-   **s3client_max_range_bytes**

    S3 Client max range bytes for reading data files.

-   **secondary_coordinator_account_identity**

    Secondary coordinator account identity.

-   **secondary_coordinator_private_key_endpoint**

    Secondary coordinator private key endpoint.

-   **secondary_coordinator_region**

    Secondary coordinator region.

-   **server_port**

    Set the port of the EC2 parent instance (that hosts the Nitro Enclave instance).

-   **sqs_cleanup_image_uri**

    The image built previously in the ECR. Example:
    `123456789.dkr.ecr.us-east-1.amazonaws.com/sqs_lambda:latest`

-   **sqs_cleanup_schedule**

    How often to clean up SQS

-   **sqs_queue_timeout_secs**

    Clean up queues not updated within the timeout period.

-   **ssh_source_cidr_blocks**

    Source ips allowed to send ssh traffic to the ssh instance.

-   **telemetry_config**

    Telemetry configuration to control whether metrics are raw or noised. Options are: mode:
    PROD(noised metrics), mode: EXPERIMENT(raw metrics), mode: COMPARE(both raw and noised metrics),
    mode: OFF(no metrics)

-   **udf_min_log_level**

    Minimum log level for UDFs. Info = 0, Warn = 1, Error = 2. The UDF will only attempt to log for
    min_log_level and above. Default is 0 (info).

-   **udf_num_workers**

    Total number of workers for UDF execution

-   **udf_update_timeout_millis**

    UDF update timeout in milliseconds. Default is 30000.

-   **use_existing_vpc**

    Whether to use existing VPC. If true, only internal traffic via mesh will be served; variable
    vpc_operator and vpc_environment will be requried.

-   **use_external_metrics_collector_endpoint**

    Whether to use external metrics collector endpoint. For AWS it is false because KV instance
    connects to OpenTelemetry metrics collector running in local host

-   **use_real_coordinators**

    Whether to use real coordinators. Please refer to our trust model:
    <https://github.com/privacysandbox/fledge-docs/blob/main/key_value_service_trust_model.md> on
    details about coordinators. For non-production testing it's better to set this to false to begin
    with and then set this to true before enabling production. For processing production requests
    this flag must be true, otherwise requests will not be decrypted successfully.
    `enclave_enable_debug_mode` should be set to `false` if the attestation check is enabled for
    coordinators. Attestation check is enabled on all production instances, and might be disabled
    for testing purposes only on staging/dev environments.

-   **vpc_cidr_block**

    CIDR range for the VPC where KV server will be deployed.
