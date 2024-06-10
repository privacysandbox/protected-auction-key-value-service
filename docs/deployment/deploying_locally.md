> FLEDGE has been renamed to Protected Audience API. To learn more about the name change, see the
> [blog post](https://privacysandbox.com/intl/en_us/news/protected-audience-api-our-new-name-for-fledge)

# FLEDGE Key/Value server deployment locally

This article is for adtech engineers who want to test the Key/Value server locally. Deploying
production servers in this way is not recommended, please see the
[AWS deployment guide](deploying_on_aws.md) or [GCP deployment guide](deploying_on_gcp.md) instead.

To learn more about FLEDGE and the Key/Value server, take a look at the following documents:

-   [FLEDGE Key/Value server explainer](https://github.com/WICG/turtledove/blob/main/FLEDGE_Key_Value_Server_API.md)
-   [FLEDGE Key/Value server trust model](https://github.com/privacysandbox/fledge-docs/blob/main/key_value_service_trust_model.md)
-   [FLEDGE explainer](https://developers.google.com/privacy-sandbox/relevance/protected-audience)
-   [FLEDGE API developer guide](https://developer.chrome.com/blog/fledge-api/)

    > The instructions written in this document are for running a test Key/Value server that does
    > not yet have full privacy protection. The goal is for interested users to gain familiarity
    > with the functionality and high level user experience. As more privacy protection mechanisms
    > are added to the system, this document will be updated accordingly.

# Build the Key/Value server artifacts

Before starting the build process, install [Docker](https://docs.docker.com/engine/install/) and
[BuildKit](https://docs.docker.com/build/buildkit/). If you run into any Docker access errors,
follow the instructions for
[setting up sudoless Docker](https://docs.docker.com/engine/install/linux-postinstall/#manage-docker-as-a-non-root-user).

## Get the source code from GitHub

The code for the FLEDGE Key/Value server is released on
[GitHub](https://github.com/privacysandbox/fledge-key-value-service).

Using Git, clone the repository into a folder:

```sh
git clone https://github.com/privacysandbox/fledge-key-value-service.git
```

## Build the local binary

From the Key/Value server repo folder, execute the following command:

```sh
./builders/tools/bazel-debian build //components/data_server/server:server \
  --config=local_instance \
  --config=local_platform \
  --config=nonprod_mode
```

## Generate UDF delta file

We provide a default UDF implementation that is loaded into the server at startup.

To use your own UDF, refer to the [UDF Delta file documentation](/docs/generating_udf_files.md) to
generate a UDF delta file.

Include the delta file in your local delta directory (see below).

# Deployment

## Create local directories

Create the directories that the local server will read data from.

```sh
mkdir /tmp/deltas
mkdir /tmp/realtime
```

The files that you want to server to serve should go into these directories. The server will load
their contents on startup and continue to watch them while it is running.

## Start the server

```sh
  ./bazel-bin/components/data_server/server/server \
  --delta_directory=/tmp/deltas \
  --realtime_directory=/tmp/realtime \
  --logging_verbosity_level=4 --stderrthreshold=0
```

The server will start up and begin listening for new delta and realtime files in the directories
provided.

# Build and run Key/Value server locally in docker

You can also build and run KV server locally in docker.

## Build the docker image

```sh
builders/tools/bazel-debian run //production/packaging/local/data_server:copy_to_dist \
 --config=local_instance --config=local_platform --config=nonprod_mode
```

## Load the image

```sh
docker load -i dist/server_docker_image.tar
```

## Run the server in docker

```sh
docker run -it --network=host -entrypoint=/server --init --rm \
--volume=/tmp/deltas:/tmp/deltas --volume=/tmp/realtime:/tmp/realtime \
--security-opt=seccomp=unconfined bazel/production/packaging/local/data_server:server_docker_image \
--port 50051 -stderrthreshold=0 -delta_directory=/tmp/deltas -realtime_directory=/tmp/realtime
```

# Common operations

## Query the server

The server can be queried by calling it with the
[GRPC CLI](https://github.com/grpc/grpc/blob/master/doc/command_line_tool.md).

**The server cannot be queried using HTTP or HTTPS because it relies on an Envoy proxy to bridge
from those protocols to GRPC and Envoy was not started.**

```sh
grpc_cli call localhost:50051 kv_server.v1.KeyValueService.GetValues \
  'kv_internal: "hi"' \
  --channel_creds_type=insecure
```

## See all flag options

```sh
./bazel-bin/components/data_server/server/server --help
```
