# AWS CodeBuild for Key/Value Server

## Overview

This README contains instructions on how to setup [AWS CodeBuild](https://aws.amazon.com/codebuild/)
to build the Key/Value server [AMIs](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/AMIs.html).
These AMIs can then be directly used for Key/Value server deployment on AWS.

### Why do this?

The Key/Value server can take around 2 hours (with 72 cores) to build. If you create an automated
build pipeline that builds new Key/Value server releases, you can avoid manual labor and increase
operational efficiency.

## CodeBuild Configuration

### Prerequisites

#### Choose an AWS Region

Setup will be simplest if you do everything in a single region. Verify that AWS CodeBuild is
available in your chosen region via the
[Global Infrastructure page](https://aws.amazon.com/about-aws/global-infrastructure/regional-product-services/).

#### Service Role

You must first create a role via [IAM](https://aws.amazon.com/iam). This service role will need to
be able to build and upload AMIs, so we will need to modify its permission policies. Attach the
following policy (you can add it as an inline policy via the Role Permissions -> Add permissions
feature in IAM):

```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "ecr-public:GetAuthorizationToken",
                "sts:GetServiceBearerToken",
                "ec2:AttachVolume",
                "ec2:AuthorizeSecurityGroupIngress",
                "ec2:CopyImage",
                "ec2:CreateImage",
                "ec2:CreateKeyPair",
                "ec2:CreateSecurityGroup",
                "ec2:CreateSnapshot",
                "ec2:CreateTags",
                "ec2:CreateVolume",
                "ec2:DeleteKeyPair",
                "ec2:DeleteSecurityGroup",
                "ec2:DeleteSnapshot",
                "ec2:DeleteVolume",
                "ec2:DeregisterImage",
                "ec2:DescribeImageAttribute",
                "ec2:DescribeImages",
                "ec2:DescribeInstances",
                "ec2:DescribeInstanceStatus",
                "ec2:DescribeRegions",
                "ec2:DescribeSecurityGroups",
                "ec2:DescribeSnapshots",
                "ec2:DescribeSubnets",
                "ec2:DescribeTags",
                "ec2:DescribeVolumes",
                "ec2:DetachVolume",
                "ec2:GetPasswordData",
                "ec2:ModifyImageAttribute",
                "ec2:ModifyInstanceAttribute",
                "ec2:ModifySnapshotAttribute",
                "ec2:RegisterImage",
                "ec2:RunInstances",
                "ec2:StopInstances",
                "ec2:TerminateInstances"
            ],
            "Resource": "*"
        }
    ]
}
```

Additionally, increase the `Maximum session duration` to 4 hours by editing the Role Summary. This
is necessary because the build uses the role's session token and we want to avoid the token expiring
while the build is still running.

#### Docker Login Credentials

You will need a [Docker Hub](https://hub.docker.com/) account because the images we use to run the
build are, by default, sourced from Docker Hub.

Create an AWS Secret (via [Secret Manager](https://aws.amazon.com/secrets-manager/)) to store your
Docker Hub password. When creating the secret, choose secret type 'Other' and make sure to store the
password as plaintext (not as a key/value pair). Add the following permissions to your secret:

```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Principal": {
                "AWS": "arn:aws:iam::<YOUR AWS ACCOUNT>:role/<YOUR SERVICE ROLE CREATED ABOVE>"
            },
            "Action": "secretsmanager:GetSecretValue",
            "Resource": "*"
        }
    ]
}
```

The build process will login to the provided Docker Hub account at build time.

### Project

Create a build project in your preferred AWS Region. The following sections will guide you through
the build project configuration.

### Source Repo

Set Github as your source provider, and use the public repository:
`https://github.com/privacysandbox/protected-auction-key-value-service.git`.

#### Webhooks

Webhook events are not available for the 'Public repository' option. If you want to run an automatic
build for every release, you will have to clone the public repo into a Github repo under your
ownership and keep your clone up to date with the public repo. If you prefer to avoid cloning the
public repository, any time you want a new build for Bidding and Auction serivces you must follow
the steps [here](#start-build).

If you do choose to use webhook events, select 'Repository in my Github account', and in the
'Primary source webhook events' section select 'Rebuild every time a code change is pushed to this
repository' for a 'Single build'. Then, add a 'Start a build' filter group of event type `PUSH`,
type `HEAD_REF`, and pattern `^refs/tags/.*`. This will build on every release tag.

Make sure that your Github fork, if updated automatically, also fetches the tags from the upstream
repo -- that way, you can build directly from the semantically versioned tags. See
[here](../../production/packaging/sync_key_value_repo.yaml) for an example Github Action that
handles syncing.

### Environment

Use the following configuration:

1. Provisioning model: On-Demand
1. Environment image: Managed image
1. Compute: EC2
1. Operating system: Ubuntu
1. Runtime: Standard
1. Image: `aws/codebuild/standard:7.0`
1. Image version: Always use latest
1. Service role: see the [service role section](#service-role-permissions).
1. Report auto-discover: disable

Additional configuration:

1. Timeout: 4 Hours
1. Queued timeout: 8 Hours
1. Privileged: Enable
1. Certificate: Do not install
1. Compute: `145 GB memory, 72 vCPUs`
1. Environment variables:

    ```plaintext
    key: AMI_REGION
    value: <YOUR CHOSEN REGION>

    key: DOCKERHUB_USERNAME
    value: <DOCKERHUB USERNAME CREATED IN PREREQUISITES>

    key: DOCKERHUB_PASSWORD_SECRET_NAME
    value: <NAME OF SECRET CREATED IN PREREQUISITES>

    key: BUILD_FLAVOR
    value: <prod or non_prod>
    ```

### Buildspec

Select `Use a buildspec file`. Enter the following:
`production/packaging/aws/codebuild/buildspec.yaml`.

### Batch configuration

Leave blank.

### Artifacts

Type: No artifacts

Additional configuration:

Encryption key: use default

### Logs

Enable CloudWatch logs.

### Service role permissions

Choose the service role you created in the [prerequisites](#service-role) and enable allowing AWS
CodeBuild to modify the role so it can be used with this build project.

### Start Build

Save the changes and create/update the project. Then, click `Start build with overrides` from the
Build projects directory and try building the Key/value server with a desired source version (e.g.,
`main` or `v0.16.0`).

If the build succeeds, you can find the new AMI at the end of the build log.
