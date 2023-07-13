> FLEDGE has been renamed to Protected Audience API. To learn more about the name change, see the
> [blog post](https://privacysandbox.com/intl/en_us/news/protected-audience-api-our-new-name-for-fledge)

# Load data into the FLEDGE Key/Value server

The FLEDGE Key/Value server is used to send real-time signals to the buyers and the sellers during a
FLEDGE auction.

There are two ways to populate data in the server. The standard path is by uploading files to a
cloud file storage service. The standard upload is the authoritative, high bandwidth and persistent
source of truth.

The other way is via a low latency path. To apply such an update, you should send an update to a
dedicated broadcast topic.

This doc explains the expected file format, and processes to perform the common data loading
operations. Please note the following:

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

-   Consists of key/value mutation events (updates/deletes) for a fixed time window. The events are
    in the format of Flatbuffers ([Schema](/public/data_loading/data_loading.fbs)).
-   Each mutation event is associated with a `logical_commit_timestamp`, larger timestamp indicates
    a more recent record.
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

The data CLI is located under: `//tools/data_cli`. First build the cli using the following command
(Note that to build the cli to use `generate_snapshot` command with data in AWS S3, use
`--//:platform=aws`.):

```sh
-$ builders/tools/bazel-debian run //production/packaging/tools:copy_to_dist --//:instance=local --//:platform=local
```

After building, the cli will be packaged into a docker image tar file under
`dist/tools_binaries_docker_image.tar`. Using the generated docker image file is the recommended way
to run the data cli. Load the docker image into docker as follows:

```sh
-$ docker load -i dist/tools_binaries_docker_image.tar
```

Run the following to see a list of all available commands and their input arguments:

```sh
-$ docker run -it --rm \
    --entrypoint=/tools/data_cli/data_cli \
    bazel/production/packaging/tools:tools_binaries_docker_image \
    --help
```

This will print something like the following output:

```sh
-$ ...
Usage: data_cli <command> <flags>

Commands:
- format_data          Converts input to output format.
    [--input_file]     (Optional) Defaults to stdin. Input file to convert records from.
    [--input_format]   (Optional) Defaults to "CSV". Possible options=(CSV|DELTA)
    [--output_file]    (Optional) Defaults to stdout. Output file to write converted records to.
    [--output_format]  (Optional) Defaults to "DELTA". Possible options=(CSV|DELTA).
    [--record_type]    (Optional) Defaults to "KEY_VALUE_MUTATION_RECORD". Possible
                                  options=(KEY_VALUE_MUTATION_RECORD|USER_DEFINED_FUNCTIONS_CONFIG).
                                  If reading/writing a UDF config, use "USER_DEFINED_FUNCTIONS_CONFIG".

  Examples:
...

- generate_snapshot             Compacts a range of delta files into a single snapshot file.
    [--starting_file]           (Required) Oldest delta file or base snapshot to include in compaction.
    [--ending_delta_file]       (Required) Most recent delta file to include compaction.
    [--snapshot_file]           (Optional) Defaults to stdout. Output snapshot file.
    [--data_dir]                (Required) Directory (or S3 bucket) with input delta files.
    [--working_dir]             (Optional) Defaults to "/tmp". Directory used to write temporary data.
    [--in_memory_compaction]    (Optional) Defaults to true. If false, file backed compaction is used.
  Examples:
...
-$
```

As an example, to convert a CSV file to a DELTA file, run the following command:

```sh
-$ docker run -it --rm \
    --volume=$PWD:$PWD \
    --user $(id -u ${USER}):$(id -g ${USER}) \
    --entrypoint=/tools/data_cli/data_cli \
    bazel/production/packaging/tools:tools_binaries_docker_image \
    format_data \
    --input_file="$PWD/data.csv" \
    --input_format=CSV \
    --output_file="$PWD/DELTA_0000000000000001" \
    --output_format=DELTA
```

Here are samples of a valid csv files that can be used as input to the cli:

```sh
# The following csv example shows csv with simple string value.
key,mutation_type,logical_commit_time,value,value_type
key1,UPDATE,1680815895468055,value1,string
key2,UPDATE,1680815895468056,value2,string
key1,UPDATE,1680815895468057,value11,string
key2,DELETE,1680815895468058,value2,string

# The following csv example shows csv with set values.
# By default, column delimiter = "," and value delimiter = "|"
key,mutation_type,logical_commit_time,value,value_type
key1,UPDATE,1680815895468055,elem1|elem2,string_set
key2,UPDATE,1680815895468056,elem3|elem4,string_set
key1,UPDATE,1680815895468057,elem6|elem7|elem8,string_set
key2,DELETE,1680815895468058,elem10,string_set
```

Note that the csv delimiters for set values can be changed to any character combination, but if the
defaults are not used, then the chosen delimiters should be passed to the data_cli using the
`--csv_column_delimiter` and `--csv_value_delimiter` flags.

And to generate a snapshot from a set of delta files, run the following command (replacing flag
values with your own values):

```sh
-$ export GLOG_logtostderr=1;
export DATA_DIR=<data_dir>;
docker run -it --rm \
    --env GLOG_logtostderr \
    --volume=/tmp:/tmp \
    --volume=$DATA_DIR:$DATA_DIR \
    --user $(id -u ${USER}):$(id -g ${USER}) \
    --entrypoint=/tools/data_cli/data_cli \
    bazel/production/packaging/tools:tools_binaries_docker_image \
    generate_snapshot \
    --data_dir="$DATA_DIR" \
    --working_dir=/tmp \
    --starting_file=DELTA_0000000000000001 \
    --ending_delta_file=DELTA_0000000000000010 \
    --snapshot_file=SNAPSHOT_0000000000000001
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

> Note: Access control of the S3 bucket is managed by your IAM system on the cloud platform. Make
> sure to set the right permissions.

You can use the AWS CLI to upload the sample data to S3, or you can also use the UI.

```sh
-$ S3_BUCKET="[[YOUR_BUCKET]]"
-$ aws s3 cp riegeli_data s3://${S3_BUCKET}/DELTA_001
```

> Caution: The filename must start with `DELTA_` prefix, followed by a 16-digit number.

Confirm that the file is present in the S3 bucket:

![the delta file listed in the S3 console](assets/s3_delta_file.png)

## Integrating file uploading with your data source for AWS

AWS provides libraries to communicate with S3, such as the
[C++ SDK](https://aws.amazon.com/sdk-for-cpp/). As soon as a file is uploaded to a watched bucket it
will be read into the service, assuming that it has a higher logical commit timestamp.

# Realtime updates

The server exposes a way to post low latency updates. To apply such an update, you should send a
delta file to a dedicated broadcast topic.

![Realtime design](assets/realtime_design.png)

In the case of AWS it is a Simple Notification Service (SNS) topic. That topic is created in
terraform
[here](https://github.com/privacysandbox/fledge-key-value-service/blob/7f3710b1f1c944d7879718a334afd5cb8f80f3d9/production/terraform/aws/services/data_storage/main.tf#L107).
Delta files contain multiple rows, which allows you to batch multiple updates together. There is a
[limit](https://docs.aws.amazon.com/AWSSimpleQueueService/latest/SQSDeveloperGuide/quotas-messages.html)
of 256KB for the message size.

Each data server listens to that topic by longpolling a subscribed queue. Once the update is
received, it is immediately applied to the in-memory data storage.

## Data upload sequence

The records you modify through the realtime update channel should still be added to the standard
data loading path. If it is not, then that update can be lost, for example, during a server restart.
The standard upload is the authoritative and persistent source of truth, and the low latency update
allows to speed up the update latency.

![Realtime sequence](assets/realtime_sequence.png)

As per the diagram below, first you should
[write](<(#using-the-c-reference-library-to-read-and-write-data-files)>) the updates to a delta file
that will be uploaded via a standard path later. The purpose of this step is to guarantee that this
record won't be missed later.

Then you should [send](#sample-upload) the high priority updates. Note, that here you _probably_
want to generate new delta files specific to what will be sent to SNS, but you could reuse the ones
you generated above.

Then, once you're ready to upload your delta file via a [standard path](#upload-data-files-to-aws),
you should do it. This step should be performed after you sent the data to the low latency path.
Storing the low latency updates in the standard path persists them. Therefore the standard path
files serve as journals so when the servers restart, they can recover the low latency updates
through the standard path files. We recommend to limit as much as possible the time between the low
latency update time and its journaling time to reduce the inconsistency between servers that receive
data from low latency path and servers that have to reapply the updates through the standard journal
path.

Technically, the first step can be performed after sending updates to the low latency path, as long
as you guarantee that that data won't be lost and is persisted somewhere.

## Sample upload

### AWS CLI

Before running the below command, make sure that you've set the correct AWS variables
(AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY) that allow you to communicate with your AWS resources.

```sh
topic_arn=....your realtime SNS topic ARN...
// sample delta file name
file=$(base64 DELTA_1675868575987012)
aws sns publish --topic-arn "$topic_arn" --message "$file"
```

### AWS cpp

Check out this sample
[tool](https://github.com/privacysandbox/fledge-key-value-service/blob/d65103e3944cfb8de6e70136a2066b38b56ce82c/components/tools/realtime_updates_publisher.cc)
on how to insert the low latency updates.
