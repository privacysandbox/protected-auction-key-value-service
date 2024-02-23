# GCP Key Value Server Terraform vars documentation

-   **backup_poll_frequency_secs**

    Backup poll frequency for delta file notifier in seconds.

-   **collector_dns_zone**

    Google Cloud DNS zone name for collector.

-   **collector_domain_name**

    The domain name for metrics collector

-   **collector_machine_type**

    The machine type for metrcis collector

-   **collector_service_name**

    The metrics collector service name

-   **collector_service_port**

    The grpc port that receives traffic destined for the OpenTelemetry collector

-   **cpu_utilization_percent**

    CPU utilization percentage across an instance group required for autoscaler to add instances.

-   **data_bucket_id**

    Directory to watch for files.

-   **data_loading_num_threads**

    Number of parallel threads for reading and loading data files.

-   **enable_external_traffic**

    Whether to serve external traffic. If disabled, only internal traffic via service mesh will be
    served.

-   **environment**

    Assigned environment name to group related resources. Also servers as gcp image tag.

-   **envoy_port**

    External load balancer will send traffic to this port. Envoy will forward traffic to
    kv_service_port. Must match envoy.yaml. Ignored if `enable_external_traffic` is false.

-   **existing_service_mesh**

    Existing service mesh. This would only be used if use_existing_service_mesh is true.

-   **existing_vpc_id**

    Existing vpc id. This would only be used if use_existing_vpc is true.

-   **gcp_image_repo**

    A URL to a docker image repo containing the key-value service.

-   **gcp_image_tag**

    Tag of the gcp docker image uploaded to the artifact registry.

-   **instance_template_waits_for_instances**

    True if terraform should wait for instances before returning from instance template application.
    False if faster apply is desired.

-   **kv_service_port**

    The grpc port that receives traffic destined for the frontend service.

-   **logging_verbosity_level**

    Logging verbosity level

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

-   **primary_coordinator_account_identity**

    Account identity for the primary coordinator.

-   **primary_key_service_cloud_function_url**

    Primary workload identity pool provider.

-   **primary_workload_identity_pool_provider**

    Primary key service cloud function url.

-   **project_id**

    GCP project id.

-   **realtime_updater_num_threads**

    Amount of realtime updates threads locally.

-   **regions**

    Regions to deploy to.

-   **regions_use_existing_nat**

    Regions that use existing nat. No new nats will be created for regions specified here.

-   **route_v1_to_v2**

    Whether to route V1 requests through V2.

-   **secondary_coordinator_account_identity**

    Account identity for the secondary coordinator.

-   **secondary_key_service_cloud_function_url**

    Secondary key service cloud function url.

-   **secondary_workload_identity_pool_provider**

    Secondary workload identity pool provider.

-   **server_dns_zone**

    Dns zone for Kv-serer. Ignored if `enable_external_traffic` is false.

-   **server_domain_ssl_certificate_id**

    Ssl certificate id of the Kv-server domain. Ignored if `enable_external_traffic` is false.

-   **server_url**

    Kv-serer URL. Example: kv-server-environment.example.com. Ignored if `enable_external_traffic`
    is false.

-   **service_account_email**

    Email of the service account that be used by all instances.

-   **service_mesh_address**

    Service mesh address of the KV server.

-   **tee_impersonate_service_accounts**

    Tee can impersonate these service accounts. Necessary for coordinators.

-   **telemetry_config**

    Telemetry configuration to control whether metrics are raw or noised. Options are: mode:
    PROD(noised metrics), mode: EXPERIMENT(raw metrics), mode: COMPARE(both raw and noised metrics),
    mode: OFF(no metrics)

-   **udf_num_workers**

    Number of workers for UDF execution.

-   **use_confidential_space_debug_image**

    If true, use the Confidential space debug image. Else use the prod image, which does not allow
    SSH. The images containing the service logic will run on top of this image and have their own
    prod and debug builds.

-   **use_existing_service_mesh**

    Whether to use existing service mesh.

-   **use_existing_vpc**

    Whether to use existing vpc network.

-   **use_external_metrics_collector_endpoint**

    Whether to use metrics collector on external host. On gcp, this should always be true because
    OpenTelemetry endpoints need to run on a different host

-   **use_real_coordinators**

    Use real coordinators.

-   **vm_startup_delay_seconds**

    The time it takes to get a service up and responding to heartbeats (in seconds).
