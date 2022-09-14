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

module "kv_server" {
  source = "../modules/kv_server"

  # Variables related to environment configuration.
  environment = var.environment
  region      = var.region

  # Variables related to network, dns and certs configuration.
  vpc_cidr_block      = var.vpc_cidr_block
  root_domain         = var.root_domain
  root_domain_zone_id = var.root_domain_zone_id
  certificate_arn     = var.certificate_arn

  # Variables related to EC2 instances.
  instance_type   = var.instance_type
  instance_ami_id = var.instance_ami_id

  # Variables related to server configuration.
  mode               = var.mode
  server_port        = var.server_port
  enclave_cpu_count  = var.enclave_cpu_count
  enclave_memory_mib = var.enclave_memory_mib

  # Variables related to autoscaling and load balancing.
  autoscaling_desired_capacity = var.autoscaling_desired_capacity
  autoscaling_max_size         = var.autoscaling_max_size
  autoscaling_min_size         = var.autoscaling_min_size

  # Variables related to data storage and cleanup.
  s3_delta_file_bucket_name = var.s3_delta_file_bucket_name
  sqs_cleanup_image_uri     = var.sqs_cleanup_image_uri
  sqs_cleanup_schedule      = var.sqs_cleanup_schedule
  sqs_queue_timeout_secs    = var.sqs_queue_timeout_secs

  # Variables related to AWS backend services.
  vpc_gateway_endpoint_services   = var.vpc_gateway_endpoint_services
  vpc_interface_endpoint_services = var.vpc_interface_endpoint_services

  # Variables related to health checks.
  healthcheck_healthy_threshold   = var.healthcheck_healthy_threshold
  healthcheck_interval_sec        = var.healthcheck_interval_sec
  healthcheck_unhealthy_threshold = var.healthcheck_unhealthy_threshold

  # Variables related to SSH
  ssh_source_cidr_blocks = var.ssh_source_cidr_blocks
}

output "kv_server_url" {
  value = module.kv_server.kv_server_url
}
