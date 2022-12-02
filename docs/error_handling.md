# FLEDGE Key/Value server error handling

## Error Handling Principles

Here we are defining initially a few high-level error handling principles. We will continue to
adjust and improve this doc as our understanding of the system and error scenarios becomes clearer.
More importantly, the error handling mechanisms can be considered one aspect of the interfaces
between Adtech users and the system, and therefore we welcome any feedback from the community.

### Prefer infinite retries and alerting over crashing.

-   TEE (Trusted Execution Environment) states disappear after a crash. Instead of crashing, with
    infinite exponential retries and an alerting mechanism, the process may still expose a channel
    to provide privacy-safe debug information.
-   Risk: Infinite retry may not remove the server from the pool even if it is not able to serve. If
    a crash and restart can fix the issue, infinite retries increase the error rate.

### Gracefully fallback to the existing dataset when data loading errors occur.

-   Errors, especially during data loading, can occur for a number of reasons, some of which may
    require reuploading data or reconfiguring some cloud provider resources. Since these might
    require manual fixing, continuing to serve available data minimizes the downtime of the read
    service.
-   Risk: Might serve (partially) stale data.

### Skip delta file/record on errors.

-   When a delta file or record in a delta file cannot be read after a few retries, it will be
    skipped.

### Fire alerts for setup and data loading issues.

-   While the server is setting up AWS resources and reading data, any errors should be exposed
    through alerts. (Setting up alerting is WIP.)
-   The alerts should include the instance and/or resource IDs where possible.

### No Error Checking in TEE scripts.

-   Shell scripts that run in TEE should not do error prechecks/validations since errors cannot be
    inspected outside the TEE. All validations should be done inside the C++ server code where all
    of the error handling mechanisms are developed and used.

### Error messages should include the type of origin where possible.

-   Type can be either data plane or control plane. The data plane represents the server system and
    the control plane represents the cloud and trust infrastructure.
-   Certain errors could come from unclear origins. But this should be kept at a minimum.

## Specific Error Scenarios

Here are some common error scenarios and how they are handled based on the principles.

### Connecting to AWS services

The server tries to connect to multiple AWS services. If it encounters any errors, it will keep
trying to connect to the services instead of crashing.

A lot of errors happen due to misconfiguration or mismatching resource IDs. These should be resolved
either manually through the AWS console/cli or by retrying the Terraform setup.

Some common errors:

-   Call to AWS client (S3Client, SSMClient, etc.) is not allowed by IAM
-   Missing `environment` tag on EC2 instance
-   Missing required parameters in Parameter Store
-   Resource ID (S3 bucket name, SNS topic ARN) not found

While most of these should be detected and fixed during the server startup, changing the expected
resources after the server startup can cause failures as well. Examples include deleting the S3
delta file bucket or SNS topic ARN.

### Data Loading

When the server encounters errors reading delta files, after a few retries, it should generally skip
the file and send an alert.

Some possible error scenarios include

-   Unexpected file format
-   Missing required metadata
-   File corruption
-   OOM issues
