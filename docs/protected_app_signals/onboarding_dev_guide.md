# Protected App Signals, Ad Retrieval developer guide

This Ad Retrieval guide is a natural extension of this PAS
[guide](https://developer.android.com/design-for-safety/privacy-sandbox/guides/protected-audience/protected-app-signals#ad-retrieval)
which assumes a BYOS set up, but it could use a TEE Ad Retrieval server instead.

This guide provides sample retrieval and lookup usecases. To support these usecases, details on how
to set up a test environment in the same VPC as the B&A server and reuse the same mesh, upload a
sample UDF function and upload a sample Delta file are given.

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
gsutil cp bazel-bin/docs/protected_app_signals/examples/DELTA_0000000000000001 gs://${GCS_BUCKET}
```

More [details](../data_loading/loading_data.md#upload-data-files-to-gcp)

### Retrieval Path

Note that this is a simplistic example created for an illustrative purpose. The retrieval case can
get more complicated.

[Advanced_onboarding_dev_guide](/docs/protected_app_signals/advanced_onboarding_dev_guide.md)
demonstrates how you can query for a set of words, taking advantage of the native set query support,
and sort them based on scoring criteria defined by word similarities, using embeddings.

### UDF

This [script](examples/ad_retrieval_udf.js) looks up and returns values for the keys specified in
`protectedSignals`.

```javascript
function HandleRequest(requestMetadata, protectedSignals, deviceMetadata, contextualSignals) {
    let protectedSignalsKeys = [];
    const parsedProtectedSignals = JSON.parse(protectedSignals);
    for (const [key, value] of Object.entries(parsedProtectedSignals)) {
        protectedSignalsKeys.push(key);
    }
    const kv_result = JSON.parse(getValues(protectedSignalsKeys));
    if (kv_result.hasOwnProperty('kvPairs')) {
        return kv_result.kvPairs;
    }
    const error_message = 'Error executing handle PAS:' + JSON.stringify(kv_result);
    console.error(error_message);
    throw new Error(error_message);
}
```

#### Loading the UDF

UDFs are also ingested through Delta files. Build the delta file:

```sh
./builders/tools/bazel-debian build //docs/protected_app_signals/examples:ad_retrieval_udf
```

##### GCP

Upload the delta file to the bucket

```sh
export GCS_BUCKET=your-gcs-bucket-id
gsutil cp bazel-bin/docs/protected_app_signals/examples/DELTA_0000000000000002 gs://${GCS_BUCKET}
```

More [details](../data_loading/loading_data.md#upload-data-files-to-gcp)

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
  string_output: "{\"ad_id1\":{\"value\":\"ad_id1_value\"}}"
}
```

### Lookup Path

The list of IDs is provided by the buyer in the contextual path. The server will lookup the data
associated with the IDs.

No need to load a UDF here, since the default UDF for PAS usecase should handle that.

#### Input

```proto
metadata {
  fields {
    key: "is_pas"
    value {
      string_value: "true"
    }
  }
}
partitions {
  arguments {
    data {
      list_value {
        values {
          string_value: "ad_id1"
        }
        values {
          string_value: "ad_id2"
        }
      }
    }
  }
}
```

#### Output

```proto
single_partition {
  string_output: "{\"ad_id1\":{\"value\":\"ad_id1_value\"},\"ad_id2\":{\"status\":{\"code\":5,\"message\":\"Key not found: ad_id2\"}}}"
}
```
