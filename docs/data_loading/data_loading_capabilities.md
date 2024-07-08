# Data loading capabilities

See [Generating and loading data guide](loading_data.md) for more information on the process, tools
and libraries available for generating and loading data into KV servers.

## Tuning parameters

At a high level, KV server instances stream data files using a parallel reader capable of reading
different chunks of a single data file concurrently. For AWS S3, the level of concurrency for the
reader and the number of concurrent S3 client connections can be tuned using the following terraform
parameters:

-   [data_loading_num_threads](https://github.com/privacysandbox/fledge-key-value-service/blob/4d6f691b0d12f9604988c14f534f6e91f4025f29/production/terraform/aws/environments/kv_server_variables.tf)
    sets the number of concurrent threads used to read a single data file [default: 16].
-   [s3client_max_connections](https://github.com/privacysandbox/fledge-key-value-service/blob/4d6f691b0d12f9604988c14f534f6e91f4025f29/production/terraform/aws/environments/kv_server_variables.tf)
    sets the maximum number of concurrent connections used by the S3 client [default: 64].

## Benchmarking tool

The data loading benchmark tool can be used to search for optimal
[tuning parameters](#tuning-parameters) that are best suited to specific hardware, memory and
network specs. To build the benchmarking tool for AWS use the following command (note the
`--config=aws_platform` build flag):

```sh
builders/tools/bazel-debian run //production/packaging/tools:copy_to_dist --config=local_instance --config=aws_platform
```

After building, load the tool into docker as follows:

```sh
docker load -i dist/tools_binaries_docker_image.tar
```

See the [Benchmarking on AWS EC2](#benchmarking-on-aws-ec2) section for some examples of how to run
the tool.

## Benchmarking on AWS EC2

Setup:

The results below were obtained using the following setup.

-   m5.2xlarge instance (8 vCPUs, 32 GB ram, upto 10Gbps network bandwidth)
-   input delta file (4,000,000 records, each record ~512 bytes in size)

Full benchmark command:

```sh
AWS_ACCESS_KEY_ID=...
AWS_SECRET_ACCESS_KEY=...
AWS_DEFAULT_REGION=us-east-1
docker run -it --rm \
    --env AWS_DEFAULT_REGION \
    --env AWS_ACCESS_KEY_ID \
    --env AWS_SECRET_ACCESS_KEY \
    --entrypoint=/tools/benchmarks/data_loading_benchmark \
    bazel/production/packaging/tools:tools_binaries_docker_image \
    --benchmark_time_unit=ms \
    --benchmark_counters_tabular=true \
    --benchmark_filter="BM_DataLoading_MutexCache*" \
    --data_directory=kv-server-gorekore-data-bucket \
    --filename="benchmarking-data" \
    --create_input_file \
    --num_records=4000000 \
    --record_size=512 \
    --args_benchmark_iterations=1 \
    --args_client_max_range_mb=8 \
    --args_client_max_connections=64,128 \
    --args_reader_worker_threads=16,32,64
```

Results:

| data_loading_num_threads | s3client_max_connections | data loading time | num records per sec |
| ------------------------ | ------------------------ | ----------------- | ------------------- |
| 16                       | 64                       | 7.92 sec          | 505 k/s             |
| 32                       | 64                       | 7.82 sec          | 511 k/s             |
| 64                       | 64                       | 7.95 sec          | 503 k/s             |
| 16                       | 128                      | 7.46 sec          | 536 k/s             |
| 32                       | 128                      | 7.70 sec          | 519 k/s             |
| 64                       | 128                      | 8.71 sec          | 459 k/s             |

One intepretation of the results above is that, it takes the KV server 7.92 sec to load a delta file
with 4'000'000 records (each 0.5kb in size) at a rate of 505k records per second when the parallel
reader is using 16 threads and the S3 client is configured with 64 concurrent connections.
