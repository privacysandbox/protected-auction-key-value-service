# Latency Benchmarking Tool

## Requirements

-   Install ghz

The benchmarking tool is a wrapper around [ghz](https://ghz.sh/docs/intro).

Follow instructions to [install ghz](https://ghz.sh/docs/install) and make sure it is installed
correctly:

```sh
ghz -v
```

-   Follow deployment guides

Please read through the deployment guide for the relevant cloud provider
([AWS](/docs/deployment/deploying_on_aws.md)).

At the minimum, you will need to go through the steps up until the `terraform init` command.
Ideally, follow the entire guide and make sure the deployment setup works.

## Running benchmarks

### Option 1: Using `deploy_and_benchmark` script

This script will deploy a terraform configuration and run benchmarks using ghz. Optionally, it can
also deploy a terraform configuration multiple times while overriding sets of variables and run
benchmarks against each deployment.

Note: Please make sure enclave memory is large enough for your data set.

#### Example

Start from the workspace root.

1. Generate SNAPSHOT/DELTA files to upload to data storage For AWS:

    ```sh
    ./tools/serving_data_generator/generate_test_riegeli_data
    GENERATED_DELTA=/path/to/delta/dir/GENERATED_DELTA_FILE
    aws s3 cp $GENERATED_DELTA s3://bucket_name
    ```

1. Provide a directory of SNAPSHOT (or DELTA) files with keys that should be sent in the request to
   the server. This SNAPSHOT (or DELTA) file should only have `UPDATE` mutations. The tool will
   iterate through each SNAPSHOT file, select keys from that file, and run benchmarks with those
   keys.

    For this example, we'll use the generated DELTA files from step 1

    ```sh
    SNAPSHOT_DIR=/path/to/delta/dir/
    ```

1. Set up your terraform config files and save them

    ```sh
    TF_VAR_FILE=/path/to/my.tfvars.json
    TF_BACKEND_CONFIG=/path/to/my.backend.conf
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

    - For an example, see `latency_benchmarking/example/tf_overrides.txt`.

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
    ./tools/latency_benchmarking/deploy_and_benchmark \
    --snapshot-dir ${SNAPSHOT_DIR} \
    --tf-var-file ${TF_VAR_FILE} \
    --tf-backend-config ${TF_BACKEND_CONFIG} \
    --tf-overrides ${TF_OVERRIDES} \
    --csv-output ${PWD}/my_summary.csv
    ```

The result will be in `my_summary.csv`

### Option 2: Using `run_benchmarks` script (manual deployment)

This script assumes that a server has already been deployed. It will run benchmarks using ghz
against a given server address.

Note: Please make sure enclave memory is large enough for your data set.

#### Example

Start from the workspace root.

```sh
SNAPSHOT_DIR=/path/to/snapshot/dir
NUMBER_OF_LOOKUP_KEYS_LIST="1 10 100"
SERVER_ADDRESS="myexample.kv-server.com:8443"
./tools/latency_benchmarking/run_benchmarks \
--server-address ${SERVER_ADDRESS} \
--snapshot-dir ${SNAPSHOT_DIR} \
--number-of-lookup-keys-list "${NUMBER_OF_LOOKUP_KEYS_LIST}"
```

Outputs a summary to `dist/tools/latency_benchmarking/output/<timestamp>/summary.csv`.
