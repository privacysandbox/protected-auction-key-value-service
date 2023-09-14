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

-   The implementation ignores part of the request, e.g. the `context` field.
-   For each `keyGroup` in the request, it calls `getValues(keyGroup.keyList)` to retrieve the keys
    from the internal cache and returns the key-value pairs in its response.

## 2. Generate a UDF delta file

### Option 1. Using provided UDF tools

Tools to generate UDF delta files and test them are in the `tools/udf` directory.

1. Build the executables:

    ```sh
    -$ builders/tools/bazel-debian run //production/packaging/tools:copy_to_dist_udf
    ```

2. Generate a UDF delta file using the `dist/debian/udf_delta_file_generator` executable.

    Flags:

    - `--udf_handler_name` &mdash; UDF handler name/entry point
    - `--output_dir` &mdash; output directory for the generated delta file
    - `--udf_file_path` &mdash; path to the UDF JavaScript file
    - `--logical_commit_time` &mdash; logical commit time of the UDF config
    - `--code_snippet_version` &mdash; UDF version. For telemetry, should be > 1.

    Example:

    ```sh
    -$ dist/debian/udf_delta_file_generator --output_dir="$PWD" --udf_file_path="path/to/my/udf/udf.js"
    ```

### Option 2. Generating your own delta file

You can use other options to generate delta files, e.g. using the
[`data_cli` tool](./loading_data.md).

The delta file must have a `DataRecord` with a `UserDefinedFunctionsConfig` as its record.

### Option 3. Using sample UDF configurations

A sample UDF JavaScript file and corresponding Delta file are located under the
`tools/udf/sample_udf` directory.

## 3. Test the UDF delta file

Test the generated UDF delta file using the `dist/debian/udf_delta_file_tester` executable.

It requires two delta files:

-   Delta file with key-value pairs to be stored in memory
-   Delta file with the UDF configuration (from Step 2).

Optionally, you can pass a key, subkey, and namespace that are included in the request to the UDF.

Flags:

-   `--kv_delta_file_path`: Path to delta file with KV pairs
-   `--udf_delta_file_path`: Path to delta file with UDF configuration
-   `--key`: Key to query. Defaults to an empty string.
-   `--subkey`: Subkey of the
    [`context` field](https://github.com/WICG/turtledove/blob/main/FLEDGE_Key_Value_Server_API.md#schema-of-the-request).
    Defaults to an empty string.
-   `--namespace_tag`:
    [Namespace tag for key](https://github.com/WICG/turtledove/blob/main/FLEDGE_Key_Value_Server_API.md#available-tags).
    Defaults to `keys`.

Example:

```sh
-$ dist/debian/udf_delta_file_tester --kv_delta_file_path="path/to/DELTA_WITH_KEYS" --udf_delta_file_path="path/to/DELTA_WITH_UDF" --key="my_test_key"
```

> Note: The UDF testing tool only checks if the execution is successful and the output is a valid
> JSON. It does not perform any schema validation.

## 4. Provide a UDF to the server

Generally, the delta/snapshot file just needs to be included in delta storage/bucket. Follow the
different deployment guides on how to configure your delta file storage.

The UDF will be executed for the V2 API.
