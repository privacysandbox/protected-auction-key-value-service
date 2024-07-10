# Diagnostic Tool

The KV server's out-of-box build and terraform solution will handle all the setups required for the
server cloud deployment. However, Adtechs may prefer to use their own server deployment solution
tailored to their production needs. This diagnostic tool is developed to help Adtechs identify and
resolve setup issues that prevent KV server from running properly on their end.

The diagnostic tool only supports AWS environment for now. The tool will need to be deployed and run
on the same EC2 machine where the KV server in enclave is running. The tool will perform several
checks including server health checks and system dependency checks etc, and prints the summary in
the end.

## Check steps performed by the diagnostic tool

1. Enclave server health check. If any of checks in this step is failed, move on to next steps
    - Check if enclave is running
    - Check grpc request against KV server
    - Check http request against KV server
2. System dependencies check. All checks must be passed for the KV server to start and process grpc
   requests. If all checks passed in this step, move on to next step
    - VSock proxy check
    - VPC CIDR and etc/resolv.conf checks. More details about required setup for DNS resolution in
      [private_communication_aws.md](/docs/private_communication_aws.md)
3. Envoy proxy checks. Envoy proxy is not required for the KV server to run and process grpc
   requests, but is required for KV server to receive http requests. All checks must be passed in
   this step for the Envoy proxy to successfully forward the http requests to the KV server.
    - Envoy dependency checks
    - Test Envoy proxy with hello world grpc server
4. Otel collector checks. Otel collector is not required for the KV server to run and process http
   and grpc requests. All checks must be passed in this step for the KV server to export metrics and
   consented logs to AWS Cloudwatch
5. Run KV server outside of enclave to check if server can start and process grpc and http requests.
   If all checks in this step are passed, it means that the server functionalities work as expected.

If all the checks besides the enclave server health check are passed, it means something else might
be wrong to prevent the server running properly inside enclave. Adtechs will need to check the
infrastructure setup on their end and provide relevant information for further assistance. Adtechs
can also run
[CPIO validator](https://github.com/privacysandbox/data-plane-shared-libraries/blob/main/docs/cpio/validator.md)
to further troubleshoot specific configurations like parameter fetching and DNS config etc.

## Build the tool

From the KV server repo root, run the build command

```shell
builders/tools/bazel-debian run //tools/server_diagnostic:copy_to_dist
```

The build command will generate the following binaries:

1. The diagnostic_cli go binary
    - dist/tools/server_diagnostic/diagnostic_cli.
2. The diagnostic tool box docker image. The tool box contains the hello world grpc server and
   grpcurl that are required by the diagnostic tool to perform checks
    - amd64:dist/tools/server_diagnostic/amd64/diagnostic_tool_box_docker_image_amd64.tar
    - arm64:dist/tools/server_diagnostic/arm64/diagnostic_tool_box_docker_image_arm64.tar

## Deploy the tool binaries to AWS EC2

The tool will need to be deployed and run on the same EC2 machine where the KV server enclave is
running. Follow the similar steps described in this
[doc](/docs/developing_the_server.md#develop-and-run-the-server-inside-aws-enclave) to scp the tool
binaries to EC2 ssh instance then to the server's EC2 instance.

Here are the steps and example commands (assume EC2 instance is running AMD architecture):

1.Send public key to EC2 ssh machine to establish connection (Skip this if there is no EC2 ssh
instance setup)

```shell
# Send public key to the EC2 ssh instance
aws ec2-instance-connect send-ssh-public-key --instance-id <EC2 ssh instance id> --availability-zone <instance availability zone> --instance-os-user ec2-user --ssh-public-key file://my_key.pub --region <instance region>
```

2.Copy diagnostic tool binaries to EC2 ssh instance (Skip this if there is no EC2 ssh instance
setup)

```shell
# The EC2_ADDR for scp'ing from the public internet to ssh instance is the Public IPv4 DNS, e.g., ec2-3-81-186-232.compute-1.amazonaws.com
# Copy the diagnostic_cli to the EC2 ssh instance /home/ec2-user directory
scp -o "IdentitiesOnly=yes" -i ./my_key dist/tools/server_diagnostic/diagnostic_cli ec2-user@{EC2_ADDR}:/home/ec2-user
# Copy the diagnostic tool box docker container to the EC2 ssh instance /home/ec2-user directory
scp -o "IdentitiesOnly=yes" -i ./my_key dist/tools/server_diagnostic/amd64/diagnostic_tool_box_docker_image_amd64.tar ec2-user@{EC2_ADDR}:/home/ec2-user
```

3.From EC2 ssh instance, send public key to server's EC2 machine to establish connection

```shell
aws ec2-instance-connect send-ssh-public-key --instance-id <EC2 instance id> --availability-zone <instance availability zone> --instance-os-user ec2-user --ssh-public-key file://my_key.pub --region <instance region>
```

4.Copy diagnostic tool binaries from EC2 ssh instance to server's EC2 instance

```shell
# The EC2_ADDR for scp'ing from the ssh instance is the Private IP DNS name e.g., ip-10-0-226-225.ec2.internal
# Copy the diagnostic_cli to the EC2 instance /home/ec2-user directory
scp -o "IdentitiesOnly=yes" -i ./my_key /home/ec2-user/diagnostic_cli ec2-user@{EC2_ADDR}:/home/ec2-user
# Copy the diagnostic tool box docker container to the EC2 instance /home/ec2-user directory
scp -o "IdentitiesOnly=yes" -i ./my_key /home/ec2-user/diagnostic_tool_box_docker_image_amd64.tar ec2-user@{EC2_ADDR}:/home/ec2-user
```

## Run the diagnostic tool

Help command to see all available flags and their default values

```shell
./diagnostic_cli -help
```

Example command to run the diagnostic tool

```shell
./diagnostic_cli -environment=<KV server deployment environment> -region=us-east-1
```

## Example summary report

```txt
-------------------------------------SUMMARY--------------------------------------------

-----------------------------------INPUT FLAGS------------------------------------------

environment: demo
envoy_config: /etc/envoy/envoy.yaml
envoy_protoset: /etc/envoy/query_api_descriptor_set.pb
help: false
otel_collector_config: /opt/aws/aws-otel-collector/etc/otel_collector_config.yaml
otel_collector_ctl: /opt/aws/aws-otel-collector/bin/aws-otel-collector-ctl
region: us-east-1
server_docker_image: /home/ec2-user/server_docker_image.tar
server_enclave_image: /opt/privacysandbox/server_enclave_image.eif
server_grpc_port: 50051
server_http_port: 51052
tool_output_dir: ./tools/output
toolbox_docker_image: ./diagnostic_tool_box_docker_image_amd64.tar
verbosity: 1
vsock_proxy_file: /opt/privacysandbox/proxy
----------------------------ENCLAVE SERVER HEALTH CHECKS--------------------------------
Are all checks passed? false

OK. Server binary exists.
FAILED. Test server was running.
FAILED. Test grpc requests working.
FAILED. Test http requests working.

----------------------------SYSTEM DEPENDENCY CHECKS------------------------------------
All checks must be passed for server to start and process grpc requests

OK. /opt/privacysandbox/proxy exists.
OK. vsockproxy.service is enabled.
OK. /etc/resolv.conf exists.
OK. VPC Ipv4 Cidr matches nameserver in /etc/resolv.conf.

-------------------------------ENVOY PROXY CHECKS---------------------------------------
All checks must be passed for server to process http requests

OK. /etc/envoy/query_api_descriptor_set.pb exists.
OK. /etc/envoy/envoy.yaml exists.
OK. Envoy is running.
OK. Envoy is listening the http port 51052.
OK. Test Envoy with hello world grpc server.
Hello world server log is located in ./tools/output/hello_world_server.log

------------------------------OTEL COLLECTOR CHECKS--------------------------------------
All checks must be passed for server to export metrics

OK. /opt/aws/aws-otel-collector/bin/aws-otel-collector-ctl exists.
OK. /opt/aws/aws-otel-collector/etc/otel_collector_config.yaml exists.
OK. aws-otel-collector.service is enabled.

------------------------OUTSIDE OF ENCLAVE SERVER HEALTH CHECKS--------------------------
Are all checks passed? true

OK. Server binary exists.
OK. Test server was running.
OK. Test grpc requests working.
OK. Test http requests working.
Test server log is located in ./tools/output/kv_server_docker.log
```
