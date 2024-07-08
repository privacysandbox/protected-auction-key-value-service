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

module "networking" {
  source                   = "../../services/networking"
  service                  = var.service
  environment              = var.environment
  regions                  = var.regions
  regions_cidr_blocks      = var.regions_cidr_blocks
  regions_use_existing_nat = var.regions_use_existing_nat
  collector_service_name   = var.collector_service_name
  use_existing_vpc         = var.use_existing_vpc
  existing_vpc_id          = var.existing_vpc_id
  enable_external_traffic  = var.enable_external_traffic
}

module "security" {
  source                 = "../../services/security"
  service                = var.service
  environment            = var.environment
  network_id             = module.networking.network_id
  subnets                = module.networking.subnets
  proxy_subnets          = module.networking.proxy_subnets
  collector_service_port = var.collector_service_port
}

module "autoscaling" {
  count                                 = var.num_shards
  source                                = "../../services/autoscaling"
  gcp_image_tag                         = var.gcp_image_tag
  gcp_image_repo                        = var.gcp_image_repo
  service                               = var.service
  environment                           = var.environment
  vpc_id                                = module.networking.network_id
  subnets                               = module.networking.subnets
  service_account_email                 = var.service_account_email
  service_port                          = var.kv_service_port
  envoy_port                            = var.envoy_port
  min_replicas_per_service_region       = var.min_replicas_per_service_region
  max_replicas_per_service_region       = var.max_replicas_per_service_region
  use_confidential_space_debug_image    = var.use_confidential_space_debug_image
  vm_startup_delay_seconds              = var.vm_startup_delay_seconds
  machine_type                          = var.machine_type
  instance_template_waits_for_instances = var.instance_template_waits_for_instances
  cpu_utilization_percent               = var.cpu_utilization_percent
  parameters                            = var.parameters
  tee_impersonate_service_accounts      = var.tee_impersonate_service_accounts
  shard_num                             = count.index
  enable_external_traffic               = var.enable_external_traffic
}

module "metrics_collector_autoscaling" {
  source                          = "../../services/metrics_collector_autoscaling"
  environment                     = var.environment
  vpc_id                          = module.networking.network_id
  subnets                         = module.networking.subnets
  service_account_email           = var.service_account_email
  vm_startup_delay_seconds        = var.vm_startup_delay_seconds
  cpu_utilization_percent         = var.cpu_utilization_percent
  collector_machine_type          = var.collector_machine_type
  collector_service_name          = var.collector_service_name
  collector_service_port          = var.collector_service_port
  collector_startup_script_path   = "${path.module}/${var.collector_startup_script_path}"
  max_replicas_per_service_region = var.max_replicas_per_service_region
}

module "metrics_collector" {
  source                    = "../../services/metrics_collector"
  environment               = var.environment
  subnets                   = module.networking.subnets
  proxy_subnets             = module.networking.proxy_subnets
  collector_instance_groups = module.metrics_collector_autoscaling.collector_instance_groups
  collector_service_name    = var.collector_service_name
  collector_service_port    = var.collector_service_port
  collector_dns_zone        = var.collector_dns_zone
  collector_domain_name     = var.collector_domain_name
}

module "service_mesh" {
  source                    = "../../services/service_mesh"
  service                   = var.service
  environment               = var.environment
  service_port              = var.kv_service_port
  kv_server_address         = var.service_mesh_address
  project_id                = var.project_id
  instance_groups           = flatten(module.autoscaling[*].kv_server_instance_groups)
  collector_forwarding_rule = module.metrics_collector.collector_forwarding_rule
  collector_tcp_proxy       = module.metrics_collector.collector_tcp_proxy
  use_existing_service_mesh = var.use_existing_service_mesh
  existing_service_mesh     = var.existing_service_mesh
  enable_external_traffic   = var.enable_external_traffic
}

module "external_load_balancing" {
  count = var.enable_external_traffic ? 1 : 0

  source                           = "../../services/external_load_balancing"
  service                          = var.service
  environment                      = var.environment
  service_port                     = var.kv_service_port
  server_url                       = var.server_url
  server_dns_zone                  = var.server_dns_zone
  server_domain_ssl_certificate_id = var.server_domain_ssl_certificate_id
  instance_groups                  = flatten(module.autoscaling[*].kv_server_instance_groups)
  internal_load_balancer           = module.service_mesh.internal_load_balancer
  grpc_route                       = module.service_mesh.grpc_route
  server_ip_address                = module.networking.server_ip_address
}

module "parameter" {
  source      = "../../services/parameter"
  service     = var.service
  environment = var.environment
  parameters  = var.parameters
}

module "realtime" {
  source      = "../../services/realtime"
  service     = var.service
  environment = var.environment
}

module "data_storage" {
  source         = "../../services/data_storage"
  service        = var.service
  environment    = var.environment
  data_bucket_id = var.data_bucket_id
}

module "metrics_dashboard" {
  source      = "../../services/dashboards"
  environment = var.environment
}
