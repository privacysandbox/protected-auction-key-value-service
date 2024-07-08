!! This workflow works only for
[Protected App Signals (PAS)](https://developers.google.com/privacy-sandbox/relevance/protected-audience/android/protected-app-signals).
It does not support Protected Audience (PA), since the UDF and Server API is different.!!

## Background

This document provides a detailed overview of the Ad Retrieval server, which is a server-side
component of the Protected App Signals (PAS) API. The Ad Retrieval server must run within a trusted
execution environment (TEE).

The PAS API flow at a high level is:

1. Ad tech companies would curate signals from a variety of sources, including first-party data,
   contextual data, and third-party data.
1. The signals would be stored securely on the device.
1. Ad tech companies would have access to the signals during a Protected Auction to serve relevant
   ads.
1. **Ad tech custom logic deployed in Trusted execution environments can access the signals to do
   real-time ad retrieval and bidding**
1. Buyers submit ads with bids to sellers for final scoring to choose a winning ad to render.

The Ad retrieval service is a proposed solution for performing ad and creative targeting, as listed
in the step #4. In general, ad retrieval typically includes ads matching, filtering, scoring,
ranking and top-k selection. It is developed based on the Trusted Execution Environment (TEE) on
selected public cloud platforms.

Today, the ad retrieval service is implemented as part of the Privacy Sandbox's TEE key value
service. The privacy characteristics conform to
[the KV service trust model](https://github.com/privacysandbox/fledge-docs/blob/main/key_value_service_trust_model.md).

## Use case overview

Today ad techs curate data from multiple sources and use this data to choose relevant ads for the
user. Ad requests are typically accompanied by an AdID and publisher provided data (ex OpenRTB)
which is used to determine most relevant ads for the user based on a user profile keyed on the AdiD.

Ad candidates are filtered down to a few relevant ones for an ad request. This is the retrieval
phase and the focus of this document. Bids are computed for the selected set of ads. Top bids are
sent to the seller who would score and pick the winning ad. The winning ad is then rendered by the
client.

![alt_text](../assets/ad_retrieval_use_case_overview.png 'use case overview')

#### Figure 1: High level layout of the retrieval flow

An ad tech has up to N active ad campaigns at any given time. To choose relevant ads for each ad
retrieval request an ad tech needs to filter the ads to the top K number of ads. To do this ad techs
typically follow a multi stage filtering process that looks something like the following:

![alt_text](../assets/ad_retrieval_filter_funnel.png 'image_tooltip')

### Coarse-grained selection

Ad techs filter a large number of ads (e.g., 10,000s) to a few (e.g., 1,000s) using device and
publisher provided signals, such as geo, ad slot type size, language, etc.

### Filtering

This filtering will further reduce the number of ads that are eligible to be shown. For example, ad
tech uses real-time signals, such as the amount of budget left for campaigns and other information,
status of the campaign (active / inactive etc.) to further reduce the number of ads from thousands
to hundreds.

### Lightweight scoring and Top-K selection

From the remaining ad candidates, ad techs can reduce the number of results returned by scoring,
sorting, and truncating the list to the top K results. The lightweight scores can be computed using
inputs such as the embedding vectors sent in via the request and per-ad embedding vectors stored in
the Ads dataset. The set of candidates fetched at this stage can be further narrowed down during a
bidding phase using more powerful inference capabilities. The bidding phase is outside the scope of
the retrieval service.

Note: This is just a general overview of the ad tech selection process. The specific steps and
processes involved may vary depending on the individual ad tech implementation.

## Ad retrieval walkthrough

![alt_text](../assets/ad_retrieval_walkthrough.png 'walkthrough')

<!-- markdownlint-disable MD013 -->

#### Figure 2: Typical setup in one region. Components in and surrounded by purple are part of the PAS system. Components in yellow are developed by the ad tech operator, proprietary to the ad tech and may differ vastly between one ad tech and another.

<!-- markdownlint-enable MD013 -->

### How to deploy the system and load data to it

This section describes how data is loaded into the server.

#### Deployment

The ad tech builds the service system by downloading the source code from the
[Github repository](https://github.com/privacysandbox/fledge-key-value-service) and following the
documentation in the repository.

The ad tech deploys the system to a supported public cloud of their choice. At time of publication,
the system will be available on GCP and AWS.

The service runs in the same network as the Bidding & Auction services. The Bidding & Auction
services' configuration is updated to send Ad retrieval requests to the Ad Retrieval service.

#### Data loading

The ad retrieval service consumes data generated from ad techs' own custom data generation systems.
The retrieval service development team anticipates that various forms of data generation systems
will be used, such as periodic batch files, constant streaming files or pub/sub:

-   For data that require low latency propagation, such as budgets, the ad tech bundles the data
    into a specific format of file and pushes the data file into the Cloud's pub/sub system, which
    then pushes the data to the servers.
-   For data that does not require low latency propagation, the ad tech writes them into the same
    format of files, uploads them to the Cloud's blob storage system such as AWS S3, and the service
    will pick up the data.

For privacy reasons, the service loads data into its RAM and serves requests from there.

For details, see
"[Loading data into the key value server](https://team.git.corp.google.com/kiwi-air-force-eng-team/kv-server/+/refs/heads/main/docs/loading_data.md)".

#### Data model

The dataset in the server RAM is organized as simple key value pairs. The key is a string and value
can either be a string or a set of strings.

##### Design pattern: Ad hierarchy

The data model being simple KV pairs does not mean all data must be flat. The ad tech may use a
hierarchical model such as Campaign -> Ad Group -> Creative. The ad tech can use these as key prefix
to represent "tables" such as `"campaign_123_adgroup"`.

At request processing time, it is up to the ad tech to perform "joins". The ad tech can first look
up certain campaigns, then look up ad groups for those campaigns. This allows ad techs to store
campaign-level information only once in the campaign entry.

##### Design pattern: use "set" to represent a group of values, such as ads or ad groups that share a certain feature

There are 2 categories of values: singular values and sets.

-   Singular values are the classic building blocks of key value lookups. The server can load rows
    of data where each row is a key value pair and the value is a singular value. It is the most
    versatile way to model the data.
-   Sets are supported to optimize common operations used in ad matching, which includes union,
    intersection and difference. During data loading, each row can specify which type the row uses.
    In this case the key value pair is a key and a set of values.

A typical use case of sets is to have the key be a feature (or the lack of such feature) and its set
the group of ads/campaigns/etc that satisfies this feature. During request processing, the request
may be looking for ads that have M features and do not have N features. The user-defined retrieval
logic (described below) would perform an intersection and difference of the ads of the features.
Using sets and the accompanying RunQuery API (described below) would save the user the effort of
implementing this.

##### Design pattern: versioning

Cross-key transactions are not supported. A mutation to one key value pair is independent of other
updates. If a collection of ads metadata requires query integrity, such that the queried ads
metadata of certain campaign or customer must be from the same data snapshot (This only applies to
when the upstream data generation is periodic batch based), one way to ensure it is to append a
per-snapshot version to each key, and have a specific key/value pair for the version. During
lookups, the version key is first queried so the version can be used to construct the actual ad
metadata query.

The ad tech defines the order of mutations by adding a logical timestamp to each mutation. The
service accepts/evicts records based on the timestamp but there is no guarantee that the service
receives the mutations in chronological order.

### Processing requests with ad tech-specific User Defined Functions

This phase explains the components 2 ("input signals") and 3 ("retrieval logic") in
[Figure 1](#figure-1-high-level-layout-of-the-retrieval-flow)

Ad techs cannot change the server code inside TEE. But the server provides a flexible way to execute
custom logic through "User defined functions" ("UDF"). Today, UDF supports JavaScript or WebAssembly
(WASM) which supports many languages.

A UDF is not a single function. Similar to running a JavaScript on a web page load where it can call
other dependency libraries during the execution, a UDF entrypoint is invoked by the server on each
retrieval request and it can call other functions possibly defined in other files and make use of
all the language constructs like polymorphism, templating, etc. A UDF refers to the collection of
all the code that will run inside the server.

The ad tech writes the UDF code in their preferred way, converts it into a file with specific format
recognizable by the service, and stores it in a Cloud blob storage location. The service monitors
the location and picks up the UDF as it appears.

There is one UDF invocation per request (one request per auction).

#### UDF API

In the initial version, the server uses a JavaScript wrapper layer even if WASM is used. So for
`byte[]` inputs, the input will be a base64 encoded string.

```javascript
string HandleRequest(
  requestMetadata,
  protectedSignals,
  deviceMetadata,
  contextualSignals,
  contextualAdIds,
)
```

-   requestMetadata: JSON. Per-request server metadata to the UDF. Empty for now.
-   protectedSignals: arbitrary string, originating from the device, and passed from the bidding
    service. The Protected App Signals can be decoded in the UDF to produce embeddings for top K ads
    ranking, and other information useful for the retrieval. Note: at this stage the Protected App
    Signals would be decoded and unencrypted.
-   deviceMetadata: JSON object containing device metadata forwarded by the Seller's Ad Service. See
    the
    [B&A documentation](https://github.com/privacysandbox/fledge-docs/blob/main/bidding_auction_services_api.md#metadata-forwarded-by-sellers-ad-service)
    for further details.
    -   `X-Accept-Language`: language used on the device.
    -   `X-User-Agent`: User Agent used on the device.
    -   `X-BnA-Client-IP`: Device IP address.
    -   Example:

```javascript
{
  "X-Accept-Language": "en-US",
  "X-User-Agent": "ExampleAgent",
  "X-BnA-Client-IP": "1.1.1.1"
}
```

-   [contextualSignals:](https://github.com/privacysandbox/bidding-auction-servers/blob/b222e359f09de60f0994090f7a57aa796e927345/api/bidding_auction_servers.proto#L945)
    arbitrary string originated from the contextual bidding server operated by the same DSP. The UDF
    is expected to be able to decode the string and use it. Contextual Signals may contain any
    information such as ML model version information for the protected embedding passed in via
    Protected App Signals.
-   contextualAdIds: JSON object containing an optional list of ad ids.
-   Output: string. This will be sent back to the bidding service and passed into the `generateBid`
    function for bid generation.

![alt_text](../assets/ad_retrieval_udf.png 'ad_retrieval_udf')

#### Figure 3: Zoomed-in view of request path.

#### API available to UDF

While the server invokes the UDF to process requests, the UDF has access to a few APIs provided by
the server to assist certain operations. The usage will be explained concretely in the example
below.

-   `getValues([key_strings])`: Given a list of keys, performs lookups in the loaded dataset and
    returns a list of values corresponding to the keys.
-   `runQuery(query_string)`: UDF can construct a query to perform set operations, such as union,
    intersection and difference. The query uses keys to represent the sets. The keys are defined as
    the sets are loaded into the dataset. See the exact grammar
    [here](https://github.com/privacysandbox/fledge-key-value-service/blob/main/components/query/parser.yy).

For more information, see
[the UDF spec](https://github.com/privacysandbox/fledge-docs/blob/main/key_value_service_user_defined_functions.md).

#### Example

The server is loaded with a large amount of ad candidates and their associated metadata. In this
case we limit the metadata to an embedding and a budget. The embedding is a vector of floats and the
budget is an integer USD cents.

We also have loaded the server with a mapping for each Protected App Signals and Device Metadata to
every ad that is associated with it (ex. `Games->[ad_group1, ad_group2, ad_group3,...]`)

The Protected App Signals contains a list of the of apps that have store signals on the device by
the adtech. As a reminder signals are only availbe to teh adtech whom stored the signals..

Ad tech writes the UDF for example in C++. They can use all the C++ primitives such as classes,
polymorphism, templates, etc. They use a compiler to compile the code into WASM. The entrypoint of
the code has access to the input.

The UDF goes through multiple stages of logic.

#### Stage 1: Ad matching

The ad tech wants to find some ads related to the types of apps installed on a device, in this
example "games" and "news". This can be done by first looking up the mapping, finding the list of
ads associated with games and news then performing an intersection of the 2 lists.

The service provides a few APIs to UDFs to access loaded data. The above operation can be done by
using the "RunQuery" API. The ad tech constructs a query to find the intersection of ad groups
associated with certain app types: `"games & news"` and calls `RunQuery("games & news")` which
returns `["ad group1", "ad group3"]`.

##### Design pattern: Campaign liveness

The ad tech wants to exclude certain ad groups from bidding if budget disallows. They model this
with a key/value pair where the key is "`disabled_adgroups`" and the value is a list of ad groups
`["ad group123", "ad group789", ...]`.

The ad tech updates this key with the low latency pub/sub mechanism whenever an ad group encounters
a budget issue. During ad matching, the ad tech performs a difference in its query to exclude these
ad groups, e.g., `RunQuery("(games & news) - disabled_adgroups")`.

#### Stage 2: Filtering

A large set of ad candidates is produced by the matching stage. These candidates are then fed into a
filtering stage, which can contain any number of filters. One example of a filter is negative
filtering, which is used to prevent an app-install ad from being shown for an app that is already
installed on the device.

The device can store the information of "apps already installed on the device" as the protected app
signals sent to the ad retrieval server. The specific design of the information can take various
forms, such as a collection of hashes of the package names of the apps.

If the filter computes that the corresponding app of an ad candidate exists on the device, the ad
candidate is dropped.

#### User constraints

If the dataset is large, the ad tech would need to enable sharding to store it in multiple clusters
of machines. RunQuery and GetValues APIs would perform remote calls which are slower than local
executions when the dataset is small.

### Scoring and Top-K selection (Last UDF stage)

This operation corresponds to the previously mentioned lightweight scoring and Top-K selection,
which happens after the ad matching and filtering.

#### Design pattern: Dot product selection

Suppose the ad tech wants to perform dot-product based scoring.

After filtering, the UDF has a relatively small amount of ads and their metadata.

The UDF logic uses GetValues API to query the ad embeddings of these ads, previously loaded into the
system through earlier phases.

In the request input there are the protected user embeddings generated by the Bidding Service using
Protected App Signals. The UDF could perform a dot product between user and contextual embedding and
an ad embedding that would come from the retrieval data to rank the results and select top K ads.

The UDF may perform another GetValues call on the K ads to look up additional metadata. The UDF then
aggregates them and constructs the final response according to the UDF return value format and
returns it. The service takes the return value and returns it to the calling Bidding service, which
performs further bid generation logic.

#### Design pattern: versioning of embeddings

The ad tech should specify which version of model/embedding should be used so in the case they are
using
[model factorization](https://developer.android.com/design-for-safety/privacy-sandbox/protected-app-signals#model-factorization)
the ad embeddings and user embeddings' versions match. The version can be passed in as part of the
per_buyer_signals in the request. In the dataset, the embeddings' keys have a version. The UDF
constructs the keys by conforming to the naming convention defined by the ad tech. For example the
version in the request may be `"5"` and the UDF can build a key: `"ad_123_embedding_v5"`.

#### Design pattern: linear regression ML selection

Instead of a dot product, the ad tech may link some ML libraries with their UDF and perform ML-based
selection. While in general this follows the same flow as the dot-product, there are multiple
constraints for this approach.

#### User constraints:

The retrieval server does not provide additional ML support on the server framework level. I.e., all
ML operations can only be performed within the UDF space. If WASM has constraints supporting e.g.,
Tensorflow, it will be constraints of the retrieval service. There is also no accelerator.

Same as data and UDF, the ML model must be looked up before using it. The lookup may be a remote
call. And the model needs to be small to not contend with other RAM usage, which is prominent in the
retrieval server.

Data loading has propagation delay. It may take minutes for every replica of the retrieval server to
have key/value pairs of a certain version. When setting the version in the request, it is better to
not set the latest version right away lest the lookups fail. This applies in general to all lookups.
However, the server will provide visibility to the state of the data loading.

### Ad retrieval output

The UDF should return a string on success. The string is returned to the bidding server which then
passes it to the `generateBid()` function. Although the string can just be a simple string, most
likely the string should be a serialized object whose schema is defined by each ad tech on their
own. There is no constraint on the schema as long as the ad tech's `generateBid()` logic can
recognize and use the string.

The reason that the string likely should be a serialized object is we expect that the use case would
require the ad retrieval server output to contain a dictionary of ad candidates and their
information. The ad candidates can be keyed by certain unique ids and the information may contain
the ad embeddings, and various pieces to construct the final rendering URL, such as the available
sizes of the ad. The `generateBid()` function may then generate the final render URL
`https://example.com/ads/4/123?w=1200&h=628` using contextual information.

Example schema:

```javascript
{ // Ad candidates and their information
  "123": {
    "campaign": 4,
    "sizes": [[1200, 628], [1200, 1500]],
    "embeddings": "ZXhhbXBsZSBlbWJlZGRpbmc=",
    "rank": 1
  },
  "456": {
    "campaign": 6,
    "sizes": [[1200, 628], [1200, 1500]],
    "embeddings": "YW5vdGhlciBleGFtcGxlIGVtYmVkZGluZw=="
    "rank": 2
  }
}
```

## Concrete example and specification

The service codebase has an end-to-end example in the context of information retrieval.

Documentation:

-   High level: <https://github.com/privacysandbox/fledge-docs>
-   Lower level: <https://github.com/privacysandbox/fledge-key-value-service/tree/main/docs>

Getting started & Example:
<https://github.com/privacysandbox/fledge-key-value-service/tree/main/getting_started>
