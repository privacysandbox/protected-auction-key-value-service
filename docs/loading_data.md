# Load data into the FLEDGE Key/Value server

The FLEDGE Key/Value server is used to send real-time signals to the buyers and the sellers during a
FLEDGE auction. The server reads files from a cloud file storage service. This doc explains the
expected file format, and processes to perform the common data loading operations. Please note the
following:

-   We provide a
    [C++ library reference implementation](#using-the-c-reference-library-to-read-and-write-data-files)
    and a [CLI tool](#using-the-cli-tool-to-generate-delta-and-snapshot-files) that can be used to
    generate (or write) and read data files.
-   The reference library and CLI tool are ready to use as-is, or you can write your own libraries.
-   The data generation part is a general process that applies to all cloud providers, but the
    uploading instructions are for AWS only.

# Data files

There are two types data files consumed by the server, (1) delta files and (2) snapshot files. In
both cases, newer key/value pairs supersede existing key/value pairs.

## Delta files

Delta filename must conform to the regular expression `DELTA_\d{16}`. See
[constants.h](../public/constants.h) for the most up-to-date format. More recent delta files are
lexicographically greater than older delta files. Delta files have the following properties:

-   Consists of key/value mutation events (updates/deletes) for a fixed time window.
-   Each mutation event is associated with a `logical_commit_timestamp`, larger timestamp indicates
    a more recent record.
-   Mutation events are ordered by their `logical_commit_timestamp` and this order is important at
    read time to make sure that mutations are applied correctly.
-   `logical_commit_timestamp` of the records have no relation with their file's name. It is
    acceptable to also use timestamps in file names for ordering purposes for your convenience but
    the system makes no assumption on the relation between the record timestamps and the file names.
-   There are two types of mutation events: (1) UPDATE which introduces/modifies a key/value record,
    and (2) DELETE which deletes an existing key/value record.
-   There are no enforced size limits for delta files, but smaller files are faster to read.
-   Server instances continually watch for newer delta files and update their in-memory caches.

## Snapshot files

Snapshot filename must conform to the regular expression `SNAPSHOT_\d{16}`. See
[constants.h](../public/constants.h). for the most up-to-date format. More recent snapshot files are
lexicographically greater than older snapshot files. SNpashot files have the following properties:

-   Uses the same file format as delta files and are only read at server startup time.
-   Generated from:
    -   compacting a set of delta files by merging multiple mutation events for the same key such
        that the resulting snapshot consists of only UPDATE mutation events.
    -   compacting a base snapshot file together with a set of delta files that are not in the base
        snapshot file.
-   Contains the entire set of key/value records since the beginning of time.
-   There are no enforced size limits for snapshot files.

# Experimenting with sample data

## Generate sample data

A tool is available to generate sample data in [Riegeli](https://github.com/google/riegeli) format.
From the repo base directory, run:

```sh
-$ ./tools/serving_data_generator/generate_test_riegeli_data
```

Confirm that the sample data file `DELTA_\d{16}` has been generated.

# Using the CLI tool to generate delta and snapshot files

The data CLI is located under: `//tools/data_cli`. First build the cli using the following command:

```sh
-$ builders/tools/bazel-debian run //production/packaging/tools:copy_to_dist
```

The cli executable will be located under `dist/debian/data_cli`. Run the following command to see
all the data_cli commands, their corresponding flag arguments and some examples:

```sh
-$ alias data_cli=dist/debian/data_cli
-$ data_cli --help
Usage: data_cli <command> <flags>

Commands:
- format_data          Converts input to output format.
    [--input_file]     (Optional) Defaults to stdin. Input file to convert records from.
    [--input_format]   (Optional) Defaults to "CSV". Possible options=(CSV|DELTA)
    [--output_file]    (Optional) Defaults to stdout. Output file to write converted records to.
    [--output_format]  (Optional) Defaults to "DELTA". Possible options=(CSV|DELTA).
  Examples:
...

- generate_snapshot             Compacts a range of delta files into a single snapshot file.
    [--starting_file]           (Required) Oldest delta file or base snapshot to include in compaction.
    [--ending_delta_file]       (Required) Most recent delta file to include compaction.
    [--snapshot_file]           (Optional) Defaults to stdout. Output snapshot file.
    [--data_dir]                (Required) Directory with input delta files. Cloud directories are prefixed with "cloud://".
    [--working_dir]             (Optional) Defaults to "/tmp". Directory used to write temporary data.
    [--in_memory_compaction]    (Optional) Defaults to true. If false, file backed compaction is used.
  Examples:
...
-$
```

Follow the provided examples to generate your own data. For example, to convert a CSV file to a
DELTA file, run the following command:

```sh
data_cli format_data \
   --input_file="$HOME/data/data.csv" --input_format=CSV \
   --output_file="$HOME/data/data.delta" --output_format=DELTA
```

And to generate a snapshot from a set of delta files, run the following command (replacing flag
values with your own values):

```sh
-$ export DATA_DIR=<data_dir>; \
   data_cli generate_snapshot \
      --data_dir="$DATA_DIR" \
      --working_dir="/tmp" \
      --starting_file="DELTA_0000000000000001" \
      --ending_delta_file="DELTA_0000000000000010" \
      --snapshot_file="SNAPSHOT_0000000000000001"
```

The output snapshot file will be written to `$DATA_DIR`.

# Using the C++ reference library to read and write data files

Data files are written using the [Riegeli](https://github.com/google/riegeli) format and data
records are stored as [Flatbuffers](https://google.github.io/flatbuffers/). The record schema is
here: [Flatbuffer record schema](public/data_loading/data_loading.fbs).

The C++ reference library implementation can be found under:
[C++ data file readers](../public/data_loading/readers) and
[C++ data file writers](../public/data_loading/writers). To write snapshot files, you can use
[Snapshot writer](../public/data_loading/writers/snapshot_stream_writer.h) and to write delta files,
you can use [Delta writer](../public/data_loading/writers/delta_record_stream_writer.h). Both files
can be read using the
[data file reader](../public/data_loading/readers/delta_record_stream_reader.h). The source and
destination of the provided readers and writers are required to be `std::iostream` objects.

# Writing your own C++ data libraries

Feel free to use the C++ reference library provided above as examples if you want to write your own
data library. Keep the following things in mind:

-   Make sure the output files adhere to the [delta](#delta-files) and [snapshot](#snapshot-files)
    file properties listed above.
-   Snapshot files must be written with metadata specifying the starting and ending filenames of
    records included in the snapshot. See
    [SnpashotMetadata proto](../public/data_loading/riegeli_metadata.proto).

# Upload data files to AWS

The server watches an S3 bucket for new files. The bucket name is provided by you in the Terraform
config and is globally unique.

You can use the AWS CLI to upload the sample data to S3, or you can also use the UI.

```sh
-$ S3_BUCKET="[[YOUR_BUCKET]]"
-$ aws s3 cp riegeli_data s3://${S3_BUCKET}/DELTA_001
```

> Cauition: The filename must start with `DELTA_` prefix, followed by a 16-digit number.

Confirm that the file is present in the S3 bucket:

![the delta file listed in the S3 console](assets/s3_delta_file.png)

## Integrating file uploading with your data source for AWS

AWS provides libraries to communicate with S3, such as the
[C++ SDK](https://aws.amazon.com/sdk-for-cpp/). As soon as a file is uploaded to a watched bucket it
will be read into the service, assuming that it has a higher logical commit timestamp.
