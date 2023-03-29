# Overview

Do performance tests on given server and collect request level metrics such as latency p90, p95,
p99, min and max. Visualize data on Perfgate.

## Performance test

[Apache ab](https://httpd.apache.org/docs/2.4/programs/ab.html) will be used for performance
testing. It is available through the build-system git submodule.

Usage

```shell
# From aggregate-service root

builders/tools/ab -n <num-requests> -c <concurrency> -e <metrics-file-csv> url
```

## Perfgate

Perfgate (go/perfgate) will be used for storage and visualization of metrics. A protobuf for a run
and metrics aggregate can be uploaded to perfgate using the uploader (Pending implementation
b/267095784).

## Script

A helper script `perf_test.sh` is implemented to automate the process. The script generates a run
protobuf, runs Apache ab to generate a metrics file, generates a metrics aggregate protobuf, uploads
the the run and metrics aggregate protobuf to perfgate and then cleans up.

Usage

```shell
tests/performance_tests/perf_test <cl-number> <perfgate-benchmark-key> \
    <concurrency> <num-requests> <url> <perfgate-metric-key>
```
