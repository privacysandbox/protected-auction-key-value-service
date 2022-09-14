## Directory Structure

### AWS

The root directory for Terraform templates is `${project_root}/production/terraform/aws`.

Within the root directory, the templates are organized as follows:

-   `/services` contains parametarized resources that can be imported and deployed in various
    environments.
-   `/modules` contains collections of services deployed together to define a server.
-   `/environments` contains the definitions of the various environments for the application (e.g.
    dev, staging, production), importing the various service modules and providing parameters to
    them.

### Usage

Terraform operations should be performed on a particular environment. See the
[environment guide](/production/terraform/aws/environments/README.md) for actual usage.
