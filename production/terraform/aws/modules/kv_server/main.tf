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

locals {
  service = "kv-server"
}

module "iam_roles" {
  source                   = "../../services/iam_roles"
  environment              = var.environment
  service                  = local.service
  use_existing_vpc         = var.use_existing_vpc
  existing_vpc_operator    = var.existing_vpc_operator
  existing_vpc_environment = var.existing_vpc_environment
}

module "iam_groups" {
  source                   = "../../services/iam_groups"
  environment              = var.environment
  service                  = local.service
  use_existing_vpc         = var.use_existing_vpc
  existing_vpc_operator    = var.existing_vpc_operator
  existing_vpc_environment = var.existing_vpc_environment
}

module "data_storage" {
  source                         = "../../services/data_storage"
  environment                    = var.environment
  service                        = local.service
  s3_delta_file_bucket_name      = var.s3_delta_file_bucket_name
  bucket_notification_dependency = [module.sqs_cleanup.allow_sqs_cleanup_execution_as_dependency]
}

module "sqs_cleanup" {
  source                     = "../../services/sqs_cleanup"
  environment                = var.environment
  service                    = local.service
  sqs_cleanup_image_uri      = var.sqs_cleanup_image_uri
  lambda_role_arn            = module.iam_roles.lambda_role_arn
  sqs_cleanup_schedule       = var.sqs_cleanup_schedule
  sns_data_updates_topic_arn = module.data_storage.sns_data_updates_topic_arn
  sqs_queue_timeout_secs     = var.sqs_queue_timeout_secs
  sns_realtime_topic_arn     = module.data_storage.sns_realtime_topic_arn
}

module "networking" {
  source                   = "../../services/networking"
  service                  = local.service
  environment              = var.environment
  vpc_cidr_block           = var.vpc_cidr_block
  use_existing_vpc         = var.use_existing_vpc
  existing_vpc_operator    = var.existing_vpc_operator
  existing_vpc_environment = var.existing_vpc_environment
}

module "security_groups" {
  source                   = "../../services/security_groups"
  environment              = var.environment
  service                  = local.service
  vpc_id                   = module.networking.vpc_id
  use_existing_vpc         = var.use_existing_vpc
  existing_vpc_operator    = var.existing_vpc_operator
  existing_vpc_environment = var.existing_vpc_environment
}

module "backend_services" {
  source                       = "../../services/backend_services"
  region                       = var.region
  environment                  = var.environment
  service                      = local.service
  vpc_endpoint_route_table_ids = module.networking.private_route_table_ids
  vpc_endpoint_sg_id           = module.security_groups.vpc_endpoint_security_group_id
  vpc_endpoint_subnet_ids      = module.networking.private_subnet_ids
  vpc_id                       = module.networking.vpc_id
  server_instance_role_arn     = module.iam_roles.instance_role_arn
  ssh_instance_role_arn        = module.iam_roles.ssh_instance_role_arn
  prometheus_service_region    = var.prometheus_service_region
  use_existing_vpc             = var.use_existing_vpc
  existing_vpc_operator        = var.existing_vpc_operator
  existing_vpc_environment     = var.existing_vpc_environment
}

module "telemetry" {
  source                    = "../../services/telemetry"
  environment               = var.environment
  service                   = local.service
  region                    = var.region
  prometheus_service_region = var.prometheus_service_region
}

module "mesh_service" {
  count                           = var.use_existing_vpc ? 1 : 0
  source                          = "../../services/mesh_service"
  environment                     = var.environment
  service                         = local.service
  root_domain                     = var.root_domain
  service_port                    = 50051
  server_instance_role_name       = module.iam_roles.instance_role_name
  root_domain_zone_id             = var.root_domain_zone_id
  existing_vpc_operator           = var.existing_vpc_operator
  existing_vpc_environment        = var.existing_vpc_environment
  healthcheck_interval_sec        = var.healthcheck_interval_sec
  healthcheck_timeout_sec         = var.healthcheck_timeout_sec
  healthcheck_healthy_threshold   = var.healthcheck_healthy_threshold
  healthcheck_unhealthy_threshold = var.healthcheck_unhealthy_threshold
}

module "load_balancing" {
  count                           = var.enable_external_traffic ? 1 : 0
  source                          = "../../services/load_balancing"
  environment                     = var.environment
  service                         = local.service
  certificate_arn                 = var.certificate_arn
  elb_subnet_ids                  = module.networking.public_subnet_ids
  server_port                     = var.server_port
  vpc_id                          = module.networking.vpc_id
  elb_security_group_id           = module.security_groups.elb_security_group_id
  root_domain                     = var.root_domain
  root_domain_zone_id             = var.root_domain_zone_id
  healthcheck_healthy_threshold   = var.healthcheck_healthy_threshold
  healthcheck_interval_sec        = var.healthcheck_interval_sec
  healthcheck_unhealthy_threshold = var.healthcheck_unhealthy_threshold
  http_api_paths                  = var.http_api_paths
}

module "autoscaling" {
  count                           = var.num_shards
  source                          = "../../services/autoscaling"
  environment                     = var.environment
  region                          = var.region
  service                         = local.service
  autoscaling_subnet_ids          = module.networking.private_subnet_ids
  instance_ami_id                 = var.instance_ami_id
  instance_security_group_id      = module.security_groups.instance_security_group_id
  instance_type                   = var.instance_type
  target_group_arns               = var.enable_external_traffic ? module.load_balancing[0].target_group_arns : []
  autoscaling_desired_capacity    = var.autoscaling_desired_capacity
  autoscaling_max_size            = var.autoscaling_max_size
  autoscaling_min_size            = var.autoscaling_min_size
  wait_for_capacity_timeout       = var.autoscaling_wait_for_capacity_timeout
  instance_profile_arn            = module.iam_roles.instance_profile_arn
  enclave_cpu_count               = var.enclave_cpu_count
  enclave_memory_mib              = var.enclave_memory_mib
  enclave_enable_debug_mode       = var.enclave_enable_debug_mode
  server_port                     = var.server_port
  launch_hook_name                = module.parameter.launch_hook_parameter_value
  depends_on                      = [module.iam_role_policies.instance_role_policy_attachment]
  prometheus_service_region       = var.prometheus_service_region
  prometheus_workspace_id         = var.prometheus_workspace_id != "" ? var.prometheus_workspace_id : module.telemetry.prometheus_workspace_id
  shard_num                       = count.index
  run_server_outside_tee          = var.run_server_outside_tee
  cloud_map_service_id            = var.use_existing_vpc ? module.mesh_service[0].cloud_map_service_id : ""
  app_mesh_name                   = var.use_existing_vpc ? module.mesh_service[0].app_mesh_name : ""
  virtual_node_name               = var.use_existing_vpc ? module.mesh_service[0].virtual_node_name : ""
  healthcheck_interval_sec        = var.healthcheck_interval_sec
  healthcheck_timeout_sec         = var.healthcheck_timeout_sec
  healthcheck_healthy_threshold   = var.healthcheck_healthy_threshold
  healthcheck_unhealthy_threshold = var.healthcheck_unhealthy_threshold
  healthcheck_grace_period_sec    = var.healthcheck_grace_period_sec
}

module "ssh" {
  source                  = "../../services/ssh"
  environment             = var.environment
  instance_sg_id          = module.security_groups.ssh_security_group_id
  service                 = local.service
  ssh_instance_subnet_ids = module.networking.public_subnet_ids
  instance_profile_name   = module.iam_roles.ssh_instance_profile_name
}

module "parameter" {
  source                                                     = "../../services/parameter"
  service                                                    = local.service
  environment                                                = var.environment
  s3_bucket_parameter_value                                  = module.data_storage.s3_data_bucket_id
  bucket_update_sns_arn_parameter_value                      = module.data_storage.sns_data_updates_topic_arn
  realtime_sns_arn_parameter_value                           = module.data_storage.sns_realtime_topic_arn
  backup_poll_frequency_secs_parameter_value                 = var.backup_poll_frequency_secs
  use_external_metrics_collector_endpoint                    = var.use_external_metrics_collector_endpoint
  metrics_collector_endpoint                                 = var.metrics_collector_endpoint
  metrics_export_interval_millis_parameter_value             = var.metrics_export_interval_millis
  metrics_export_timeout_millis_parameter_value              = var.metrics_export_timeout_millis
  telemetry_config                                           = var.telemetry_config
  realtime_updater_num_threads_parameter_value               = var.realtime_updater_num_threads
  data_loading_num_threads_parameter_value                   = var.data_loading_num_threads
  s3client_max_connections_parameter_value                   = var.s3client_max_connections
  s3client_max_range_bytes_parameter_value                   = var.s3client_max_range_bytes
  num_shards_parameter_value                                 = var.num_shards
  udf_num_workers_parameter_value                            = var.udf_num_workers
  udf_timeout_millis_parameter_value                         = var.udf_timeout_millis
  udf_update_timeout_millis_parameter_value                  = var.udf_update_timeout_millis
  udf_min_log_level_parameter_value                          = var.udf_min_log_level
  route_v1_requests_to_v2_parameter_value                    = var.route_v1_requests_to_v2
  add_missing_keys_v1_parameter_value                        = var.add_missing_keys_v1
  use_real_coordinators_parameter_value                      = var.use_real_coordinators
  primary_coordinator_account_identity_parameter_value       = var.primary_coordinator_account_identity
  secondary_coordinator_account_identity_parameter_value     = var.secondary_coordinator_account_identity
  primary_coordinator_private_key_endpoint_parameter_value   = var.primary_coordinator_private_key_endpoint
  secondary_coordinator_private_key_endpoint_parameter_value = var.secondary_coordinator_private_key_endpoint
  primary_coordinator_region_parameter_value                 = var.primary_coordinator_region
  secondary_coordinator_region_parameter_value               = var.secondary_coordinator_region
  public_key_endpoint_parameter_value                        = var.public_key_endpoint
  consented_debug_token_parameter_value                      = var.consented_debug_token
  enable_consented_log_parameter_value                       = var.enable_consented_log


  data_loading_file_format_parameter_value = var.data_loading_file_format
  logging_verbosity_level_parameter_value  = var.logging_verbosity_level
  use_sharding_key_regex_parameter_value   = var.use_sharding_key_regex
  sharding_key_regex_parameter_value       = var.sharding_key_regex
  enable_otel_logger_parameter_value       = var.enable_otel_logger
  data_loading_blob_prefix_allowlist       = var.data_loading_blob_prefix_allowlist
}

module "security_group_rules" {
  source                            = "../../services/security_group_rules"
  region                            = var.region
  service                           = local.service
  environment                       = var.environment
  server_instance_port              = var.server_port
  vpc_id                            = module.networking.vpc_id
  elb_security_group_id             = module.security_groups.elb_security_group_id
  instances_security_group_id       = module.security_groups.instance_security_group_id
  ssh_security_group_id             = module.security_groups.ssh_security_group_id
  vpce_security_group_id            = module.security_groups.vpc_endpoint_security_group_id
  gateway_endpoints_prefix_list_ids = module.backend_services.gateway_endpoints_prefix_list_ids
  ssh_source_cidr_blocks            = var.ssh_source_cidr_blocks
  use_existing_vpc                  = var.use_existing_vpc
}

module "iam_role_policies" {
  source                       = "../../services/iam_role_policies"
  service                      = local.service
  environment                  = var.environment
  server_instance_role_name    = module.iam_roles.instance_role_name
  sqs_cleanup_lambda_role_name = module.iam_roles.lambda_role_name
  s3_delta_file_bucket_arn     = module.data_storage.s3_data_bucket_arn
  sns_data_updates_topic_arn   = module.data_storage.sns_data_updates_topic_arn
  sns_realtime_topic_arn       = module.data_storage.sns_realtime_topic_arn
  ssh_instance_role_name       = module.iam_roles.ssh_instance_role_name
  server_parameter_arns = [
    module.parameter.s3_bucket_parameter_arn,
    module.parameter.bucket_update_sns_arn_parameter_arn,
    module.parameter.realtime_sns_arn_parameter_arn,
    module.parameter.launch_hook_parameter_arn,
    module.parameter.backup_poll_frequency_secs_parameter_arn,
    module.parameter.use_external_metrics_collector_endpoint_arn,
    module.parameter.metrics_export_interval_millis_parameter_arn,
    module.parameter.metrics_export_timeout_millis_parameter_arn,
    module.parameter.telemetry_config_parameter_arn,
    module.parameter.realtime_updater_num_threads_parameter_arn,
    module.parameter.data_loading_num_threads_parameter_arn,
    module.parameter.s3client_max_connections_parameter_arn,
    module.parameter.s3client_max_range_bytes_parameter_arn,
    module.parameter.num_shards_parameter_arn,
    module.parameter.udf_num_workers_parameter_arn,
    module.parameter.route_v1_requests_to_v2_parameter_arn,
    module.parameter.add_missing_keys_v1_parameter_arn,
    module.parameter.data_loading_file_format_parameter_arn,
    module.parameter.logging_verbosity_level_parameter_arn,
    module.parameter.use_real_coordinators_parameter_arn,
    module.parameter.use_sharding_key_regex_parameter_arn,
    module.parameter.udf_timeout_millis_parameter_arn,
    module.parameter.udf_update_timeout_millis_parameter_arn,
    module.parameter.udf_min_log_level_parameter_arn,
    module.parameter.enable_otel_logger_parameter_arn,
    module.parameter.enable_consented_log_parameter_arn,
  module.parameter.data_loading_blob_prefix_allowlist_parameter_arn]
  coordinator_parameter_arns = (
    var.use_real_coordinators ? [
      module.parameter.primary_coordinator_account_identity_parameter_arn,
      module.parameter.secondary_coordinator_account_identity_parameter_arn,
      module.parameter.primary_coordinator_private_key_endpoint_parameter_arn,
      module.parameter.secondary_coordinator_private_key_endpoint_parameter_arn,
      module.parameter.primary_coordinator_region_parameter_arn,
      module.parameter.secondary_coordinator_region_parameter_arn,
      module.parameter.public_key_endpoint_parameter_arn
    ] : []
  )
  metrics_collector_endpoint_arns = (
    var.use_external_metrics_collector_endpoint ? [
      module.parameter.metrics_collector_endpoint_arn
    ] : []
  )
  sharding_key_regex_arns = (
    var.use_sharding_key_regex ? [
      module.parameter.sharding_key_regex_parameter_arn
    ] : []
  )
  consented_debug_token_arns = (var.enable_consented_log ? [
    module.parameter.consented_debug_token_parameter_arn] : []
  )
}

module "iam_group_policies" {
  source               = "../../services/iam_group_policies"
  service              = local.service
  environment          = var.environment
  ssh_users_group_name = module.iam_groups.ssh_users_group_name
  ssh_instance_arn     = module.ssh.ssh_instance_arn
}


module "dashboards" {
  source      = "../../services/dashboard"
  environment = var.environment
  region      = var.region
}
