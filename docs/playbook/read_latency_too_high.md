# ReadLatencyTooHigh

## Overview

Read requests are taking too long.

Most likely the volume of requests is too high and the autoscaling group was not able to add more
capacity.

Alternatively, the UDF logic is taking too long and its logic should be improved.

## Recommended alert level

p50 for 5 mins is over 500 ms, but this is highly dependent on your use case.

Note that this metric is noised for privacy purposes, and as such might actually show a slightly
different number from the actual measurement.

## Alert Severity

High. This affects the server's core flow of serving read requests in time.

## Verification

Check out the
[metric](https://github.com/privacysandbox/data-plane-shared-libraries/blob/5753af60e8cfae76ef2bb35c4cc105d0ac24481d/src/metric/definition.h#L300),
and see if it's outside of the expected range.

[AWS Request Latency](https://github.com/privacysandbox/protected-auction-key-value-service/blob/552934a1e1e8d1a8beed4474408127104cdf3207/production/terraform/aws/services/dashboard/main.tf#L110)
[GCP Request Latency](https://github.com/privacysandbox/protected-auction-key-value-service/blob/552934a1e1e8d1a8beed4474408127104cdf3207/production/terraform/gcp/services/dashboards/main.tf#L148)

## Troubleshooting and solution

### Not enough capacity

Check out the CPU metrics for your machines. They are available in the dashboard linked
[here](index.md).

Consider updating the max capacity limits for your auto scaling groups, and how quickly those kick
in

[AWS](../../production/terraform/aws/services/autoscaling/main.tf#L70)

[GCP](../../production/terraform/gcp/services/autoscaling/main.tf#L144)

### Too many read requests

Check out the
[metric](https://github.com/privacysandbox/data-plane-shared-libraries/blob/5753af60e8cfae76ef2bb35c4cc105d0ac24481d/src/metric/definition.h#L293)
for total requests count, and see if it's outside of the expected range.

[AWS Request Count](https://github.com/privacysandbox/protected-auction-key-value-service/blob/552934a1e1e8d1a8beed4474408127104cdf3207/production/terraform/aws/services/dashboard/main.tf#L44)

[GCP Request Count](https://github.com/privacysandbox/protected-auction-key-value-service/blob/552934a1e1e8d1a8beed4474408127104cdf3207/production/terraform/gcp/services/dashboards/main.tf#L25)

Note that this metric is noised for privacy purposes, and as such might actually show a slightly
different number from the actual measurement.

Consider escalating to the upstream component, if you believe you're getting more requests than you
reasonably should.

### Investigate the UDF

If the number of requests is reasonable and so are CPU numbers, it probably means that the UDF you
have might not be optimally written.

Consider improving the logic, or updating the alert threshold above accordingly.

### Upgrade hardware

You can pick more performant hardware.

## Escalation

If you believe that the number of requests you're getting is too high, consider escalating to
upstream components.

## Related Links

[UDF explainer](https://github.com/privacysandbox/fledge-docs/blob/main/key_value_service_user_defined_functions.md#keyvalue-service-user-defined-functions-udfs)
