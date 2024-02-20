# AWS Realtime update capabilities

## Overview

A parameter
([AWS](https://github.com/privacysandbox/fledge-key-value-service/blob/7f3710b1f1c944d7879718a334afd5cb8f80f3d9/production/terraform/aws/environments/kv_server.tf#L51),
[GCP](../docs/GCP_Terraform_vars.md#L96)) sets the size of the thread pool that reads off a queue.
The bigger that number is, the smaller the batch size can be. It is preferred to use a larger batch
size where possible.

### AWS

For example, with 4 threads, we can get to the update-level batch size of 250.

On the other hand, the update-level batch size can be as big as an SQS message allows it to be -
[256K](https://docs.aws.amazon.com/AWSSimpleQueueService/latest/SQSDeveloperGuide/quotas-messages.html).

### GCP

The tests were performed on the batch sizes as small as 125. The implementation was well within the
SLO even with thread pool size 1. Increasing the size of the thread pool decreased the latency
further, as expected.

There is a [limit](https://cloud.google.com/pubsub/quotas#resource_limits) of 10MB for the message
size.

## Bottleneck

### AWS

Our current bottleneck is the amount of time it takes to read a batch of messages off an SQS queue.
p50 is 80ms, as measured on AWS EC2 instances. This is inline with AWS
[docs](https://aws.amazon.com/sqs/faqs/) that state: "Typical latencies for .. ReceiveMessage .. are
in the tens or low hundreds of milliseconds."

The maximum batch size that can be read off SQS is 10 messages. So the maximum number of messages we
can read off an SQS per second is 10 \* 1000 / 80 =125. This does not include deletion time and
processing time, so the actual number is smaller.

### GCP

While similar logic applies, the GCP SDK has superior performance due to
[streaming pull](https://cloud.google.com/pubsub/docs/pull#streamingpull_api) support.

## Multiple threads

To get to a higher QPS we can have multiple threads reading off a queue. This is a parameter
([AWS](https://github.com/privacysandbox/fledge-key-value-service/blob/7f3710b1f1c944d7879718a334afd5cb8f80f3d9/production/terraform/aws/environments/kv_server.tf#L51),
[GCP](../docs/GCP_Terraform_vars.md#L96)) that our solution exposes. It can be increased to match
specific QPS requirements and underlying hardware - based on the number of cores.

## Batching

However, there is a limit to this approach, since there are only that many actual physical cores,
and even when a thread is blocked on the network, context switching between threads adds overhead.
We also need to be able to have other threads do their work as well, e.g. reads and slow path write.

The second parameter that can be leveraged is batching multiple updates in a single message. See the
data below with details on the measurements showing the relationship between the batch size, the
insertion rate and the number of threads reading off a queue.

It is preferred to use a larger batch size where possible. That minimizes the number of messages
that need to be fanned out and propagated, which is where most of the time is spent.

## Measuring latency and QPS

### Test data parameters

-   45K records per second for 5 mins
-   All unique keys, all updates
-   Length - key - 30 characters, value - 10 chars
-   Server side batching - 10
-   No other server activity, e.g. other reads, quick/slow writes
-   No data loaded to the server

_AWS specific_

-   Hardware - m5.xlarge

_GCP specific_

-   Hardware - n2d-standard-4 - note that `n2d-standard-4` is almost identical to AWS's `m5.xlarge`
    The above assumptions show an idealized scenario when no other activity is occuring.

Note that the number of realtime updates threads is 1 for GCP, and 4 for AWS, unless otherwise
stated.

The GCP implementation has superior performance, so the only time when the thread pool size had to
be increased was when we got down to the batch size of 125.

### Terms

_Rate_ below means the number of messages inserted to the Topic per second.

_Server side processing_ is the amount of time it takes from the point in time we get the messages
(after they were deleted in the queue) and until they were applied to the cache.

_E2E_ measures from the point in time right before the message is sent from client to SNS until the
update is fully applied.

#### AWS

_Reading off SQS_ is the amount of time a thread is waiting on SQS to get the next batch of
messages.

## AWS numbers

### Batch size - 1K, Rate - 45 == Total - 45K, Thread pool size - 4

| Percentile | Reading off SQS | Server side processing | E2E   |
| ---------- | --------------- | ---------------------- | ----- |
| p50        | 77ms            | 3ms                    | 316ms |
| p999       | 505ms           | 34ms                   | 1.2s  |

### Batch size - 500, Rate - 105 = 52,5K, Thread pool size - 4

| Percentile | Reading off SQS | Server side processing | E2E   |
| ---------- | --------------- | ---------------------- | ----- |
| p50        | 77ms            | 3ms                    | 222ms |
| p999       | 622ms           | 17ms                   | 1.7s  |

### Batch size - 250, Rate - 210 = 52,5K, Thread pool size - 4

| Percentile | Reading off SQS | Server side processing | E2E   |
| ---------- | --------------- | ---------------------- | ----- |
| p50        | 47ms            | 4ms                    | 961ms |
| p999       | 1.8s            | 19ms                   | 4.7s  |

Note, that by increasing the number of realtime updates threads from 4 up, we can further decrease
the batch size.

## GCP Numbers

### Batch size - 1K, Rate - 45 == Total - 45K, Thread pool size - 1

| Percentile | Server side processing | E2E  |
| ---------- | ---------------------- | ---- |
| p50        | 2ms                    | 32ms |
| p999       | 5ms                    | 2s   |

### Batch size - 500, Rate - 105 = 52,5K, Thread pool size - 1

| Percentile | Server side processing | E2E  |
| ---------- | ---------------------- | ---- |
| p50        | 1ms                    | 35ms |
| p999       | 3ms                    | 5s   |

### Batch size - 250, Rate - 210 = 52,5K, Thread pool size - 1

| Percentile | Server side processing | E2E  |
| ---------- | ---------------------- | ---- |
| p50        | 450us                  | 67ms |
| p999       | 1.6ms                  | 4.4s |

### Batch size - 125, Rate - 420 = 52,5K, Thread pool size - 1

| Percentile | Server side processing | E2E   |
| ---------- | ---------------------- | ----- |
| p50        | 310us                  | 300ms |
| p999       | 1.1ms                  | 0.9s  |

### Batch size - 125, Rate - 420 = 52,5K, Thread pool size - 4

| Percentile | Server side processing | E2E  |
| ---------- | ---------------------- | ---- |
| p50        | 460us                  | 32ms |
| p999       | 3.6ms                  | 1.6s |

## Tests

You can test our service with our tools.

[Data generation script](https://github.com/privacysandbox/fledge-key-value-service/blob/7f3710b1f1c944d7879718a334afd5cb8f80f3d9/tools/serving_data_generator/generate_load_test_data)
Allows to generate N deltas, with B updates per delta. You can configure the N, and B (batch size).

[Publisher](https://github.com/privacysandbox/fledge-key-value-service/blob/7f3710b1f1c944d7879718a334afd5cb8f80f3d9/components/tools/realtime_updates_publisher.cc#L122)
allows to insert batched updates at the specified rate from a specified folder.

### Getting p values

#### AWS -- Querying Prometheus

You can query Prometheus directly by using the script below. You can update the p value, 0.5 in this
case, to the value you're interested in. AMP_QUERY_ENDPOINT can be found in AWS UI. It is a url for
the prometheus workspace that's created by
[this](https://github.com/privacysandbox/fledge-key-value-service/blob/7f3710b1f1c944d7879718a334afd5cb8f80f3d9/production/terraform/aws/services/telemetry/main.tf#L17)

```sh
AWS_ACCESS_KEY_ID=...
AWS_SECRET_ACCESS_KEY=.. \
AMP_QUERY_ENDPOINT=https://aps-workspaces.us-east-1.amazonaws.com/workspaces/{YOURGUID}/api/v1/query
## sample query for histogram
QUERY='query=histogram_quantile(0.5,rate(ReceivedLowLatencyNotificationsE2E_bucket[20m]))'
## sample query for counter
QUERY='query=rate(EventStatus{event="RealtimeTotalRowsUpdated"}[1m])'

docker run --rm -it okigan/awscurl --access_key $AWS_ACCESS_KEY_ID  --secret_key $AWS_SECRET_ACCESS_KEY  --region us-east-1 --service aps $AMP_QUERY_ENDPOINT -X POST  -H "Content-Type: application/x-www-form-urlencoded" --data $QUERY
```

You can update the metric to the one you're interested.

| Prometheus Metric Name                        | Human readable name        | Type      | Unit        |
| --------------------------------------------- | -------------------------- | --------- | ----------- |
| ReceivedLowLatencyNotificationsE2E            | E2E                        | Histogram | microsecond |
| ReceivedLowLatencyNotifications               | Server side processing     | Histogram | nanosecond  |
| AwsSqsReceiveMessageLatency                   | Reading off SQS            | Histogram | microsecond |
| ReceivedLowLatencyNotificationsE2EAwsProvided | E2E based on AWS timestamp | Histogram | microsecond |
| RealtimeTotalRowsUpdated                      | QPS                        | Counter   |             |

Note that to calculate percentiles for `ReceivedLowLatencyNotifications`, you need to query it like
this:

```sh
histogram_quantile(0.5,rate(Latency_bucket{event="ReceivedLowLatencyNotifications"}[10m]))
```

#### GCP -- Using the dahsboard

You can query the prometheus the same way it's done for AWS. Note that KV server doesn't expose
`AwsSqsReceiveMessageLatency`, and `AWS` in the metric name should be substituted with `GCP`.

You can also use the UI
[dashboard](/production/terraform/gcp/dashboards/realtime_pubsub_dashboard.json). Make sure to
replace PROJECT_ID and ENVIRONMENT with your values.
