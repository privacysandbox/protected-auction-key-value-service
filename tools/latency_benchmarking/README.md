TODO: Write more extensive README

Example:

From the workspace root:

```sh
cp <MY_SNAPSHOT_FILE> tools/latency_benchmarking/SNAPSHOT_0000000000000001
NUMBER_OF_LOOKUP_KEYS_LIST="1 10 100"
SERVER_ADDRESS="lusa.kv-server.privacysandboxdemo.app:8443"
./tools/latency_benchmarking/run_benchmarks --number-of-lookup-keys-list "${NUMBER_OF_LOOKUP_KEYS_LIST}" --server-address ${SERVER_ADDRESS}
```

Outputs a summary to `dist/tools/latency_benchmarking/output/<timestamp>/summary.csv`.
