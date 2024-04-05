## Directory Structure

### AWS/GCP

The root directory for Terraform templates is `${project_root}/production/terraform/${platform}`,
where `${platform}` is eigher `aws` or `gcp`.

Within the root directory, the templates are organized as follows:

-   `/services` contains parameterized resources that can be imported and deployed in various
    environments.
-   `/modules` contains collections of services deployed together to define a server.
-   `/environments` contains the definitions of the various environments for the application (e.g.
    dev, staging, production), importing the various service modules and providing parameters to
    them.

### Usage

Terraform operations should be performed on a particular environment. See the
[AWS environment guide](/production/terraform/aws/environments/README.md) and
[GCP environment guide](/production/terraform/gcp/environments/README.md) for actual usage.
