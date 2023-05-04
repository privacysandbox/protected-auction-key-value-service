> FLEDGE has been renamed to Protected Audience API. To learn more about the name change, see the
> [blog post](https://privacysandbox.com/intl/en_us/news/protected-audience-api-our-new-name-for-fledge)

# Generating UDF code configs for the server

During server startup, the server reads in the UDF configuration through existing delta/snapshot
files in the bucket. The UDF delta file must be in the delta storage before the server starts. If
not, it will default to a simple pass-through implementation at `public/udf/constants.h`.

Currently, the server does not support code updates.

Depending on the environment, you will need to either include the delta file with the code configs
in the terraform or the local directory.

Please read through the
[UDF explainer](https://github.com/privacysandbox/fledge-docs/blob/main/key_value_user_defined_functions.md#keyvalue-service-user-defined-functions-udfs)
for requirements and APIs.

# Steps for including the UDF delta file

## 1. Write your UDF

### Option A. Write a custom UDF

Write the UDF according to the
[API in the UDF explainer](https://github.com/privacysandbox/fledge-docs/blob/main/key_value_user_defined_functions.md#apis).

Note that the UDF should be in JavaScript (and optionally JavaScript + inline WASM).

### Option B. Use the reference UDF

We provide a [simple reference implementation](tools/udf/udf.js):

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

    - `--output_dir` &mdash; output directory for the generated delta file
    - `--udf_file_path` &mdash; path to the UDF JavaScript file

    Example:

    ```sh
    -$ dist/debian/udf_delta_file_generator --output_dir="$PWD" --udf_file_path="path/to/my/udf/udf.js"
    ```

### Option 2. Generating your own delta file

You can use other options to generate delta files, e.g. using the
[`data_cli` tool](./loading_data.md).

The delta file must have the following key value records with an `UPDATE` mutation type:

| Key              | Value                                                                 |
| ---------------- | --------------------------------------------------------------------- |
| udf_handler_name | Name of the handler function that serves as the execution entry point |
| udf_code_snippet | UDF code snippet that contains the handler                            |

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
    Defaults to `keys`. Options: `keys`, `renderUrls`, `adComponentRenderUrls`.

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
