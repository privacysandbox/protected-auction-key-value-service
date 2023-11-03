# Quick start

This gets you started with a local Key Value server with a simple working example.

## Setup

In this tutorial, we use linux as the example environment. Other platforms may have potential
unsupported issues.

Before starting the build process, install [Docker](https://docs.docker.com/engine/install/) and
[BuildKit](https://docs.docker.com/build/buildkit/). If you run into any Docker access errors,
follow the instructions for
[setting up sudoless Docker](https://docs.docker.com/engine/install/linux-postinstall/#manage-docker-as-a-non-root-user).

## Clone the github repo

Using Git, clone the repository into a folder:

```sh
git clone https://github.com/privacysandbox/fledge-key-value-service.git
```

## Build the local binary

From the Key Value server repo folder, execute the following command:

```sh
./builders/tools/bazel-debian build //components/data_server/server:server \
  --//:platform=local \
  --//:instance=local
```

This will take a while for the first time. Subsequent builds can reuse cached progress.

This command starts a build environment docker container and performs build from within.

-   The `--//:instance=local` means the server itself runs as a local binary instead of running on a
    specific cloud.
-   The `--//:platform=local` means the server will integrate with local version of auxiliary
    systems such as blob storage, parameter, etc. Other possible values are cloud-specific, in which
    case the server will use the corresponding cloud APIs to interact.

The output of this step should be a server binary. Run:

```sh
mkdir dist
cp bazel-bin/components/data_server/server/server dist/server
```

## Run the server

```sh
docker compose -f getting_started/quick_start_assets/docker-compose.yaml build kvserver
docker compose -f getting_started/quick_start_assets/docker-compose.yaml run --rm -p 50051:50051 kvserver
```

In a separate terminal, at the repo root, run

```sh
tools/grpc_cli call localhost:50051 kv_server.v1.KeyValueService.GetValues \
  'kv_internal: "hi"' \
  --channel_creds_type=insecure
```

You should expect a response of:

```json
connecting to localhost:50051
kv_internal {
  fields {
    key: "hi"
    value {
      string_value: "Hello, world! If you are seeing this, it means you can query me successfully"
    }
  }
}
Rpc succeeded with OK status
```

Now you can stop the server container (with `docker ps` and `docker kill`).

## Process HTTP requests with Envoy

On production, Envoy (unless the cloud load balancer has native support) is used to convert HTTP requests to gRPC requests.

First build the gRPC RPC descriptor.

```sh
./builders/tools/bazel-debian build //public/query:query_api_descriptor_set
cp bazel-bin/public/query/query_api_descriptor_set.pb dist/query_api_descriptor_set.pb
chmod 744 dist/query_api_descriptor_set.pb
```

```sh
docker compose -f getting_started/quick_start_assets/docker-compose.yaml up
```

In a separate terminal, run:

```sh
curl http://localhost:51052/v1/getvalues?kv_internal=hi
```

And you can see:

```json
{
 "kvInternal": {
  "hi": "Hello, world! If you are seeing this, it means you can query me successfully"
 }
}
```