# The word2vec sample

This sample demonstrates how key to set data and key to value data can be loaded into a server and
queried. In this case we load the server with categorized groups of words, as well as embedding
vectors for a set of words.

The sample will demonstrate how you can query for a set of words, and sort them based on scoring
criteria defined by word similarities.

## Generating DELTA files

File generation is composed of 2 steps:

-   Generate the CSV data
-   Convert the CSV data to DELTA files

### Generate CSV data

`data_generator.py` uses a pretrained word2vec dictionary of word to embedding.

It is able to create CSV files which can then be consumed by the `data_cli`` tool. These CSVs can be
transformed into DELTA files which can in turn be loaded into the kv server.

`categories.csv` contains word->set of related word mappings `embeddings.csv` contains
word->embedding mappings

### Building DELTA files with bazel

BUILD rules take care of generating the csv and piping them to the data_cli for you. The following
commands will build DELTA files for both embeddings and category DELTA files.

```sh
builders/tools/bazel-debian build tools/udf/sample_word2vec:generate_categories_delta
builders/tools/bazel-debian build tools/udf/sample_word2vec:generate_embeddings_delta
```
