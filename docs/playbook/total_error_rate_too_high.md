# TotalErrorRateTooHigh

## Overview

The number of total errors has crossed an alert threshold.

## Recommended alert level

Number of errors in the past 5 minutes is over 100.

Note that this metric is noised for privacy purposes, and as such might actually show a slightly
different number from the actual measurement.

## Alert Severity

High. While it does not necessarily mean that the server is down or that any critical flows are
affected, there is a high chance that they are.

## Verification

Check out the [metric](../../components/telemetry/server_definition.h#100),
[metric](../../components/telemetry/server_definition.h#154),
[metric](../../components/telemetry/server_definition.h#301) and see if it's outside of the expected
range.

Dashboards:

AWS:

[Request Errors](https://github.com/privacysandbox/protected-auction-key-value-service/blob/552934a1e1e8d1a8beed4474408127104cdf3207/production/terraform/aws/services/dashboard/main.tf#L176),
[Internal Request Errors](https://github.com/privacysandbox/protected-auction-key-value-service/blob/552934a1e1e8d1a8beed4474408127104cdf3207/production/terraform/aws/services/dashboard/main.tf#L198),
[Server Non-request Errors](https://github.com/privacysandbox/protected-auction-key-value-service/blob/552934a1e1e8d1a8beed4474408127104cdf3207/production/terraform/aws/services/dashboard/main.tf#L220)

GCP:

[Request Errors](https://github.com/privacysandbox/protected-auction-key-value-service/blob/552934a1e1e8d1a8beed4474408127104cdf3207/production/terraform/gcp/services/dashboards/main.tf#L259),
[Internal Request Errors](https://github.com/privacysandbox/protected-auction-key-value-service/blob/552934a1e1e8d1a8beed4474408127104cdf3207/production/terraform/gcp/services/dashboards/main.tf#L301),
[Server Non-request Errors](https://github.com/privacysandbox/protected-auction-key-value-service/blob/552934a1e1e8d1a8beed4474408127104cdf3207/production/terraform/gcp/services/dashboards/main.tf#L343)

## Troubleshooting and solution

Figure out what specific error is firing. You can group by the error by dimensions, and most likely
only one of those if firing. Having that knowledge, check the [logs](index.md).

Can it be that this is a transient error due to some temporary outage? Can this be cause by a recent
regression in the codebase? Beyond that it's hard to give a specific recommendation other than
analyzing the code and trying to understand what might have changed in the recent usage patterns.

## Escalation

You might want to [escalate](index.md) to your cloud provider or to privacy sandbox based on the
nature of the error.
