# Metrics to pay attention to

You can group by `shard_number` for all metrics below.

## Counters

LookupFuturesCreationFailure

ShardedLookupFailure

ShardedLookupServerRequestFailed

LookupClientMissing

ShardedLookupServerKeyCollisionOnCollection

InternalRunQueryEmtpyQuery

InternalRunQueryMissingKeyset

InternalRunQueryParsingFailure

InternalRunQueryKeysetRetrievalFailure

InternalRunQueryQueryFailure

ShardedLookupGrpcFailure

## Latency

InternalRunQuery

## Dashboard

Feel free to import this [dashboard](../production/terraform/gcp/dashboards/sharding_dashboard.json)
and modify it to suit your needs.
