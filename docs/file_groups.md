# File groups

## What are file groups?

A file group is a group of split files treated logically as one file for data loading purposes by
the KV server. Each split can be uploaded to the blob storage bucket as soon as it's completely
generated which improves scalability and throughput for data generation pipelines.

> Currently, file groups are only supported for [SNAPSHOT](loading_data.md#snapshot-files) files.

## Naming scheme

FILE GROUP NAME FORMAT:
`<FILE_TYPE>`_`<LOGICAL_TIMESTAMP>`_`<PART_FILE_INDEX>`_OF_`<NUM_PART_FILES>`

VARIABLE DESCRIPTION:

-   `FILE_TYPE`: type of the file. Currently, this can only be SNAPSHOT.
-   `LOGICAL_TIMESTAMP`: is a 16 digit monotonic clock that only moves forward. a larger value means
    a more recent file.
-   `PART_FILE_INDEX`: a 5 digit number that represents the index of the file in the file group.
    each index can only be used with a single part file name, loading part files sharing an index is
    not guaranteed to be correct. valid range is `[0..NUM_PART_FILES-1]`.
-   `NUM_PART_FILES`: a 6 digit number that represents the total number of part files in a file
    group. valid range is `[1..100,000]`.

VALID EXAMPLES:

-   `SNAPSHOT_1705430864435450_00000_OF_000010` is the first part file in a snapshot file group with
    10 part files.
-   `SNAPSHOT_1705430864435450_00009_OF_000010` is the last part file in a snapshot file group with
    10 part files.

There is a util function to generate split file names conforming to this naming scheme from
components [ToFileGroupFileName(...)](../public/data_loading/filename_utils.h#L67)

## Server startup and snapshot file groups

During server startup, the server looks for the most recent, complete snapshot file group and loads
that first. Then, it continues with loading most recent delta files not included in the snapshot
file group. A file group is considered complete is all of it's part files are in the storage at load
time. For example, the following set of part files a complete snapshot file group of size 2:

`complete_group = ["SNAPSHOT_1705430864435450_00000_OF_000002", "SNAPSHOT_1705430864435450_00001_OF_000002"]`
