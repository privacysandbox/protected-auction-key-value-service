# UDF tester

This binary directly invokes UDF which can access data input.

It requires two delta files:

-   Delta file with key-value pairs to be stored in memory
    ([docs](/docs/data_loading/loading_data.md))
-   Delta file with the UDF configuration ([docs](/docs/generating_udf_files.md)).

## Flags:

-   `--kv_delta_file_path`: Path to delta file with KV pairs
-   `--udf_delta_file_path`: Path to delta file with UDF configuration
-   `--input_arguments`: List of input arguments in JSON format. Each input argument should be
    equivalent to a [UDFArgument](/public/api_schema.proto).

> Note: The UDF testing tool only checks if the execution is successful and the output is a valid
> JSON. It does not perform any schema validation.

## Steps to use examples in this directory:

(From the repo root)

1. Create UDF delta file.
    - Example dummy udf file is
      [/getting_started/examples/canonical_examples/example_udf.js](/getting_started/examples/canonical_examples/example_udf.js).
    - Run
      `./builders/tools/bazel-debian build //getting_started/examples/canonical_examples:udf_delta`.
      The output would be
      `bazel-bin/getting_started/examples/canonical_examples/DELTA_0000000000000002`.
1. Create Data delta file.

-   Example data csv file is
    [/getting_started/examples/canonical_examples/example_data.csv](/getting_started/examples/canonical_examples/example_data.csv).
    -   Run
        `./builders/tools/bazel-debian build //getting_started/examples/canonical_examples:generate_data_delta`.
        The output would be
        `bazel-bin/getting_started/examples/canonical_examples/DELTA_0000000000000001`.

1. Build the tester.
   `./builders/tools/bazel-debian build //getting_started/examples/canonical_examples:udf_delta_file_tester`
1. Run the tester.

    ```sh
    bazel-bin/tools/udf/udf_tester/udf_delta_file_tester --kv_delta_file_path bazel-bin/getting_started/examples/canonical_examples/DELTA_0000000000000001 --udf_delta_file_path bazel-bin/getting_started/examples/canonical_examples/DELTA_0000000000000002 --input_arguments='[{"data":["a"]}]'
    ```

## Examples

-   Single input argument w/ list of strings as data:

    ```sh
    bazel-bin/tools/udf/udf_tester/udf_delta_file_tester --kv_delta_file_path path/to/kv/file --udf_delta_file_path path/to/delta/file --input_arguments='[{"data":["foo0", "foo1"]}]'
    ```

-   Single input argument w/ string as data:

    ```sh
    bazel-bin/tools/udf/udf_tester/udf_delta_file_tester --kv_delta_file_path path/to/kv/file --udf_delta_file_path path/to/delta/file --input_arguments='[{"data":"foo0"}]'
    ```

-   Single input argument w/ tags and list of strings as data:

    ```sh
    bazel-bin/tools/udf/udf_tester/udf_delta_file_tester --kv_delta_file_path path/to/kv/file --udf_delta_file_path path/to/delta/file --input_arguments='[{"tags":["tag1"], "data":["foo0"]}]'
    ```

-   Multiple input arguments:

```sh
bazel-bin/tools/udf/udf_tester/udf_delta_file_tester --kv_delta_file_path path/to/kv/file --udf_delta_file_path path/to/delta/file --input_arguments='[{"data":["foo1"]}, {"tags":["tag1"], "data":["foo0"]}]'
```
