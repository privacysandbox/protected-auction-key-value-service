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

The default branch contains the latest stable release. To get the latest code, switch to the `main`
branch:

```sh
git checkout main
```

## Build the local binary

From the Key Value server repo folder, execute the following command:

```sh
./builders/tools/bazel-debian build //components/data_server/server:server \
  --config=local_instance \
  --config=local_platform \
  --config=nonprod_mode
```

This will take a while for the first time. Subsequent builds can reuse cached progress.

This command starts a build environment docker container and performs build from within.

-   The `--config=local_instance` means the server itself runs as a local binary instead of running
    on a specific cloud.
-   The `--config=local_platform` means the server will integrate with local version of auxiliary
    systems such as blob storage, parameter, etc. Other possible values are cloud-specific, in which
    case the server will use the corresponding cloud APIs to interact.

The output of this step should be a server binary. Run:

```sh
mkdir -p dist/deltas
cp bazel-bin/components/data_server/server/server dist/server
```

## Run the server

```sh
docker-compose -f getting_started/quick_start_assets/docker-compose.yaml build kvserver
docker-compose -f getting_started/quick_start_assets/docker-compose.yaml run --rm kvserver
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

On production, Envoy (unless the cloud load balancer has native support) is used to convert HTTP
requests to gRPC requests.

First build the gRPC RPC descriptor.

```sh
./builders/tools/bazel-debian build //public/query:query_api_descriptor_set
cp bazel-bin/public/query/query_api_descriptor_set.pb dist/query_api_descriptor_set.pb
chmod 744 dist/query_api_descriptor_set.pb
```

```sh
chmod 444 components/envoy_proxy/envoy.yaml
docker-compose -f getting_started/quick_start_assets/docker-compose.yaml up
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

## Load something

So far the server can only process a specific request by looking up an internal test key. A query
`curl http://localhost:51052/v1/getvalues?keys=example_key` would return empty.

It can use 2 types of things for meaningful processing: data and logic. For privacy reasons, the
server loads all necessary data in the form of files into its RAM and serves all requests with the
in-RAM dataset. It loads the existing data at startup and updates its in-RAM dataset as new files
appear.

The files are called "Delta files", which are similar to database journal files. Each delta file has
many rows of records and each record can be an update or delete.

The delta file type is [Riegeli](https://github.com/google/riegeli) and the record format is
[Flatbuffers](https://flatbuffers.dev/).

Now let's add some data. There is some example data in
[/getting_started/examples/canonical_examples/example_data.csv](/getting_started/examples/canonical_examples/example_data.csv).
The [build definition](/getting_started/examples/canonical_examples/BUILD.bazel) has predefined the
command to use `data_cli` to generate the data.

```sh
./builders/tools/bazel-debian build //getting_started/examples/canonical_examples:generate_data_delta
cp bazel-bin/getting_started/examples/canonical_examples/DELTA_0000000000000001 dist/deltas/DELTA_0000000000000001
```

The `data_cli` command can also be run outside the predefined build target for your own CSV:

```sh
./builders/tools/bazel-debian build //tools/data_cli
bazel-bin/tools/data_cli/data_cli format_data --input_file /path/to/your/file.csv --input_format CSV --output_file dist/deltas/DELTA_0000000000000001 --output_format DELTA
```

The kv server log shows:

```txt
data_orchestrator.cc:188] Loading /tmp/deltas/DELTA_0000000000000001
```

And query:

```sh
curl http://localhost:51052/v1/getvalues?keys=example_key
```

```json
{
    "keys": {
        "example_key": "AAAAAAAAAAAAAAAAAA"
    }
}
```

See [here](/docs/data_loading/loading_data.md) for more information about data loading.

## Use `User Defined Functions (UDF)` to process requests

The above examples use direct lookup from its in-RAM dataset. The server also supports using
[User Defined Functions](https://github.com/privacysandbox/fledge-docs/blob/main/key_value_service_user_defined_functions.md)
to perform custom logic to process requests.

UDFs are also ingested through Delta files.

Let's start with the [example UDF](/getting_started/examples/canonical_examples/example_udf.js):

```js
function HandleRequest(executionMetadata, ...udf_arguments) {
    return 'echo ' + JSON.stringify(udf_arguments) + ' ' + getValues(['example_key']);
}
```

This is the generic signature of the UDF. Different KV server applications may have their own API
defined. But the foundation of the KV server is not limited to a particular application.

Let's convert the JS code to a Delta file. The
[build definition](/getting_started/examples/canonical_examples/BUILD.bazel) has predefined the
command to use `udf_delta_file_generator` to generate the udf delta file.

```sh
./builders/tools/bazel-debian build //getting_started/examples/canonical_examples:udf_delta
cp bazel-bin/getting_started/examples/canonical_examples/DELTA_0000000000000002 dist/deltas/DELTA_0000000000000002
```

(Similar to the data file, the UDF file can also be generated with building the tool specified in
the build target and running it with your own command line flags. See
[details](/docs/generating_udf_files.md).)

And query:

```sh
curl -X PUT http://localhost:51052/v2/getvalues -d '{"partitions":[{"arguments":[{"data":"hi"}]}]}'
```

Output:

```json
{
    "singlePartition": {
        "stringOutput": "\"echo [\\\"hi\\\"] {\\\"kvPairs\\\":{\\\"example_key\\\":{\\\"value\\\":\\\"AAAAAAAAAAAAAAAAAA\\\"}},\\\"status\\\":{\\\"code\\\":0,\\\"message\\\":\\\"ok\\\"}}\""
    }
}
```

Note that this uses a v2 getvalues endpoint. This endpoint uses a generic API definition which maps
to the generic UDF signature and is not limited to a particular application such as Protected
Audience. The v1 API used previously on the other hand is specific to Protected Audience. The
example UDF used here does not conform to the Protected Audience API so the v1/getvalues curl call
which is specific to Protected Audience would not succeed.

## Protected Audience UDF

Now let's add another UDF to make it work for the
[Protected Audience KV server v2 API](https://github.com/WICG/turtledove/blob/main/FLEDGE_Key_Value_Server_API.md).

Add a new file with the following:

```sh
cat << 'EOF' > getting_started/examples/canonical_examples/example_udf2.js
function getKeyGroupOutputs(hostname, udf_arguments) {
  let keyGroupOutputs = [];
  for (let argument of udf_arguments) {
    let keyGroupOutput = {};
    let data = argument.data;
    keyGroupOutput.tags = argument.tags;
    const getValuesResult = JSON.parse(getValues(data));
    // getValuesResult returns "kvPairs" when successful and "code" on failure.
    // Ignore failures and only add successful getValuesResult lookups to output.
    if (getValuesResult.hasOwnProperty("kvPairs")) {
      const kvPairs = getValuesResult.kvPairs;
      const keyValuesOutput = {};
      for (const key in kvPairs) {
        if (kvPairs[key].hasOwnProperty("value")) {
          keyValuesOutput[key] = { "value": kvPairs[key].value + hostname };
        }
      }
      keyGroupOutput.keyValues = keyValuesOutput;
      keyGroupOutputs.push(keyGroupOutput);
    }
  }
  return keyGroupOutputs;
}

function HandleRequest(executionMetadata, ...udf_arguments) {
  logMessage(JSON.stringify(executionMetadata));
  const keyGroupOutputs = getKeyGroupOutputs(executionMetadata.requestMetadata.hostname, udf_arguments);
  return {keyGroupOutputs, udfOutputApiVersion: 1};
}
EOF
```

Also make a new build rule so we can set this UDF with a higher timestamp to overwrite the existing
UDF in the server.

```sh
cat << 'EOF' >> getting_started/examples/canonical_examples/BUILD.bazel
run_binary(
    name = "udf2_delta",
    srcs = [
        ":example_udf2.js",
    ],
    outs = [
        "DELTA_0000000000000003",
    ],
    args = [
        "--udf_file_path",
        "$(location :example_udf2.js)",
        "--output_path",
        "$(location DELTA_0000000000000003)",
        "--logical_commit_time",
        "1800000000",
    ],
    tool = "//tools/udf/udf_generator:udf_delta_file_generator",
)
EOF
```

Note the logical_commit_time is higher than the first UDF. Build the delta file:

```sh
./builders/tools/bazel-debian build //getting_started/examples/canonical_examples:udf2_delta
cp bazel-bin/getting_started/examples/canonical_examples/DELTA_0000000000000003 dist/deltas/DELTA_0000000000000003
```

Query with Protected Audience Chrome V1 API:

```sh
curl 'http://localhost:51052/v1/getvalues?keys=example_key&hostname=example.com'
```

## Further steps

At this point we have looked at all the basic components. See the following specialty guides for
advanced topics and features:

-   [Writing WebAssembly User defined functions:](/docs/inline_wasm_udfs.md)
-   [Deploying on AWS](/docs/deployment/deploying_on_aws.md)
-   [Deploying on GCP](/docs/deployment/deploying_on_gcp.md)
-   [Sharding](https://github.com/privacysandbox/protected-auction-services-docs/blob/main/key_value_service_sharding.md)
-   [Working with Terraform](/docs/deployment/working_with_terraform.md)
-   [UDF binary data API](/docs/udf_read_apis_with_binary_data.md)
