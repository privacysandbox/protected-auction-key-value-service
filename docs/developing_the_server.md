# FLEDGE K/V Server developer guide

## Repository set up

### Initialize and update submodules

Before using the repository, initialize the repo's submodules. Run:

```shell
git submodule update --init
```

The submodule content can be updated at any time using the following command.

```shell
git submodule update --remote --merge
```

## Data Server

The data server provides the read API for the KV service.

### Prereqs

-   [Docker](https://docs.docker.com/get-docker/) on linux
-   Supported CPU architectures: AMD64, ARM64

### Run the server locally inside a docker container

1.  Build the image

    1. (Optional) If you need to make changes to code of a dependency repo:

        1. Note the path of the local repo
        1. Comment out the http_archive target in [WORKSPACE](/WORKSPACE) file (or a file loaded by
           it)
        1. Add a target in the WORKSPACE file:

            - [local_repository](https://bazel.build/reference/be/workspace#local_repository) if the
              directory tree has WORKSPACE and BUILD files
            - [new_local_repository](https://bazel.build/reference/be/workspace#new_local_repository)
              otherwise.

            Example (note, it must be added to the WORKSPACE file itself):

            ```bazel
            local_repository(
                name = "bazel_skylib",
                path = "/tmp/local_dependency/bazel_skylib",
            )
            ```

        1. Set environment variable to map the path for builds:

            ```sh
            export EXTRA_DOCKER_RUN_ARGS='--volume /tmp/local_dependency:/tmp/local_dependency'
            ```

    1. Build the server artifacts and copy them into the `dist/debian/` directory.

    ```sh
    builders/tools/bazel-debian run //production/packaging/aws/data_server:copy_to_dist --//:instance=local --//:platform=aws
    ```

1.  Load the image into docker

    ```sh
    docker load -i dist/debian/server_docker_image.tar
    ```

1.  Run the container. Port 50051 can be used to query the server directly through gRPC. Port 51052
    can be used to query with HTTP which is served through Envoy to the server. --environment must
    be specified. The server will still read data from S3 and the server uses environment to find
    the S3 bucket. The environment is configured as part of the
    [AWS deployment process](/docs/deploying_on_aws.md).

    Set region. The region should be where your environment is deployed:

    ```sh
    export AWS_DEFAULT_REGION=us-east-1
    ```

    ```sh
    docker run -it --rm --entrypoint=/server/bin/init_server_basic --env AWS_DEFAULT_REGION --env AWS_ACCESS_KEY_ID --env AWS_SECRET_ACCESS_KEY -p 127.0.0.1:50051:50051 -p 127.0.0.1:51052:51052 bazel/production/packaging/aws/data_server:server_docker_image --port 50051 --environment=your_aws_environment
    ```

### Run the server locally

For example:

```sh
builders/tools/bazel-debian run //components/data_server/server:server --//:instance=local --//:platform=aws -- --environment="dev"
```

> Attention: The server can run locally while specifying `aws` as platform, in which case it will
> contact AWS based on the local AWS credentials. However, this requires the AWS environment to be
> set up first following the [AWS deployment guide](/docs/deploying_on_aws.md). You might need to
> set up the following parameters in the AWS System Manager:
>
> | Parameter Name                 | Value                                                          |
> | ------------------------------ | -------------------------------------------------------------- |
> | kv-server-local-data-bucket-id | Name of the delta file S3 bucket                               |
> | kv-server-local-bucket-sns-arn | ARN of the Simple Notification Service (SNS) for the S3 bucket |
> | kv-server-local-launch-hook    | Any value, this won't be needed for                            |
> | kv-server-local-mode           | "DSP" or "SSP"                                                 |

We are currently developing this server for local testing and for use on AWS Nitro instances
(similar to the
[Aggregation Service](https://github.com/google/trusted-execution-aggregation-service). We
anticipate supporting additional cloud providers in the future.

### Interact with the server

-   Use `grpc_cli` to interact with your local instance. You might have to pass
    `--channel_creds_type=insecure`.

Example:

```sh
grpc_cli call localhost:50051 kv_server.v1.KeyValueService.GetValues "kv_internal: 'hi'" --channel_creds_type=insecure
```

-   For HTTP queries:

```sh
curl http://localhost:51052/v1/getvalues?kv_internal=hi
```

## Develop and run the server inside AWS enclave

The KV service instance should be set up by following the deployment guide
([AWS](/docs/deploying_on_aws.md)). For faster iteration, enclave image of the server is also
produced under `dist/`. Once the system has been started, iterating on changes to the server itself
only requires restarting the enclave image:

1. Copy the new enclave EIF to an AWS EC2 instance that supports nitro enclave. Note: The system has
   a SSH instance that a developer can access. From there the user can access actual server EC2
   instances, using the same SSH key. So the copy command below should be repeated twice to reach
   the destination EC2 instance.

    ```sh
    scp -i ~/"key.pem" dist/server_enclave_image.eif ec2-user@${EC2_ADDR}.compute-1.amazonaws.com:/home/ec2-user/server_enclave_image.eif
    ```

1. Start the enclave job (If one is running, terminate it first, see below for instructions):

    ```sh
    nitro-cli run-enclave --cpu-count 2 --memory 30720 --eif-path server_enclave_image.eif --debug-mode --enclave-cid 16
    ```

1. To see logs of the TEE job:

    ```sh
    ENCLAVE_ID=$(nitro-cli describe-enclaves | jq -r ".[0].EnclaveID"); [ "$ENCLAVE_ID" != "null" ] && nitro-cli console --enclave-id ${ENCLAVE_ID}
    ```

1. To terminate the job:

    ```sh
    ENCLAVE_ID=$(nitro-cli describe-enclaves | jq -r ".[0].EnclaveID"); [ "$ENCLAVE_ID" != "null" ] && nitro-cli terminate-enclave --enclave-id ${ENCLAVE_ID}
    ```

### Specifying platform specific src/dep

It's possible to use polymorphism + build-time flag to only build and link code specific to a
platform.

Example:

```build
cc_library(
    name = "blob_storage_client",
    srcs = select({
        "//:aws_platform": ["s3_blob_storage_client.cc"],
    }),
    hdrs = [
        "blob_storage_client.h",
    ],
    deps = select({
        "//:aws_platform": ["@aws_sdk_cpp//:s3"],
    }) + [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)
```

Available conditions are:

-   //:aws_platform
-   //:local_platform

Depending on which platform the server is being run on, you will want to specify the platform.

-   //:aws_instance
-   //:local_instance

There are two options for OpenTelemetry export when `//:local_instance` is specified:

-   //components/telemetry:local_otel_export=ostream [default]
-   //components/telemetry:local_otel_export=jaeger

When jaeger is specified, run a local instance of [Jaeger](https://www.jaegertracing.io/) to capture
telemetry.
