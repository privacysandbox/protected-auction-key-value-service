# The word2vec sample

This sample demonstrates how key to set data and key to value data can be loaded into a server and
queried. In this case we load the server with categorized groups of words, as well as embedding
vectors for a set of words.

The sample will demonstrate how you can query for a set of words, and sort them based on scoring
criteria defined by word similarities.

## Generating DELTA files

There are 2 categories of DELTA files we build, data and udf.

### Create the DELTA files for the data

File generation is composed of 2 steps:

-   Generate the CSV data
-   Convert the CSV data to DELTA files

BUILD rules take care of generating the csv and piping them to the `data_cli`` for you. The
following commands will build DELTA files for both embeddings and category DELTA files.

```sh
builders/tools/bazel-debian build tools/udf/sample_word2vec:generate_categories_delta
builders/tools/bazel-debian build tools/udf/sample_word2vec:generate_embeddings_delta
```

### Create the DELTA file for the UDF

Build the udf:

```sh
builders/tools/bazel-debian build tools/udf/sample_word2vec:udf_delta
```

At this point there are 3 DELTA files:

-   DELTA_0000000000000001 - containing the categories
-   DELTA_0000000000000002 - containing the embeddings
-   DELTA_0000000000000003 - containing the UDF

## [Local test] Start a local server and load the data

Set up the data:

```sh
mkdir /tmp/deltas
./builders/tools/normalize-bazel-symlinks
cp $(builders/tools/bazel-debian aquery 'tools/udf/sample_word2vec:udf_delta' |
   sed -n 's/Outputs: \[\(.*\)\]/\1/p' |
   xargs dirname)/DELTA* /tmp/deltas
```

Build the local server:

```sh
./builders/tools/bazel-debian build //components/data_server/server:server   --//:platform=local   --//:instance=local
```

Run the local server:

```sh
./builders/tools/normalize-bazel-symlinks
GLOG_alsologtostderr=1 \
  ./bazel-bin/components/data_server/server/server \
  --delta_directory=/tmp/deltas \
  --realtime_directory=/tmp/realtime
```

## Send a query

`body.txt` contains a json representation of a v2 request. Feel free to modify it. There are 2 sets
of information:

-   metadata - keys into the set data which do a set UNION of all entries
-   signals - only one value used. orders unioned data by similarity to signal word.

The UDF returns the top 5 results and their scores.

```sh
grpc_cli call  localhost:50051 kv_server.v2.KeyValueService/GetValues  \
  "raw_body: {data: $(tr -d '\n' < tools/udf/sample_word2vec/body.txt)}" \
   --channel_creds_type=insecure
```
