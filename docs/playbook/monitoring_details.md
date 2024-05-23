# MonitoringDetails

## Otel collector configuration

All server metrics are collected by the
[OpenTelemetry collector](https://opentelemetry.io/docs/collector/). The otel collector will not run
in the trusted environment. On AWS, the collector runs on the same EC2 host outside the enclave in
which the TEE server is running. On GCP, the collector runs in an individual VM instance which is
deployed with KV server's out of box
[terraform configuration](../../production/terraform/gcp/services/metrics_collector/main.tf), and a
[collector end point](../../production/terraform/gcp/environments/demo/us-east1.tfvars.json#L7) is
set up during deployment for the KV server instances to connect to it.

We provide default configurations for the otel collector:
[GCP](../../production/terraform/gcp/services/metrics_collector_autoscaling/collector_startup.sh)
and [AWS](../../production/packaging/aws/otel_collector/otel_collector_config.yaml), Adtech can
modify them without affecting the code running inside the TEE. The default setup enable the otel
collector to export the following server telemetry data to AWS and GCP:

-   **Metrics**
    -   AWS
        -   AWS Cloudwatch
        -   [Amazon Managed Prometheus](https://aws.amazon.com/prometheus/)
    -   GCP
        -   [Google Cloud Monitoring](https://cloud.google.com/monitoring?hl=en)
-   **Logging** (Include
    [consented request logging](https://github.com/privacysandbox/protected-auction-services-docs/blob/main/debugging_protected_audience_api_services.md)
    and non-request related logging)
    -   AWS:
        [AWS Cloudwatch Logs](https://docs.aws.amazon.com/AmazonCloudWatch/latest/logs/WhatIsCloudWatchLogs.html)
    -   GCP: [Google Cloud Logging](https://cloud.google.com/logging?hl=en)
-   **Traces**
    -   AWS: [AWS Xray](https://aws.amazon.com/xray/)
    -   GCP: [Google Cloud Trace](https://cloud.google.com/trace)

## Server telemetry configuration

The following are metrics related configurations and can be configured by server parameters.
Examples:
[AWS demo terraform setup](../../production/terraform/aws/environments/demo/us-east-1.tfvars.json),
[GCP demo terraform setup](../../production/terraform/gcp/environments/demo/us-east1.tfvars.json).

#### metrics_export_interval_millis

Export interval for metrics in milliseconds

#### metrics_export_timeout_millis

Export timeout for metrics in milliseconds

#### telemetry_config

Telemetry configuration to specify the mode for metrics collection. This mode is to control whether
metrics are collected raw or with noise. More details about telemetry configuration and how it works
with non_prod and prod build flavors are
[here](https://github.com/privacysandbox/protected-auction-services-docs/blob/main/monitoring_protected_audience_api_services.md#configuring-the-metric-collection-mode)

## Common attributes

All the metrics tracked in the KV server are tagged with the following attributes as additional
metrics dimensions.

| Attributes name        | Description                                                                                |
| ---------------------- | ------------------------------------------------------------------------------------------ |
| service.name           | The name of the service that the metric is measured on. It is a constant value "kv-server" |
| service.version        | The current version number of the server binary in use                                     |
| host.arch              | The CPU architecture the server's host system is running on                                |
| deployment.environment | The environment in which the server is deployed on                                         |
| service.instance.id    | The id of the machine instance the server is running on                                    |
| shard_number           | The shard number that the server instance belongs to                                       |

## Metrics definitions

All metrics available to KV server are defined in
[server_definition.h](../../components/telemetry/server_definition.h). Here is a list of common
properties in the metric definition and their meanings.

### Privacy definition

| Property               | Description              |
| ---------------------- | ------------------------ |
| Privacy::kImpacting    | Metric is noised with DP |
| Privacy::kNonImpacting | Metric is not noised     |

### Instrument type

#### Instrument::kUpdownCounter

Counter instrument implemented with
[Otel UpDownCounter](https://opentelemetry.io/docs/specs/otel/metrics/api/#updowncounter)

#### Instrument::kPartitionedCounter

Partitioned counter instrument implemented with
[Otel UpDownCounter](https://opentelemetry.io/docs/specs/otel/metrics/api/#updowncounter). This
instrument allows the metric to be partitioned by an additional dimension with values defined in the
partition list.

#### Instrument::kHistogram

Histogram instrument implemented with
[Otel Histogram](https://opentelemetry.io/docs/specs/otel/metrics/api/#histogram)

#### Instrument::Gauge

Gauge Instrument implemented with
[Otel Gauge](https://opentelemetry.io/docs/specs/otel/metrics/api/#gauge). Observable metrics such
as CPU and memory utilization are collected using this instrument

## Differential privacy and noising

Metrics in the request path after request is decrypted are considered privacy-sensitive thus will be
noised with differential privacy(DP). The level of noise depends on several factors, notably the
available privacy budget. The higher the budget, the less noise added to the metric. The
[total privacy budget](../../components/telemetry/server_definition.h#L96) for the server is
specified as a constant value to encompass all tracked privacy-sensitive metrics. The total privacy
budget is by default distributed evenly across all tracked privacy-sensitive metrics, we also allow
Adtech to customize the distribution across different metrics. And other supported noise related
configurations are described
[here](https://github.com/privacysandbox/protected-auction-services-docs/blob/main/monitoring_protected_audience_api_services.md#noise-related-configuration).
In addition, Adtech can also
[configure a set of metrics to monitor](https://github.com/privacysandbox/protected-auction-services-docs/blob/main/monitoring_protected_audience_api_services.md#configuring-collected-metrics),
the set of metrics will need to be selected from the defined KV metrics in
[server_definition.h](../../components/telemetry/server_definition.h).

Metrics for
[consented requests](https://github.com/privacysandbox/protected-auction-services-docs/blob/main/debugging_protected_audience_api_services.md)
will be unnoised and can be filtered by request's generation_id (a unique identifier passed by
client via LogContext proto in [V2 request API](../../public/query/v2/get_values_v2.proto), or a
default constant value "consented" if generation_id is not provided by client).

More detailed information about differential privacy used in the Protected Audience TEE servers is
in this
[doc](https://github.com/privacysandbox/protected-auction-services-docs/blob/main/monitoring_protected_audience_api_services.md#differential-privacy-and-noising).

## Error codes

All the server error codes are defined in [error_code.h](../../components/telemetry/error_code.h).
These error codes are used to report error metrics mentioned [here](total_error_rate_too_high.md)
