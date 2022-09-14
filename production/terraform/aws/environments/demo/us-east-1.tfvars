/**
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

# Variables related to environment configuration.
environment = "demo"
region      = "us-east-1"

# Variables related to network, dns and certs configuration.
vpc_cidr_block      = "10.0.0.0/16"
root_domain         = "demo-server.com"
root_domain_zone_id = "zone-id"
certificate_arn     = "cert-arn"

# Variables related to EC2 instances.
instance_type   = "m5.xlarge"
instance_ami_id = "ami-0000000"

# Variables related to server configuration.
mode               = "DSP"
server_port        = 51052
enclave_cpu_count  = 2
enclave_memory_mib = 3072

# Variables related to autoscaling and load balancing.
autoscaling_desired_capacity = 4
autoscaling_max_size         = 6
autoscaling_min_size         = 4

# Variables related to data storage and cleanup.
s3_delta_file_bucket_name = "globally-unique-bucket"
sqs_cleanup_image_uri     = "demo:latest"
sqs_cleanup_schedule      = "rate(6 hours)"
sqs_queue_timeout_secs    = 86400

# Variables related to AWS backend services.
vpc_gateway_endpoint_services = [
"s3"]
vpc_interface_endpoint_services = [
  "ec2",
  "ssm",
  "sns",
  "sqs",
  "autoscaling"
]

# Variables related to health checks.
healthcheck_healthy_threshold   = 3
healthcheck_interval_sec        = 30
healthcheck_unhealthy_threshold = 3

# Variables related to SSH.
ssh_source_cidr_blocks = ["0.0.0.0/0"]
