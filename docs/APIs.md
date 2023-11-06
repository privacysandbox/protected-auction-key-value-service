# API All-in-One User Guide

This doc summarizes all interfaces and contracts for integrating with the Key Value Server. (More
content to be added)

## Background

The Key Value Server architecture:

![Diagram of the service architecture](/docs/assets/kv_architecture.jpeg)

## Query processing API

![Diagram of API layers](/docs/assets/api_architecture.jpeg)

There are 3 levels of query processing (read) APIs in the KV server.

-   Client side library

    Different applications may have different client libraries in different languages. Used to send
    RPC requests (See below).

-   RPC (Remote procedure call) / HTTP

    The server is aware of the RPC protocol and must have the corresponding logic to handle it. The
    handling logic is in the server framework code and so inside the trust boundary. There is only
    one generic RPC API. (The current codebase has another RPC protocol for compatibility with
    Protected Audience BYOS but that is a special case and will be deprecated in the future.)

-   UDF (User Defined Functions)

    Each request contains one or more partitions. A UDF is called per partition. Having multiple
    partition in a request is purely for performance and batching purposes to reduce the network
    roundtrips.

    UDF code is implemented by Adtechs and opaque to the server.

    The server just needs to pass input arguments to the UDF and return the output through response
    to the clients.

    The server has the ability to pass in different numbers or types of arguments at runtime. This
    does not have to be fixed at compile time or initialization time. Each request can result in a
    unique UDF invocation with its unique number of arguments.

As you can see, only the RPC is owned by the server proper. The rest can be tailored towards more
specific use cases and circumstances.

### Overview

The query processing API architecture, on a high level is that:

-   There is one generic RPC protocol API.
-   Different use cases can have their own overlay API that describes how they would send the
    request and process the response.
-   Different use cases can have dedicated client-side libraries.
-   Different use cases' overlay API also includes what the UDF API is. Users of a specific use case
    write UDF according to that use case's API overlay.

### The generic RPC API

The generic RPC API is available at
[public/query/v2/get_values_v2.proto](/public/query/v2/get_values_v2.proto).

### The generic UDF API

```plaintext
String HandleRequest(UDFExecutionMetadata arg1, [UDFArgument] args)
```

Or in JavaScript form (which the current V8 engine expects):

```js
function HandleRequest(executionMetadata, ...udf_arguments) {}
```

-   The first argument is [UDFExecutionMetadata](/public/api_schema.proto). The UDFExecutionMetadata
    contains metadata in [GetValuesRequest](/public/query/v2/get_values_v2.proto). It may also
    contain other information the server passes in.
-   The rest is the arguments in the request.
-   The output string is stored in ResponsePartition.string_output.

### Use case example: The Protected Audience API overlay

The Protected Audience use case uses the KV server in a particular way. The API is defined
[here](https://github.com/WICG/turtledove/blob/main/FLEDGE_Key_Value_Server_API.md#query-api-version-2).

To process the PA requests, the UDF authors can write their UDF with the following signature:

```js
function HandleRequest(executionMetadata, ...udf_arguments) {
    const keyGroupOutputs = getKeyGroupOutputs(
        executionMetadata.requestMetadata.hostname,
        udf_arguments
    );
    return { keyGroupOutputs, udfOutputApiVersion: 1 };
}
```

The `keyGroupOutputs` schema is defined in the
[PA doc](https://github.com/WICG/turtledove/blob/main/FLEDGE_Key_Value_Server_API.md#response-version-20).

The server supports the current BYOS API. When `route_v1_to_v2` is set to true, (for local mode it
is done through Commandline flag and for cloud mode it is set through parameter store), the same UDF
can process requests in the v1 form of `http://example.com/v1/getvalues?keys=example_key`.
