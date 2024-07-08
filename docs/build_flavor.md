# Build flavors

## prod_mode vs nonprod_mode

KV server now has two build flavors: prod_mode for running server in TEE against production traffic;
nonprod_mode for running server in local machine and TEE for testing and debugging purposes against
non-production traffic. The default build mode is prod_mode. To build nonprod_mode, the flag
"--config=nonprod_mode" needs to be provided to the bazel build command.

### prod_mode:

-   Can attest with production key management system thus can decrypt production traffic
-   Console logs will be disabled. Logs in safe code path(non-request related) such as data loading
    and logs for consented requests will be exported by open-telemetry logger and published to AWS
    Cloudwatch or GCP Logs Explorer.
-   Metrics will be published as unnoised for consented requests and noised for non-consented
    requests.

### nonprod_mode

-   Cannot attest with production key management system thus cannot decrypt production traffic
-   Console logs will be enabled, and additionally all logs will be published to AWS Cloudwatch or
    GCP Logs Explorer if telemetry is configured on from server parameter.
-   All metrics will be published as unnoised.
