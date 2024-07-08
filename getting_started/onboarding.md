# KV server onboarding and self-serve guide

This document provides guidance to adtechs to onboard to Key Value server.

You may refer to KV services timeline and roadmap [here][1]. You may refer to the high level
architecture and design [here][2]. You may also refer to [privacy considerations][8], [security
goals][9] and the [trust model][3].

If you have any questions, please submit your feedback by filing a github issue for this repo.
Alternatively, you can email us at <privacy-sandbox-trusted-kv-server-support@google.com>

Following are the steps that adtechs need to follow to onboard, integrate, deploy and run KV
services in non-production and production environments.

## Step 1: Enroll with Privacy Sandbox

Refer to the [guide][13] to enroll with Privacy Sandbox. This is also a prerequisite for [enrolling
with Coordinators][14].

## Step 2: Cloud set up

Adtechs need to choose one of the currently [supported cloud platforms][27] to run services.

Refer to the corresponding cloud support explainer for details:

-   [AWS support][4]
    -   Adtechs must set up an [AWS account][34], create IAM users and security credentials.
-   [GCP support][5]
    -   Create a GCP Project and generate a cloud service account. Refer to the [GCP project
        setup][6] section for more details.

## Step 3: Enroll with Coordinators

Adtechs must enroll with [Coordinators][37] for the specific [cloud platform][27] where they plan to
run KV services in production environments ([Beta testing][7] and [GA][10]).

The Coordinators run [key management systems (key services)][43] that provision keys to KV services
running in a [trusted execution environment (TEE)][44] after service attestation. Integration of KV
server workloads with Coordinators would enable TEE server attestation and allow fetching live
encryption / decryption keys from [public or private key service endpoints][43] in KV services.

To integrate with Coordinators, adtechs will have to follow the steps below:

-   Set up a cloud account on a preferred [cloud platform][27] that is supported.
-   Provide the Coordinators with specific information related to their cloud account.
-   Coordinators will provide url endpoints of key services and other information that must be
    incorporated in KV server configurations.

Adtechs would have to enroll with Coordinators running key management systems that provision keys to
KV services after server attestion.

Adtechs should only enroll with the Coordinators for the specific cloud platform where they plan to
run KV services.

### `use_real_coordinators`

KV supports [cryptographic protection][45] with hardcoded public-private key pairs, while disabling
TEE server attestation. During initial phases of onboarding, this would allow adtechs to test KV
server workloads even before integration with Coordinators.

-   Set `use_real_coordinators` flag to `false` ([AWS][12], [GCP][11]).
-   _Note: `use_real_coordinators` should be set to `true` in production_.

### Amazon Web Services (AWS)

An adtech should provide their AWS Account Id. The Coordinators wil create IAM roles. They would
attach that AWS Account Id to the IAM roles and include in an allowlist. Then the Coordinators would
let adtechs know about the IAM roles and that should be included in the KV server Terraform configs
that fetch cryptographic keys from key management systems.

Following config parameters in KV server configs would include the IAM roles information provided by
the Coordinators.

PRIMARY_COORDINATOR_ACCOUNT_IDENTITY SECONDARY_COORDINATOR_ACCOUNT_IDENTITY

### Google Cloud Platform (GCP)

An adtech should provide [**IAM service account email**][107] to both the Coordinators.

The Coordinators would create IAM roles. After adtechs provide their service account email, the
Coordinators would attach that information to the IAM roles and include in an allowlist. Then the
Coordinators would let adtechs know about the IAM roles and that should be included in the B&A
server Terraform configs that fetch cryptographic keys from key management systems.

Enroll with Coordinators via the Protected Auction [intake form][51]. You will need to provide the
service account or aws account id created in the previous step.

_Note:_

-   _Adtechs can only run images attested by the key management systems_ _(Coordinators) in
    production._
-   _Without successfully completing the Coordinator enrollment process, adtechs_ _will not be able
    to run attestable services in TEE and therefore will not be_ _able to process production data
    using KV services._
-   _Key management systems are not in the critical path of KV services. The_ _cryptographic keys
    are fetched from the key services in the non critical_ _path at service startup and periodically
    every few hours, and cached in-memory_ _server side. Refer here for more information._

## Step 4: Build, deploy services

Follow the steps only for your preferred cloud platform to build and deploy B&A services.

### KV code repository

KV server code and configurations are open sourced in this repo.

Starting with 0.17 KV, releases hashes are allowlisted with coordinators on GCP and AWS.

### Prerequisites

The following prerequisites are required before building and packaging KV services.

-   AWS: Refer to the prerequisite steps [here][15].
-   GCP: Refer to the prerequisite steps [here][16].

### Build service images

To run KV services locally, refer [here][17].

-   AWS: Refer to the detailed instructions [here][18].
-   GCP: Refer to the detailed instructions [here][19].

### Deploy services

_Note: The configurations set the default parameter values. The values that are_ _not set must be
filled in by adtechs before deployment to the cloud._

[AWS][28]

[GCP][29]

## Step 5: Pick your usecase

KV supports the following usecases:

-   [Protected Audience (PA)][20]
-   [Protected App Signals (PAS)][21]

## Step 6: Enable debugging and monitor services

Refer to the [debugging explainer][80] to understand how user consented debugging can be used in
production.

Refer to the [monitoring explainer][81] to understand how cloud monitoring is integrated and service
[metrics][82] that are exported for monitoring by adtechs.

## Github issues

Adtechs can file issues and feature requests on this Github project.

[1]:
    https://github.com/privacysandbox/protected-auction-key-value-service/tree/main?tab=readme-ov-file#timeline-and-roadmap
[2]: /docs/APIs.md
[3]:
    https://github.com/privacysandbox/protected-auction-services-docs/blob/main/key_value_service_trust_model.md
[4]: /docs/deployment/deploying_on_aws.md
[5]: /docs/deployment/deploying_on_gcp.md
[6]:
    https://github.com/privacysandbox/protected-auction-key-value-service/blob/main/docs/deployment/deploying_on_gcp.md#set-up-your-gcp-project
[7]:
    https://github.com/privacysandbox/protected-auction-key-value-service/tree/main?tab=readme-ov-file#june-2024-beta-release
[8]:
    https://github.com/privacysandbox/fledge-docs/blob/main/trusted_services_overview.md#privacy-considerations
[9]:
    https://github.com/privacysandbox/fledge-docs/blob/main/trusted_services_overview.md#security-goals
[10]:
    https://github.com/privacysandbox/protected-auction-key-value-service/tree/main?tab=readme-ov-file#h2-2024-android-pa-ga-pas-ga
[11]:
    https://github.com/privacysandbox/protected-auction-key-value-service/blob/9a60180f9d6f52a4ca805e5463ecc9e5e80e88f9/production/terraform/gcp/environments/kv_server_variables.tf#L193
[12]:
    https://github.com/privacysandbox/protected-auction-key-value-service/blob/9a60180f9d6f52a4ca805e5463ecc9e5e80e88f9/production/terraform/aws/environments/kv_server_variables.tf#L216
[13]: https://developers.google.com/privacy-sandbox/relevance/enrollment
[14]: #step-3-enroll-with-coordinators
[15]: /docs/deployment/deploying_on_aws.md#set-up-your-aws-account
[16]: /docs/deployment/deploying_on_gcp.md#set-up-your-gcp-project
[17]: /docs/deployment/deploying_locally.md
[18]: /docs/deployment/deploying_on_aws.md#build-the-keyvalue-server-artifacts
[19]: /docs/deployment/deploying_on_gcp.md#build-the-keyvalue-server-artifacts
[20]: /docs/protected_audience/integrating_with_fledge.md
[21]: /docs/protected_app_signals/ad_retrieval_overview.md
[27]:
    https://github.com/privacysandbox/fledge-docs/blob/main/bidding_auction_services_api.md#supported-public-cloud-platforms
[28]:
    https://github.com/privacysandbox/protected-auction-key-value-service/blob/release-0.16/docs/deployment/deploying_on_aws.md
[29]:
    https://github.com/privacysandbox/protected-auction-key-value-service/blob/release-0.16/docs/deployment/deploying_on_gcp.md#deployment
[34]:
    https://docs.aws.amazon.com/signin/latest/userguide/introduction-to-iam-user-sign-in-tutorial.html
[37]:
    https://github.com/privacysandbox/fledge-docs/blob/main/trusted_services_overview.md#deployment-by-coordinators
[43]:
    https://github.com/privacysandbox/fledge-docs/blob/main/trusted_services_overview.md#key-management-systems
[44]:
    https://github.com/privacysandbox/fledge-docs/blob/main/trusted_services_overview.md#trusted-execution-environment
[45]:
    https://github.com/privacysandbox/fledge-docs/blob/main/bidding_auction_services_api.md#client--server-and-server--server-communication
[51]:
    https://docs.google.com/forms/d/e/1FAIpQLSduotEEI9h_Y8uEvSGdFoL-SqHAD--NVNaX1X1UTBeCeEM-Og/viewform
[80]:
    https://github.com/privacysandbox/fledge-docs/blob/main/debugging_protected_audience_api_services.md
[81]:
    https://github.com/privacysandbox/fledge-docs/blob/main/monitoring_protected_audience_api_services.md
[82]:
    https://github.com/privacysandbox/fledge-docs/blob/main/monitoring_protected_audience_api_services.md#proposed-metrics
[107]:
    https://github.com/privacysandbox/bidding-auction-servers/blob/main/production/deploy/aws/terraform/environment/demo/README.md#using-the-demo-configuration
