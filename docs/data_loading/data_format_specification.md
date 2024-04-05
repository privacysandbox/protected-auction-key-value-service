# Event/Record

The KV server data is represented by events or records. These are used interchangeably in the
codebase. In the context of data loading, they are also called mutations.

A data mutation record consists of a key, a value and some metadata. When loaded into the server,
they can be queried by the query API.

The record format is [Flatbuffers](https://flatbuffers.dev/). The schema is defined
[here](/public/data_loading/data_loading.fbs).

-   Each mutation event is associated with a `logical_commit_timestamp`, larger timestamp indicates
    a more recent record.
-   There are two types of mutation events:
    1. UPDATE which inserts/modifies a key/value record
    2. DELETE which deletes an existing key/value record.

# Data files

Files are used as containers of records. The server reads the files to load the records.

There are two types data files consumed by the server:

1. delta files: incremental updates
1. snapshot files: full rewrite

In both cases, newer key/value pairs supersede existing key/value pairs.

## Delta files

Delta filename must conform to the regular expression `DELTA_\d{16}`. See
[constants.h](/public/constants.h) for the most up-to-date format. More recent delta files are
lexicographically greater than older delta files. Delta files have the following properties:

-   Consists of key/value mutation events (updates/deletes) for a fixed time window.
-   `logical_commit_timestamp` of the records have no relation with their file's name. It is
    acceptable to also use timestamps in file names for ordering purposes for your convenience but
    the system makes no assumption on the relation between the record timestamps and the file names.
-   There are no enforced size limits for delta files, but smaller files are faster to read.
-   Server instances continually watch for newer delta files and update their in-memory caches.

## Snapshot files

Snapshot filename must conform to the regular expression `SNAPSHOT_\d{16}`. See
[constants.h](/public/constants.h). for the most up-to-date format. More recent snapshot files are
lexicographically greater than older snapshot files. Snpashot files have the following properties:

-   Uses the same file format as delta files and are only read at server startup time.
-   Generated from:
    -   compacting a set of delta files by merging multiple mutation events for the same key such
        that the resulting snapshot consists of only UPDATE mutation events.
    -   compacting a base snapshot file together with a set of delta files that are not in the base
        snapshot file.
-   Contains the entire set of key/value records since the beginning of time.
-   There are no enforced size limits for snapshot files.

See [File groups](file_groups.md#file-groups) on how to improve snapshot generation scalability and
throughput.

## File format

### Avro

The system supports [Avro](https://avro.apache.org/) as the file format. It should be an Avro file
containing one or more Flatbuffers records.

The Avro schema is the primitive string schema.

Each row is a serialized Flatbuffers record.

-   [C++ example](/public/data_loading/readers/avro_stream_io_test.cc)
-   [Java example](https://github.com/privacysandbox/protected-auction-key-value-service/issues/39):
    This is not maintained by the dev team and may be out of date.

#### Riegeli

The system also supports [Riegeli](https://github.com/google/riegeli). Similarly each file contains
one or more serialized Flatbuffers records.
