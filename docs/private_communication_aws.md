# Background

KV EC2 server instances communicate with AWS backend services such as S3, EC2, SQS, SNS, e.t.c.,
using AWS vpc endpoints and gateway endpoints which means that network communication to backend
services does not leave the Amazon network. For more details about vpc and gateway endpoints, see
the
[What are VPV endpoints?](https://docs.aws.amazon.com/whitepapers/latest/aws-privatelink/what-are-vpc-endpoints.html#interface-endpoints)
guide.

# Terraform configuration

vpc and gateway endpoints configuration is here:
[production/terraform/aws/services/backend_services/main.tf](/production/terraform/aws/services/backend_services/main.tf)
Note that the endpoints are associated with an
[IAM policy](/production/terraform/aws/services/backend_services/main.tf#L18) that only allows
server and ssh instances to communicate with the AWS backend services.

## Adding a new backend service's vpc endpoint:

-   Check whether the service integrates with PrivateLink here:
    <https://docs.aws.amazon.com/vpc/latest/privatelink/aws-services-privatelink-support.html> and
    find its service name, e.g., the service name for AWS App Runner is
    `com.amazonaws.region.apprunner`.
-   Add the service name to the terraform list here:
    [aws_vpc_endpoint::vpc_interface_endpoint"](/production/terraform/aws/services/backend_services/main.tf#L53).
    Note that we only need to add the service part of the service name, e.g., for AWS App Runner, we
    will only add `apprunner` to the list.
-   Run `terraform apply ...` to create the new vpc endpoint.

## Adding a new backend service's gateway endpoint:

-   As of now, only S3 and DynamoDB support ingerating with a gateway endpoint
    <https://docs.aws.amazon.com/vpc/latest/privatelink/gateway-endpoints.html>.
-   Since `s3` service is already setup, to add DymanoDB, add `dynamodb` service to the list here:
    [aws_vpc_endpoint::vpc_gateway_endpoint](/production/terraform/aws/services/backend_services/main.tf#L37)

# DNS resolution

Services in a KV server vpc will use the vpc endpoints automatically via a DNS resolver that is
automatically configured in the vpc by AWS. The DNS resolver's address is at +2 of the vpc's base
cidr block, e.g., KV server's default vpc base cidr block is `10.0.0.0` so the private DNS resolver
in a KV server vpc is at address `10.0.0.2`.

[proxify.cc](https://github.com/privacysandbox/data-plane-shared-libraries/blob/578c988ad077fa46056335fd07d316c26f452285/scp/cc/aws/proxy/src/proxify.cc#L41)
is used to configure the `/etc/resolv.conf` file which points to the DNS resolver that the server
process running inside the enclave should use to resolve DNS names. The DNS nameserver configured in
proxify.cc currently is `10.0.0.2` because the base vpc cidr block is `10.0.0.0`.

We are working on changing the DNS resolution process to depend on the parent instance's
`/etc/resolv.conf` because it should always have the correct DNS configuration.
