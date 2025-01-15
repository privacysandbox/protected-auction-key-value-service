> FLEDGE has been renamed to Protected Audience API. To learn more about the name change, see the
> [blog post](https://privacysandbox.com/intl/en_us/news/protected-audience-api-our-new-name-for-fledge)

# Generating UDF code configs for the server

The server starts with a simple pass-through implementation at `public/udf/constants.h`.

UDF configurations can be updated as the server is running using delta/snapshot files as per the
[data loading guide](generating_udf_files.md).

-   To override an existing UDF, the delta/snapshot file must have a
    [`DataRecord`](/public/data_loading/data_loading.fbs) with a `UserDefinedFunctionsConfig`.

-   Similar to a `KeyValueMutationRecord`, the `UserDefinedFunctionsConfig` has a
    `logical_commit_time`. The UDF will only be updated for configs with a higher
    `logical_commit_time` than the existing one. The minimum `logical_commit_time` is 1.

Please read through the
[UDF explainer](https://github.com/privacysandbox/fledge-docs/blob/main/key_value_service_user_defined_functions.md#keyvalue-service-user-defined-functions-udfs)
for more requirements and APIs.

# Steps for including the UDF delta file

## 1. Write your UDF

### Option A. Write a custom UDF

Write the UDF according to the
[API in the UDF explainer](https://github.com/privacysandbox/fledge-docs/blob/main/key_value_service_user_defined_functions.md#apis).

Note that the UDF should be in JavaScript (and optionally JavaScript + inline WASM).

### Option B. Use the reference UDF

We provide a [simple reference implementation](/tools/udf/sample_udf/udf.js):

-   The implementation ignores part of the request, e.g. the `metadata` field.
-   For each `keyGroup` in the request, it calls `getValues(keyGroup)` to retrieve the keys from the
    internal cache and returns the key-value pairs in its response.

## 2. Generate a UDF delta file

### Option 1. Using provided UDF tools

Tools to generate UDF delta files and test them are in the `tools/udf` directory.

1. Build the tools binaries docker image:

    ```sh
    -$ builders/tools/bazel-debian run //production/packaging/tools:copy_to_dist --config=local_instance --config=local_platform
    ```

2. Load the tools binaries docker image:

    ```sh
    -$ docker load -i dist/tools_binaries_docker_image.tar
    ```

3. Generate a UDF delta file using the `udf_delta_file_generator` executable.

    Flags:

    - `--udf_handler_name` &mdash; UDF handler name/entry point
    - `--output_dir` &mdash; output directory for the generated delta file
    - `--udf_file_path` &mdash; path to the UDF JavaScript file
    - `--logical_commit_time` &mdash; logical commit time of the UDF config
    - `--code_snippet_version` &mdash; UDF version. For telemetry, should be > 1.

    Example:

    ```sh
    -$ export DATA_DIR=<data_dir>
    docker run -it --rm \
      --volume=$DATA_DIR:$DATA_DIR \
      --user $(id -u ${USER}):$(id -g ${USER}) \
      --entrypoint=/tools/udf/udf_delta_file_generator \
      bazel/production/packaging/tools:tools_binaries_docker_image \
      --output_dir="$DATA_DIR" \
      --udf_file_path="$DATA_DIR/udf.js"
    ```

### Option 2. Generating your own delta file

You can use other options to generate delta files, e.g. using the
[`data_cli` tool](/docs/data_loading/loading_data.md).

The delta file must have a `DataRecord` with a `UserDefinedFunctionsConfig` as its record.

### Option 3. Using sample UDF configurations

A sample UDF JavaScript file is located under the `tools/udf/sample_udf` directory.

## 3. Test the UDF delta file

Test the generated UDF delta file using the [UDF Delta file tester](/tools/udf/udf_tester).

## 4. Provide a UDF to the server

Generally, the delta/snapshot file just needs to be included in delta storage/bucket. Follow the
different deployment guides on how to configure your delta file storage.

The UDF will be executed for the V2 API.
