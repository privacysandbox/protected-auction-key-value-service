# Protected App Signals, Advanced Ad Retrieval developer guide

This is an advanced part of the
[Ad Retreival dev guide](/docs/protected_app_signals/onboarding_dev_guide.md)

It illustrates the points made in the
[use case overview](/docs/protected_app_signals/ad_retrieval_overview.md#use-case-overview).
![alt_text](../assets/ad_retrieval_filter_funnel.png 'image_tooltip')

This sample demonstrates how key->set(set queries) and key->value(value lookups) data can be loaded
into a server and used together. In this case the key->set data is categorized groups of words. The
key->value data is a word to embedding mapping. An embedding is a vector of numbers.

The sample will demonstrate how you can query for a set of words, and sort them based on scoring
criteria defined by word similarities, using embeddings.

## Word2vec

We are using [word2vec](https://en.wikipedia.org/wiki/Word2vec) technique here. It is an NLP
technique for obtaining vector representations of words. These vectors capture information about the
meaning of the word based on the surrounding words.

## Generating DELTA files

There are 2 categories of DELTA files we build, data and udf.

### Create the DELTA files for the data

File generation is composed of 2 steps:

-   Generate the CSV data
-   Convert the CSV data to DELTA files

BUILD rules take care of generating the csv and piping them to the `data_cli` for you. The following
commands will build DELTA files for both embeddings and category DELTA files.

```sh
builders/tools/bazel-debian build //docs/protected_app_signals/examples/advanced:generate_categories_delta
builders/tools/bazel-debian build //docs/protected_app_signals/examples/advanced:generate_embeddings_delta
```

generated csv data for categories looks like (key="catalyst"):

```txt
catalyst,UPDATE,1691033853939065,role|part|precursor|impetus|catalyst,string_set
```

generated csv data for embeddings looks like (key="catalyst"):

```txt
catalyst,UPDATE,1691033853941974,"[0.0272216796875, 0.11083984375, 0.12890625, -0.11669921875,...]",string
```

In this example you can see that the embedding is stored as a JSON string.

### Create the DELTA file for the UDF

Build the udf:

```sh
builders/tools/bazel-debian build //docs/protected_app_signals/examples/advanced:udf_delta
```

At this point there are 3 DELTA files:

-   DELTA_0000000000000001 - Category sets
-   DELTA_0000000000000002 - Embedding values
-   DELTA_0000000000000003 - UDF code and metadata

## [Local test] Start a local server and load the data

Set up the data:

```sh
mkdir /tmp/deltas
cp $(builders/tools/bazel-debian aquery '//docs/protected_app_signals/examples/advanced:udf_delta' |
   sed -n 's/Outputs: \[\(.*\)\]/\1/p' |
   xargs dirname)/DELTA* /tmp/deltas
```

Build the local server:

```sh
./builders/tools/bazel-debian build //components/data_server/server:server --config=local_instance --config=local_platform --config=nonprod_mode
```

Run the local server:

```sh
  ./bazel-bin/components/data_server/server/server \
  --delta_directory=/tmp/deltas \
  --realtime_directory=/tmp/realtime --stderrthreshold=0
```

## Send a query

`body.txt` contains a json representation of a v2 request. Feel free to modify it. There are 2 sets
of information:

-   metadata - Keys into the set data which do a UNION of all entries
-   signals - Only one value used. Orders unioned data by similarity to signal word.

The UDF returns the top 5 results and their scores.

```sh
grpc_cli call  localhost:50051 kv_server.v2.KeyValueService/GetValuesHttp  \
  "raw_body: {data: $(tr -d '\n' < docs/protected_app_signals/examples/advanced/body.txt)}" \
   --channel_creds_type=insecure
```
