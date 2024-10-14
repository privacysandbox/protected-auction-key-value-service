# Set query language

The K/V server supports a simple set query language that can be invoked using three UDF reap APIs
(1) `runQuery("query")`, `runSetQueryUInt32("query")` and `runSetQueryUInt64("query")`. The query
language implements threee set operations `union` denoted as `|`, `difference` denoted as `-` and
`intersection` denoted as `&` and queries use sets of strings and numbers as input.

As an example, suppose that we have indexed two sets of 32 bit integer ad ids targeting `games` and
`news` to the K/V server memory store. We can find the sets of ad ids targeting both `games` and
`news` by intersecting `games` and `news` sets using the following query in a UDF:
`games_and_news_ads = runSetQueryInt("games & news")`.

# Constructing queries

## Query operators

The set query language [grammar](/components/query/parser.yy) supports three binary operators:

-   `union` or `|` operator - e.g., given `A = [1, 2, 3]` and `B = [3, 4, 5]`, then
    `A | B = [1, 2, 3, 4, 5]`.
-   `difference` or `-` operator - e.g., given `A = [1, 2, 3]` and `B = [3, 4, 5]`, then
    `A - B = [1, 2]`
-   `intersection` or `&` operator - e.g., given `A = [1, 2, 3]` and `B = [3, 4, 5]`, then
    `A & B = [ 3 ]`

## Query operands

Queries support three types of sets as operands:

-   set of `uint32` numbers - For working with uint32 numbers. The data loading type is `UInt32Set`
    in [data_loading.fbs](/public/data_loading/data_loading.fbs).
-   set of `uint64` numbers - For working with uint64 (supports also uint32) numbers. The data
    loading type is `UInt64Set` in [data_loading.fbs](/public/data_loading/data_loading.fbs).
-   set of strings - arbitrary length UTF-8 or 7-bit ASCII strings are supported. The data loading
    type is `StringSet` in [data_loading.fbs](/public/data_loading/data_loading.fbs).

## Query syntax

Queries can be constructed using long form operator names (`union`, `difference`, `intersection`) or
short form operator symbols (`|`, `-`, `&`). By default, queries are evaluated left to right and one
can use paretheses to override default precedence, e.g., (1) `A & B - C` is semantically different
from (2) `A & (B - C)` because in (1), `A` is intersected with `B` and then `C` is subtracted from
the result whereas in (2) `A` is intersected with the result of finding the difference between `B`
and `C`.

Note that valid queries always have an operator between two operands. For example, given three sets
`A`, `B` and `C`, valid queries include `A | B & C`, `A - B - C`, `B & C - A`, but `A | B | C &` is
invalid because of the last `&`.

## Visualizing ASTs for queries

We provide a tool [query_toy.cc](/components/tools/query_toy.cc) that can be used to visualize AST
trees for queries. For example, run the following commands to evaluate and visualize the AST for
`A & B | (C - D)`:

```bash
./builders/tools/bazel-debian build -c opt //components/tools:query_toy &&
bazel-bin/components/tools/query_toy --query="A & C | (B - D)" --dot_path="$PWD/query.dot" &&
dot -Tpng query.dot > query.png
```

This produces the following image: ![query visualization image](assets/query_visualization.png)

# Running queries in UDFs

The K/V server supports two APIs for running queries inside UDFs.

-   `runQuery("query")`
    -   Takes a valid `query` as input and returns a set of strings.
    -   See [run_query_udf.js](/tools/udf/sample_udf/run_query_udf.js) for a JavaScript example.
-   `runSetQueryUInt32("query")`
    -   Takes a valid `query` as input and returns a byte array of serialized 32 bit usinged
        integers.
    -   See [run_set_query_uint32_udf.js](/tools/udf/sample_udf/run_set_query_uint32_udf.js) for a
        JavaScript example.
-   `runSetQueryUInt64("query")`
    -   Takes a valid `query` as input and returns a byte array of serialized 64 bit usinged
        integers.
    -   Note that `uint32` numbers can be loaded using `UInt64Set` and then queries can be evaluated
        using `runSetQueryUInt64("query")`. However, this approach can be significantly less
        efficient than using `UInt32Set` and `runSetQueryUInt32("query")`.
    -   See [run_set_query_uint64_udf.js](/tools/udf/sample_udf/run_set_query_uint64_udf.js) for a
        JavaScript example.

## Which API should I use, `runQuery` vs. `runSetQueryInt`?

`runSetQueryUInt32` and `runSetQueryUInt64` implements a much more perfomant version of query
evaluation based on bitmaps. So (1) if your sets can be represented as 32 bit unsigned integer sets
and are relatively dense compared to the range of numbers, then use `runSetQueryUInt32` and (2) if
your sets must be represented as 64 bit unsigned integer sets and are relatively dense compared to
the range of numbers, then use `runSetQueryUInt64`. We also provide a benchmarking tool
[query_evaluation_benchmark](/components/tools/benchmarks/query_evaluation_benchmark.cc) that can be
used to determine whether using integer sets vs. string sets would be much more performant for a
given scenario. For example:

```bash
./builders/tools/bazel-debian run -c opt //components/tools/benchmarks:query_evaluation_benchmark -- \
    --benchmark_counters_tabular=true \
    --benchmark_time_unit=us \
    --set_size=500000 \
    --range_min=1000000 \
    --range_max=2000000 \
    --set_names="A,B,C,D" \
    --query="(A - B) | (C & D)"
```

benchamrks evaluating set operations and the query `(A - B) | (C & D)` using sets with 500,000
elements randomly selected from the range `[1,000,000 - 2,000,000]`. On my machine, the benchmark
produces the following output which shows superior performance for integer sets:

```bash
Run on (128 X 2450 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x64)
  L1 Instruction 32 KiB (x64)
  L2 Unified 512 KiB (x64)
  L3 Unified 32768 KiB (x8)
Load Average: 30.38, 15.45, 9.58
-----------------------------------------------------------------------------------------------------------
Benchmark                                                      Time             CPU   Iterations      Ops/s
-----------------------------------------------------------------------------------------------------------
kv_server::BM_SetUnion<kv_server::UInt32Set>                15.2 us         15.2 us        45767  65.834k/s
kv_server::BM_SetUnion<kv_server::UInt64Set>                22.6 us         22.6 us        30639 44.2866k/s
kv_server::BM_SetUnion<kv_server::StringSet>               51034 us        51031 us           12   19.596/s
kv_server::BM_SetDifference<kv_server::UInt32Set>           16.3 us         16.3 us        44607 61.4837k/s
kv_server::BM_SetDifference<kv_server::UInt64Set>           22.9 us         22.9 us        30509 43.6579k/s
kv_server::BM_SetDifference<kv_server::StringSet>          21059 us        21057 us           33  47.4906/s
kv_server::BM_SetIntersection<kv_server::UInt32Set>         16.4 us         16.4 us        42387 60.9809k/s
kv_server::BM_SetIntersection<kv_server::UInt64Set>         22.0 us         22.0 us        31571 45.3572k/s
kv_server::BM_SetIntersection<kv_server::StringSet>        21156 us        21156 us           33  47.2686/s
-------------------------------------------------------------------------------------------------------------
Benchmark                                                      Time             CPU   Iterations QueryEvals/s
-------------------------------------------------------------------------------------------------------------
kv_server::BM_AstTreeEvaluation<kv_server::UInt32Set>       43.8 us         43.8 us        15727   22.8436k/s
kv_server::BM_AstTreeEvaluation<kv_server::UInt64Set>       63.0 us         63.0 us        11544   15.8808k/s
kv_server::BM_AstTreeEvaluation<kv_server::StringSet>      62148 us        62135 us           11    16.0939/s
```

# Loading sets into the K/V server

See [data loading guide](data_loading/loading_data.md) on how to load sets into the K/V server.
