# Protected App Signals, Ad Retrieval developer guide

This Ad Retrieval guide is a natural extension of this PAS
[guide](https://developer.android.com/design-for-safety/privacy-sandbox/guides/protected-audience/protected-app-signals#ad-retrieval)
which assumes a BYOS set up, but it could use a TEE Ad Retrieval server instead.

This guide provides a sample retrieval usecase. To support this usecase, details on how to set up a
test environment in the same VPC as the B&A server and reuse the same mesh, upload a sample UDF
function and upload a sample Delta file are given.

For an overview of the Protected App Signals API, read the design
[proposal.](https://developer.android.com/design-for-safety/privacy-sandbox/protected-app-signals)

For an overview of the Key Value/ Ad Retrieval Protected App Signals, read the the following
[doc.](/docs/protected_app_signals/ad_retrieval_overview.md)

## Deploying an Ad Retrieval server

[GCP.](/docs/deployment/deploying_on_gcp.md) Please follow the `B&A integration within the same VPC`
section there.

AWS support is coming later.

## Example Setup

This script is an extension of the example scenario listed
[here.](https://developer.android.com/design-for-safety/privacy-sandbox/guides/protected-audience/protected-app-signals#example-setup)

Consider the following scenario: using the Protected App Signals API, an ad tech stores relevant
signals based on user app usage. In our example, signals are stored that represent in-app purchases
from several apps. During an auction, the encrypted signals are collected and passed into a
Protected Auction running in B&A. The buyer's UDFs running in B&A use the signals to fetch ad
candidates and compute a bid.

The fetching happens by calling the Ad Retrieval server.

### Loading data

For privacy reasons, the server loads all necessary data in the form of files into its RAM and
serves all requests with the in-RAM dataset. It loads the existing data at startup and updates its
in-RAM dataset as new files appear.

The files are called "Delta files", which are similar to database journal files. Each delta file has
many rows of records and each record can be an update or delete.

The delta file type is [Riegeli](https://github.com/google/riegeli) and the record format is
[Flatbuffers](https://flatbuffers.dev/).

Now let's add some data. There is some example data in
[examples/ad_retrieval.csv](./examples/ad_retrieval.csv). The
[build definition](./examples/BUILD.bazel) has predefined the command to use `data_cli` to generate
the data.

```sh
./builders/tools/bazel-debian build //docs/protected_app_signals/examples:generate_delta
```

##### GCP

Upload the delta file to the bucket

```sh
export GCS_BUCKET=your-gcs-bucket-id
gsutil cp bazel-bin/docs/protected_app_signals/examples/DELTA_0000000000000002 gs://${GCS_BUCKET}
```

More [details](../data_loading/loading_data.md#upload-data-files-to-gcp)

### UDF

This script looks up and returns values for the keys specified in `protectedSignals`.

```javascript
function HandleRequest(
    requestMetadata,
    protectedSignals,
    deviceMetadata,
    contextualSignals,
    contextualAdIds
) {
    let protectedSignalsKeys = [];
    const parsedProtectedSignals = JSON.parse(protectedSignals);
    for (const [key, value] of Object.entries(parsedProtectedSignals)) {
        protectedSignalsKeys.push(key);
    }
    return getValues(protectedSignalsKeys);
}
```

Note that this is a simplistic example created for an illustrative purpose. The retreieval case can
get more complicated.

See this [example](../../getting_started/examples/sample_word2vec/). The sample demonstrates how you
can query for a set of words, taking advantage of the native set query support, and sort them based
on scoring criteria defined by word similarities, using embeddings.

#### Loading the UDF

UDFs are also ingested through Delta files. Build the delta file:

```sh
./builders/tools/bazel-debian build //docs/protected_app_signals/examples:ad_retreival_udf
```

##### GCP

Upload the delta file to the bucket

```sh
export GCS_BUCKET=your-gcs-bucket-id
gsutil cp bazel-bin/docs/protected_app_signals/examples/DELTA_0000000000000001 gs://${GCS_BUCKET}
```

More [details](../data_loading/loading_data.md#upload-data-files-to-gcp)

### Retrieval Path

Summary: Retrieve ads data.

#### Input

```proto
partitions {
  arguments {
    data {
      string_value: "{\"ad_id1\":1}"
    }
  }
  arguments {
    data {
      struct_value {
        fields {
          key: "X-Accept-Language"
          value {
            string_value: "en-US,en;q=0.9"
          }
        }
        fields {
          key: "X-BnA-Client-IP"
          value {
            string_value: "104.133.126.32"
          }
        }
        fields {
          key: "X-User-Agent"
          value {
            string_value: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36"
          }
        }
      }
    }
  }
  arguments {
    data {
      string_value: "{\"h3\": \"1\"}"
    }
  }
  arguments {
  }
}
```

#### Output

```proto
single_partition {
  string_output: "[{\"ad_id1\":\"ad_id1_value\"}]"
}
```
