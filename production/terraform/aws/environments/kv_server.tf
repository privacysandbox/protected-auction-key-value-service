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
  vpc_cidr_block           = var.vpc_cidr_block
  root_domain              = var.root_domain
  root_domain_zone_id      = var.root_domain_zone_id
  certificate_arn          = var.certificate_arn
  use_existing_vpc         = var.use_existing_vpc
  existing_vpc_operator    = var.existing_vpc_operator
  existing_vpc_environment = var.existing_vpc_environment
  enable_external_traffic  = var.enable_external_traffic
  with_existing_kv         = var.with_existing_kv

  # Variables related to EC2 instances.
  instance_type   = var.instance_type
  instance_ami_id = var.instance_ami_id

  # Variables related to server configuration.
  route_v1_requests_to_v2   = var.route_v1_requests_to_v2
  server_port               = var.server_port
  enclave_cpu_count         = var.enclave_cpu_count
  enclave_memory_mib        = var.enclave_memory_mib
  enclave_enable_debug_mode = var.enclave_enable_debug_mode
  run_server_outside_tee    = var.run_server_outside_tee
  add_missing_keys_v1       = var.add_missing_keys_v1

  # Variables related to autoscaling and load balancing.
  autoscaling_desired_capacity = var.autoscaling_desired_capacity
  autoscaling_max_size         = var.autoscaling_max_size
  autoscaling_min_size         = var.autoscaling_min_size
  http_api_paths               = var.http_api_paths

  # Variables related to data storage and cleanup.
  s3_delta_file_bucket_name    = var.s3_delta_file_bucket_name
  sqs_cleanup_image_uri        = var.sqs_cleanup_image_uri
  sqs_cleanup_schedule         = var.sqs_cleanup_schedule
  sqs_queue_timeout_secs       = var.sqs_queue_timeout_secs
  backup_poll_frequency_secs   = var.backup_poll_frequency_secs
  realtime_updater_num_threads = var.realtime_updater_num_threads

  # Variables related to health checks.
  healthcheck_healthy_threshold   = var.healthcheck_healthy_threshold
  healthcheck_interval_sec        = var.healthcheck_interval_sec
  healthcheck_unhealthy_threshold = var.healthcheck_unhealthy_threshold
  healthcheck_timeout_sec         = var.healthcheck_timeout_sec
  healthcheck_grace_period_sec    = var.healthcheck_grace_period_sec

  # Variables related to SSH
  ssh_source_cidr_blocks = var.ssh_source_cidr_blocks

  # Variables related to metrics.
  use_external_metrics_collector_endpoint = var.use_external_metrics_collector_endpoint
  metrics_collector_endpoint              = var.metrics_collector_endpoint
  metrics_export_interval_millis          = var.metrics_export_interval_millis
  metrics_export_timeout_millis           = var.metrics_export_timeout_millis
  telemetry_config                        = var.telemetry_config

  # Variables related to prometheus service
  prometheus_service_region = var.prometheus_service_region
  prometheus_workspace_id   = var.prometheus_workspace_id

  # Variables related to data loading.
  data_loading_num_threads           = var.data_loading_num_threads
  s3client_max_connections           = var.s3client_max_connections
  s3client_max_range_bytes           = var.s3client_max_range_bytes
  data_loading_file_format           = var.data_loading_file_format
  data_loading_blob_prefix_allowlist = var.data_loading_blob_prefix_allowlist

  # Variables related to sharding.
  num_shards             = var.num_shards
  use_sharding_key_regex = var.use_sharding_key_regex
  sharding_key_regex     = var.sharding_key_regex

  # Variables related to UDF execution.
  udf_num_workers           = var.udf_num_workers
  udf_timeout_millis        = var.udf_timeout_millis
  udf_update_timeout_millis = var.udf_update_timeout_millis
  udf_min_log_level         = var.udf_min_log_level

  # Variables related to coordinators
  use_real_coordinators                      = var.use_real_coordinators
  primary_coordinator_account_identity       = var.primary_coordinator_account_identity
  secondary_coordinator_account_identity     = var.secondary_coordinator_account_identity
  primary_coordinator_private_key_endpoint   = var.primary_coordinator_private_key_endpoint
  secondary_coordinator_private_key_endpoint = var.secondary_coordinator_private_key_endpoint
  primary_coordinator_region                 = var.primary_coordinator_region
  secondary_coordinator_region               = var.secondary_coordinator_region
  public_key_endpoint                        = var.public_key_endpoint

  # Variables related to logging
  logging_verbosity_level                      = var.logging_verbosity_level
  logging_verbosity_backup_poll_frequency_secs = var.logging_verbosity_backup_poll_frequency_secs
  enable_otel_logger                           = var.enable_otel_logger
  consented_debug_token                        = var.consented_debug_token
  enable_consented_log                         = var.enable_consented_log
}

output "kv_server_url" {
  value = module.kv_server.kv_server_url
}
