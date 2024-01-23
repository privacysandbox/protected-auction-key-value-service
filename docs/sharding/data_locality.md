# Overview

Data locality allows an AdTech to put some keys on the same shard, e.g. keys A, B can be explicitly
marked to be put on the same shard.

Data locality provides a solution to the limitations around transactions, versioning and data
sharding explained below. A set of versioned data can be configured to reside on the same shard,
allowing for the cross row transaction to be executed.

This doc assumes familiarity with other sharding docs in this folder.

# Transactions, versioning and data sharding limitations

The KV server:

-   Does _not_ currently have cross row transactions
-   Does _not_ load the rows in a delta file sequentially
-   Does load delta files in order

The implications of these limitations on versioned data is that consistent cross row versioned data
is a job for the adtech operator.

## Process for non sharded KV Server

If an AdTech has two rows with keys ["A", "B"], that must be updated within a "transaction" the
following technique can be used:

-   Encode a version number into the keys. Ex. ["A_V1", "B_V1", ]
-   Add a new key "Version" whose value is the version number to use. Ex. "V1"
-   When querying a key "A" or "B"
    -   First query "Version"
    -   Append the version value to the key and then query them ["A_V1", "B_V1"]
-   When creating a new version do the following in different Delta files. Each file will be
    processed in order, each row may be processed out of order.
    -   Delta_1
        -   Add new version "A_V2" = "new_a_value"
        -   Add new version "B_V2" = "new_b_value"
    -   Delta_2
        -   Update Version = "V2"

## Process limitations for the sharded KV server

The above process only works when all of the versioned data and the versioned key reside on the same
shard. There are no data guarantees around this, without the data locality feature enabled.

Without that feature, AdTechs have limited ability to dictate which data can live on which shard. An
AdTech owns the "key space" in today's sharding world, they could do the work to cleverly name
(some) keys in a way to force colocation. This would be computationally expensive and would
obfuscate key names.

# Data loading and serving flow with the data locality feature

A regex can be set by an AdTech on startup as a terraform
[parameter](https://github.com/privacysandbox/fledge-key-value-service/blob/b047d89ebfa6312ec8d1de275da69fd60d24eba3/production/terraform/aws/environments/kv_server_variables.tf#L254)
that will be applied to all keys. This
[parameter](https://github.com/privacysandbox/fledge-key-value-service/blob/b047d89ebfa6312ec8d1de275da69fd60d24eba3/production/terraform/aws/environments/kv_server_variables.tf#L248)
should be set to `true`. The regex is global in the sense that there is only 1 regex and it is known
to all shards.

If a KV server has more than 1 shard, a regex is set and a match is produced during data loading or
data lookup, that match is treated as the sharding key. If there is no match or no regex, then the
whole key is treated as the sharding key.

E.g. given a key `Customer1_somevalue1`, and a regex - `^(.*)_.*$` - we get a shard_key `Customer1`.
The sharding function is then applied to the shard_key.

That way the following two keys: `Customer1_somevalue1`, `Customer1_somevalue2` will be placed on
the same shard.

## Changing the regex

If a regex is specified, changing the regex later can be challenging. The regex affects how the data
is loaded and stored. So any regex change, unless it is trivial, will result in the necessity to
fully reload the data with keys matching the new regex.

The standard advice of bringing up a second environment and then forwarding traffic to the new
system can be applied here, if downtime is to be avoided.

# Changes to the AdTechs flow

-   Come up with a regex and specify it as a parameter.
-   Ensure that all keys that use the data locality feature follow the naming convention specified
    by the regex.
-   Shard data when generating deltas following the steps specified here.
