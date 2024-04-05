# Demo Environment

An existing demo environment exists for the KV server and can be used as a reference for creating
your own environment (e.g. "dev" and "prod").

## Synopsis

```bash
nano demo/us-east1.backend.conf  # Customize backend parameters
nano demo/us-east1.tfvars.json  # Customize input variables to suit your demo environment.
terraform init --backend-config=demo/us-east1.backend.conf --var-file=demo/us-east1.tfvars.json --reconfigure
terraform plan --var-file=demo/us-east1.tfvars.json
terraform apply --var-file=demo/us-east1.tfvars.json
```

## Configuration Files

The files which should be modified for your purposes for each environment that you create are:

-   [us-east1.tfvars.json](demo/us-east1.tfvars.json) - an example configuration file for the KV
    server.
-   [us-east1.backend.conf](demo/us-east1.backend.conf) - contains terraform state bucket location -
    should be edited to point to a state bucket you control.
