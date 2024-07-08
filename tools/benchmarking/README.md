# Request Benchmarking Tool

## Requirements

-   Follow deployment guides

To use the tool you will need to have a complete deployment setup.

Please read through the deployment guide for the relevant cloud provider
([AWS](/docs/deployment/deploying_on_aws.md) or [GCP](/docs/deployment/deploying_on_gcp.md)).

At the minimum, you will need to go through the steps up until the `terraform init` command.
Ideally, follow the entire guide and make sure your deployment setup works.

## Tools

Both the `run_benchmarks` and `deploy_and_benchmark` scripts are meant as a convenience to run
benchmarks against the server with different parameters/configurations. Instead of running multiple
iterations of benchmarks manually, the scripts take certain input parameters, run benchmarks, and
summarize the result.

> Note: The script does not inspect the server's response. Should the server time out or not
> respond, the script currently does not attempt to rerun the benchmarks, it will simply continue.
> To find out which benchmarks need to be rerun, inspect the summary csvs.

### run_benchmarks

This script assumes that a server has already been deployed.

It creates one or multiple requests, runs ghz for each request, and outputs the summary in a CSV
file.

The request body is designed to work with the server's [default UDF](/public/udf/constants.h). To
ensure the script works, any custom UDF will need to be able to handle the same inputs (outputs are
not affected).

```js
{"metadata": {...}, "partitions": [{"id": 0, "compressionGroupId": 0, "arguments": [{"tags": ["custom", "keys"], "data": <keys>}]}]}
```

The tool takes a directory of snapshot files or delta files with key value mutations of type
`Update`.

For each snapshot file in `snapshot-dir`, the script reads its keys and may generate multiple
requests, one for each element in `number-of-lookup-keys-list`. The number of lookup keys element
indicates how many keys from the snapshot file should be included in a request.

For example, given a `snapshot-dir` with 2 snapshot files and a `number-of-lookup-keys-list` with 3
elements, the script will generate 6 requests.

Each benchmark iteration will about 40 seconds to execute. In total, the script would take about 4-5
minutes to complete the benchmark + 1-2 minutes for pre- and post processing.

#### Reference

Usage:

```sh
./tools/benchmarking/run_benchmarks <flags>
```

Flags:

-   `--server-address`

    Required. gRPC host and port.

    Example: `--server-address my-server:8443`

-   `--snapshot-dir`

    Required if `--snapshot-csv-dir` is not provided. Full path to a directory of snapshot files.
    These snapshot files are converted to CSVs using the
    [`data cli`](/docs/data_loading/loading_data.md). The keys from the snapshot file are used to
    create requests.

-   `--snapshot-csv-dir`

    Required if `--snapshot-dir` is not provided. Full path to a directory of only snapshot CSV
    files (i.e. converted using the [`data cli`](/docs/data_loading/loading_data.md)). This avoids
    doing the conversion in the tool, which saves some time for multiple runs on the same snapshot
    files.

-   `--number-of-lookup-keys-list` (Optional)

    A list of number of keys (in quotes, as a string) to include in a request. The tool will iterate
    through snapshot csvs and the `--number-of-lookup-keys-list` to construct a request for each
    combination. Default is `"1 5 10"`.

    Example: `--number-of-lookup-keys-list "1 10"`

-   `--benchmark-duration` (Optional)

    How long each benchmark iteration should run for. Default is `"5s"`.

-   `--ghz-tags` (Optional)

    Tags to include for all ghz runs. The csv summary will include this tag for all iterations.

    Example: `-- ghz-tags '{"my_tag":"my_value"}'`

-   `--request-metadata-json` (Optional)

    The request metadata json object to use for all requests. Default is empty `{}`.

    Example: `--request-metadata-json '{"metadata_key":"metadata_value"}'`

-   `--filter-snapshot-by-sets` (Optional)

    Whether to filter snapshot csvs using `value_type=string_set` only. This will create requests
    that only include keys of sets. Default is false.

#### Example

Start from the workspace root.

```sh
SNAPSHOT_DIR=/path/to/snapshot/dir
NUMBER_OF_LOOKUP_KEYS_LIST="1 10 100"
SERVER_ADDRESS="demo.kv-server.your-domain.example:8443"
./tools/benchmarking/run_benchmarks \
--server-address ${SERVER_ADDRESS} \
--snapshot-dir ${SNAPSHOT_DIR} \
--number-of-lookup-keys-list "${NUMBER_OF_LOOKUP_KEYS_LIST}"
```

The summary can be found in `dist/tools/benchmarking/output/<timestamp_seconds>/summary.csv`.

### deploy_and_benchmark

This script will deploy a terraform configuration and call the `run_benchmarks` script.

The tool will _not_ upload key value delta/snapshot files. Please ensure those are already loaded
into the blob storage bucket that the server will be reading from.

It includes almost all of the `run_benchmarks` flags in addition to more deployment parameters to
iterate through.

-   If provided with a terraform overrides file with sets of terraform variable overrides, the
    script will deploy once per set of variables and run benchmarks.
-   If given a directory with UDF deltas, it will upload each UDF to the data bucket, and run
    benchmarks against it.
-   If given a json lines file with request metadata, it will iterate through each request metadata
    and run benchmarks against it.

> The number of iterations can increase dramatically with each added parameter. For example, given 3
> sets of terraform overrides, 3 UDF deltas, and 3 request metadata jsons, `run_benchmarks` will
> execute 27 times. If in addition to that, the `number-of-lookup-keys` has 3 elements and
> `snapshot-dir` has 3 files, the script will be running 243 iterations of ghz, once for each
> combination of parameters.

#### Reference

Usage:

```sh
./tools/benchmarking/deploy_and_benchmark <flags>
```

Flags:

-   `--cloud-provider`

    Required. Cloud provider. Options: "aws" or "gcp"

-   `--tf-var-file`

    Required. Full path to `tfvars.json` file.

-   `--tf-backend-config`

    Required. Full path to `backend.conf` file.

-   `--server-url`

    Required. URL to deployed server.

    Example: "<https://demo.kv-server.your-domain.example/>"

-   `--snapshot-dir`

    Required. Full path to a directory of snapshot files. The keys from the snapshot file are used
    to create requests.

-   `--csv-output` (Optional)

    Full path to csv output. Contains a summary of all ghz runs.

-   `--tf-overrides` (Optional)

    File with terraform variable overrides. Each line in the file counts as a set of variable
    overrides. If provided, the tool will deploy once per line. The expected format for each line is
    a comma-separated list of terraform variable overrides, e.g.
    `instance_type=c5.4xlarge,enclave_cpu_count=12`

    Example: `/tools/benchmarking/example/aws_tf_overrides.txt`

-   `--udf-delta-dir` (Optional)

    Full path to directory of udf delta files to use for benchmarking. The tool will upload each
    file in the directory to the given `data-bucket` and run benchmarks after.

    Note that all udf deltas are uploaded once per deployment and removed by the end of each
    deployment. Since the server does not restart in between udf uploads, it will only pick up udf
    deltas if the logical_commit_times and versions are set correctly.

    See the [udf delta documentation](/docs/generating_udf_files.md) for more info.

-   `--data-bucket` (Optional)

    The data bucket to upload the udf files too. This should be the same data bucket that the server
    is reading delta files from.

-   `--request-metadata-json-file` (Optional)

    Path to JSON lines file to iterate through to generate requests.

    Example: `/tools/benchmarking/example/request_metadata.jsonl`

-   `--number-of-lookup-keys-list` (Optional)

    A list of number of keys (in quotes, as a string) to include in a request. The tool will iterate
    through snapshot csvs and the `--number-of-lookup-keys-list` to construct a request for each
    combination. Default is `"1 10 50 100"`.

    Example: `--number-of-lookup-keys-list "1 10"`

-   `--minimum-server-wait-secs` (Optional)

    The amount of time that the tool should wait after the `terraform apply` command before checking
    whether the server is healthy.

    If this is too low, the tool may still be pinging the previous deployment's server, since it may
    take a while to tear down the instance.

    Default is `90`.

-   `--extra-server-wait-timeout` (Optional)

    The amount of time that the tool should continue checking server health on top of the
    `minimum-server-wait-secs`.

    The tool will continue pinging the server until the timeout is reached or the server is healthy.

    If the delta files

    Default is `5m`.

    Examples: `5m`, `600s`

-   `--benchmark-duration` (Optional)

    How long each benchmark iteration should run for. Default is `"5s"`.

-   `--ghz-tags` (Optional)

    Tags to include for all ghz runs. The csv summary will include this tag for all iterations.

    Example: `-- ghz-tags '{"my_tag":"my_value"}'`

-   `--cleanup-deployment` (Optional)

    Whether to call terraform destroy once the tool exits. Default false.

-   `--filter-snapshot-by-sets` (Optional)

    Whether to filter snapshot csvs using `value_type=string_set` only. This will create requests
    that only include keys of sets. Default is false.

#### Example

Start from the workspace root.

1. Generate SNAPSHOT/DELTA files to upload to data storage:

    ```sh
    ./tools/serving_data_generator/generate_test_riegeli_data
    GENERATED_DELTA=/path/to/delta/dir/GENERATED_DELTA_FILE
    ```

    For AWS:

    ```sh
    aws s3 cp $GENERATED_DELTA s3://bucket_name
    ```

    For GCP:

    ```sh
    gcloud storage cp $GENERATED_DELTA gs://bucket_name
    ```

1. Set your cloud provider

    AWS:

    ```sh
    CLOUD_PROVIDER="aws"
    ```

    GCP:

    ```sh
    CLOUD_PROVIDER="gcp"
    ```

1. Provide a directory of SNAPSHOT (or DELTA) files with keys that should be sent in the request to
   the server. This SNAPSHOT (or DELTA) file should only have `UPDATE` mutations. The tool will
   iterate through each SNAPSHOT file, select keys from that file, and run benchmarks with those
   keys.

    For this example, we'll use the generated DELTA files from step 1

    ```sh
    SNAPSHOT_DIR=/path/to/delta/dir/
    ```

1. Set up your terraform config files

    ```sh
    TF_VAR_FILE=/path/to/my.tfvars.json
    TF_BACKEND_CONFIG=/path/to/my.backend.conf
    ```

1. Set the server url

    ```sh
    SERVER_URL="https://demo.kv-server.your-domain.example/"
    ```

1. (optional) Provide a file with sets of terraform variables to be overriden. For each set of
   terraform variables, the script will `terraform apply` once with the given variables and run
   benchmarks against the deployed server. The variable override file should have the following
   format:

    - Each line should be in the form

    ```txt
    variable_name1=variable_valueA,variable_name2=variable_valueB
    ```

    - Each line is considered a set of variables to be overriden in one `terraform apply` command

    - For an example, see `/benchmarking/example/aws_tf_overrides.txt`.

        ```sh
        TF_OVERRIDES=/path/to/tf_variable_overrides.txt
        ```

1. (optional) Write UDFs If given a directory of UDF delta files, the tool iterates through each
   one, uploads it to the given data bucket, runs benchmarks, then removes the UDF delta from the
   data bucket.

    ```sh
    UDF_DELTA_DIR=/path/to/udf_deltas/
    DATA_BUCKET=s3://bucket_name
    ```

1. Run the script and wait for the result

    ```sh
    ./tools/benchmarking/deploy_and_benchmark \
    --cloud-provider ${CLOUD_PROVIDER} \
    --server-url ${SERVER_URL} \
    --snapshot-dir ${SNAPSHOT_DIR} \
    --tf-var-file ${TF_VAR_FILE} \
    --tf-backend-config ${TF_BACKEND_CONFIG} \
    --tf-overrides ${TF_OVERRIDES} \
    --csv-output ${PWD}/my_summary.csv
    ```

    The result will be in `my_summary.csv`.

    ```sh
    ls my_summary.csv
    ```

## Appendix

### Things of note when deploying

-   Make sure the DELTA/SNAPSHOT files follow the expected logic for time stamps and naming,
    otherwise the data may not load properly. See the
    [data format specification](/docs/data_loading/data_format_specification.md) and
    [udf delta doc](/docs/generating_udf_files.md) for more info.

-   Make sure the chosen instance sizes have enough memory to hold your data.

-   On AWS, some larger instances have multiple NUMA clusters. However, an enclave can only run on
    one NUMA cluster. You may want to consider [sharding](/docs/sharding/sharding.md).

-   Depending on the number of iterations the script runs, it may take a long time to finish. Each
    benchmark (ghz) iteration takes 40-60s + a few minutes of overhead per deployment.
