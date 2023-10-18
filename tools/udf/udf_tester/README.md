# UDF tester

This binary directly invokes UDF which can access data input.

Steps:

(From the repo root)

1. Create UDF delta file.
    - Example udf file is in this directory named `example_udf.js`.
    - Run `./builders/tools/bazel-debian build //tools/udf/udf_tester:udf_delta`. The output would
      be `bazel-bin/tools/udf/udf_tester/DELTA_0000000000000001`.
1. Create Data delta file.
    - Example data csv file is in this directory named `example_data.csv`.
    - Run `./builders/tools/bazel-debian build //tools/udf/udf_tester:generate_data_delta`. The
      output would be `bazel-bin/tools/udf/udf_tester/DELTA_0000000000000002`.
1. Build the tester.
   `./builders/tools/bazel-debian build //tools/udf/udf_tester:udf_delta_file_tester`
1. Run the tester.

    ```sh
    bazel-bin/tools/udf/udf_tester/udf_delta_file_tester --kv_delta_file_path bazel-bin/tools/udf/udf_tester/DELTA_0000000000000002 --udf_delta_file_path bazel-bin/tools/udf/udf_tester/DELTA_0000000000000001 --input_arguments="\"a\""
    ```
