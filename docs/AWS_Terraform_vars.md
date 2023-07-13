# AWS Key Value Server Terraform vars documentation

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

-   **data_loading_num_threads**

    the number of concurrent threads used to read and load a single delta or snapshot file from blob
    storage.

-   **enclave_cpu_count**

    Set how many CPUs the server will use.

-   **enclave_memory_mib**

    Set how much RAM the server will use.

-   **environment**

    The value can be any arbitrary unique string (there is a length limit of ~10), and for example,
    strings like `staging` and `prod` can be used to represent the environment that the Key/Value
    server will run in.

-   **healthcheck_healthy_threshold**

    Consecutive health check successes required to be considered healthy

-   **healthcheck_interval_sec**

    Amount of time between health check intervals in seconds.

-   **healthcheck_unhealthy_threshold**

    Consecutive health check failures required to be considered unhealthy.

-   **instance_ami_id**

    Set the value to the AMI ID that was generated when the image was built.

-   **instance_type**

    Set the instance type. Use instances with at least four vCPUs. Learn more about which types are
    supported from the
    [AWS article](https://docs.aws.amazon.com/enclaves/latest/user/nitro-enclave.html).

-   **metrics_export_interval_millis**

    Export interval for metrics in milliseconds.

-   **metrics_export_timeout_millis**

    Export timeout for metrics in milliseconds.

-   **mode**

    Set the server mode. The acceptable values are [DSP] or [SSP]

-   **num_shards**

    Total number of shards

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

-   **s3_delta_file_bucket_name**

    Set a name for the bucket that the server will read data from. The bucket name must be globally
    unique. This bucket is different from the one that was manually created for Terraform states
    earlier.

-   **s3client_max_connections**

    S3 Client max connections for reading data files.

-   **s3client_max_range_bytes**

    S3 Client max range bytes for reading data files.

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

-   **udf_num_workers**

    Total number of workers for UDF execution

-   **vpc_cidr_block**

    CIDR range for the VPC where KV server will be deployed.
