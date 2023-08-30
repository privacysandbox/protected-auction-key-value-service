# GCP Key Value Server Terraform vars documentation

-   **backup_poll_frequency_secs**

    Backup poll frequency for delta file notifier in seconds.

-   **cpu_utilization_percent**

    CPU utilization percentage across an instance group required for autoscaler to add instances.

-   **data_bucket_id**

    Directory to watch for files.

-   **data_loading_num_threads**

    Number of parallel threads for reading and loading data files.

-   **directory**

    Directory to watch for files.

-   **environment**

    Assigned environment name to group related resources. Also servers as gcp image tag.

-   **gcp_image_repo**

    A URL to a docker image repo containing the key-value service.

-   **instance_template_waits_for_instances**

    True if terraform should wait for instances before returning from instance template application.
    False if faster apply is desired.

-   **kv_service_port**

    The grpc port that receives traffic destined for the frontend service.

-   **launch_hook**

    Launch hook.

-   **machine_type**

    Machine type for the key-value service. Must be compatible with confidential compute.

-   **max_replicas_per_service_region**

    Maximum amount of replicas per each service region (a single managed instance group).

-   **metrics_export_interval_millis**

    Export interval for metrics in milliseconds.

-   **metrics_export_timeout_millis**

    Export timeout for metrics in milliseconds.

-   **min_replicas_per_service_region**

    Minimum amount of replicas per each service region (a single managed instance group).

-   **num_shards**

    Total number of shards.

-   **project_id**

    GCP project id.

-   **realtime_directory**

    Local directory to watch for realtime file changes.

-   **realtime_updater_num_threads**

    Amount of realtime updates threads locally.

-   **regions**

    Regions to deploy to.

-   **route_v1_to_v2**

    Whether to route V1 requests through V2.

-   **s3client_max_connections**

    S3Client max connections for reading data files.

-   **s3client_max_range_bytes**

    S3Client max range bytes for reading data files.

-   **service_account_email**

    Email of the service account that be used by all instances.

-   **udf_num_workers**

    Number of workers for UDF execution.

-   **use_confidential_space_debug_image**

    If true, use the Confidential space debug image. Else use the prod image, which does not allow
    SSH. The images containing the service logic will run on top of this image and have their own
    prod and debug builds.

-   **use_real_coordinators**

    Use real coordinators.

-   **vm_startup_delay_seconds**

    The time it takes to get a service up and responding to heartbeats (in seconds).