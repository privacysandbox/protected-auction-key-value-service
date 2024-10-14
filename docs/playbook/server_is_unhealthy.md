# ServerIsUnhelathy

## Overview

This alert means that kv server is unhelathy and cannot fully serve traffic.

If you see this alert, there is a big chance you'll see most, if not all, other alerts firing too.

## Background

AdTechs can define their uptime off the load balancer response codes.

Specifically, they would get a distribution over a period of time K seconds of 2xx's - Y%, which
AdTechs can map to "up" and the rest to "down".

KV server is a grpc server which returns grpc status codes. Those are mapped to http codes as
explained
[here](https://chromium.googlesource.com/external/github.com/grpc/grpc/+/refs/tags/v1.21.4-pre1/doc/statuscodes.md)

## Recommended alert level

An AdTechs starts the downtime stopwatch once 10% error rate (non-2xx's responses) is hit. An alert
is fired after 1 minute of 10% error rate (non-2xx's responses). Once the error rate goes down below
10% the stopwatch is stopped. Once the error rate is below the 10% for 10 continuous seconds, the
alert auto resolves.

## Alert Severity

Critical.

The service is down, and is unavailable to serve read requests.

This condition directly affects the uptime SLO.

## Verification

### Load balancer

#### GCP

[Load balancers](https://cloud.google.com/load-balancing/docs/https/https-logging-monitoring) come
with all sorts of monitoring for different response codes, request counts, latency, throughput etc
with easy ways of setting up alerts.

#### AWS

Similarly, AWS load balancers and target groups provide breakdowns by code and other metrics, on top
of which alerts can be defined.

### Mesh / Traffic director (internal load balancer)

#### GCP

[Less automated](https://cloud.google.com/traffic-director/docs/control-plane-observability) than
the external load balancer.

Operator needs to enable cloud logging for the backend service and then have a dashboard that
monitors the access logs for 500s, and other metrics.

#### AWS

This is being defined atm, but it's probably going to be similar to GCP.

## Shard cluster uptime definition

A shard can be down, but KV still serves requests.

Autoscaling group level alerts can be set up, for example, on the minimum number of available
healthy machines.

### GCP

Alerts can be defined for instanceGroups. In addition, this resource has good monitoring dashboards
with different tiles, as well as error logs.

### AWS

Alerts can defined for autoscaling groups. A number of userful tiles for the default dashboard is
provided. Logs are available to see the actions that are taken by the autoscaling groups to detect
anomalies.

#### Other signals for uptime

AWS and GCP have vm/ec2 level checks.

#### Checking a specific kv server instance

_Http_

```sh
curl $YOURSERVERURL/healthcheck
```

should return

```json
{
    "status": "SERVING"
}
```

_GRPC_

Run from the repo root, since you need access to the \*.pb file

```sh
grpcurl --protoset dist/query_api_descriptor_set.pb $YOURSERVERURL:8443 grpc.health.v1.Health/Check
```

should return the same response above, as the http call.

Additionally, any read requests will fail. You can run try

```sh
curl $YOURSERVERURL/v1/getvalues?keys=hi
```

A healthy response is

```json
{
    "keys": {
        "hi": {
            "value": "Hello, world! If you are seeing this, it means you can query me successfully"
        }
    },
    "renderUrls": {},
    "adComponentRenderUrls": {},
    "kvInternal": {}
}
```

## Troubleshooting & Solution

The fact that there are no healthy machines in kv's autoscaling group(s) means that the autoscaling
group manager tried to rotate in new machines, but failed.

You can check out the autoscaling events to see when that started happening.

For AWS: pay particular attention to "TerminateInstances" events that you can query on CloudTrail.

## Solution

If this alert is firing, it means that something went wrong in a big way.

You should check the metrics dashboard and logs, that are linked for you cloud [here](index.md).

Metrics noising and other privacy enhancing observability frameworks should not interfere with
troubleshooting too much for this alert.

### Out of memory

In the dashboard check the memory consumption and see if it comes close to the threshold value
(total available memory), and then the machine disappears.

The solution here is to remove the _excess_ data loaded through the standard and fast path. And then
add sharding, if necessary.

### Out of cpu

Similarly, check the cpu and how close it is to 100%.

The solution here is to add more machines to the autoscaling group or change your hardware to be
more powerful. Additionally, it might be necessary to speed up how quickly the autoscaling group is
perfroming machines rotation.

You could turn off read traffic for your server, as part of debugging. You can check if the spike in
traffic looks like a DDOS attack, and act accordingly.

Lastly, you need to analyze if you can optimize CPU consuming tasks, e.g. your UDFs.

### Out of disc

Similarly, check if you're out of disk. If you are -- you can bump up the amount of disk you're
using by updating the hardware, and also how much you allocate to the enclave. You should analyze
which part of your disc usage is growing, e.g. mb your logs are stored on the disc and are bound to
run out space. In that case you need to figure out a proper log rotation strategy.

### An implementation bug

It might be that some incorrect implementation hit an edge case. It might be helpful to turn up log
verbosity and analyze the last few entries before the machine crashes.

A common technique to address this bug is to revert to a previous more stable build.

If you believe that this is a KV server issue, you should escalate using the info from
[here](index.md).

## Related Links

[Server initialization](../server_initialization.md) provides extra details on the server
initialization lifecycle and how it affects the health check.
