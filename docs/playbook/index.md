# Key Value Server Playbook

This article lists important metrics ad techs should monitor and set up alerts for.

There are other metrics that you should probably monitor and have alerts for, based on your use case
and service level indicator guarantees (SLIs). Further guidance will be available when Key Value
Server (KV server) moves to general availability (GA).

## Important metrics

For each metric, review our recommendations to get set up, verify an issue, and troubleshoot common
errors:

-   [Uptime](./server_is_unhealthy.md)
-   [Read latency](./read_latency_too_high.md)
-   [Total error rate](./total_error_rate_too_high.md)

## Debugging

[Debugging Protected Audience API Services](https://github.com/privacysandbox/protected-auction-services-docs/blob/main/debugging_protected_audience_api_services.md)
provides an overview of ways to debug a key-value server. Important points are made about trade-offs
between privacy and ease of debugging.

Note that KV Server does not currently support all the features mentioned in this document, but
these features on are on our roadmap and will be available when we move to GA.

## Monitoring

[Monitoring Protected Audience API Services](https://github.com/privacysandbox/protected-auction-services-docs/blob/main/monitoring_protected_audience_api_services.md)
has high-level suggestions for Protected Audience monitoring. All metrics available to KV server are
defined in [server_definition.h](../../components/telemetry/server_definition.h), see details in
[MonitoringDetails](monitoring_details.md).

### Logs

Our default terraform parameters set the variable `enable_otel_logger` to true. Our default set up
starts up oTel collector.

#### AWS logs

[AWS](../../production/terraform/aws/environments/kv_server_variables.tf#L271)

Find logs in Cloud Watch with the group name and log stream specified under
[awscloudwatchlogs](../../production/packaging/aws/otel_collector/otel_collector_config.yaml#L45)

#### GCP logs

[GCP](../../production/terraform/gcp/environments/kv_server_variables.tf#L290)

You can find your logs in `Logs Explorer`.

### Dashboard

[AWS dashboard](../../production/terraform/aws/services/dashboard/main.tf)

[GCP dashboard](../../production/terraform/gcp/services/dashboards/main.tf)

## Typical problems

### How do I know if my system is healthy?

The two most important things are [Uptime](./server_is_unhealthy.md) and
[Read latency](./read_latency_too_high.md).

You should also monitor logs to make sure you don't see any errors in there.

Beyond that, you can verify that you can [load data](../data_loading/loading_data.md) and then
[read](../testing_the_query_protocol.md) it.

### How do I know if my read latency is meeting my SLI?

Please see [Read latency](read_latency_too_high.md).

### My realtime messages don't go through. How to fix it?

Please review this [Realime updates](../data_loading/loading_data.md#realtime-updates) section.

This document provides sample code to send updates to different cloud providers, from the console as
well as from code.

Try sending an update using the steps described in the document, and see if it goes through.

[AWS Realtime update capabilities explains](../data_loading/realtime_updates_capabilities.md)
explains how to further tune and test real-time updates.

For real-time updates, pay attention to the
[kReceivedLowLatencyNotificationsE2E](../../components/telemetry/server_definition.h#L311) metric.

## Escalation

### Cloud specific

#### AWS

[Support](https://aws.amazon.com/contact-us/)

#### GCP

[Support](https://cloud.google.com/support?hl=en)

### Privacy Sandbox

#### KV server

Our oncall is responsible for monitoring the issues created for this repo.

Alternatively, you can email us at <privacy-sandbox-trusted-kv-server-support@google.com>
