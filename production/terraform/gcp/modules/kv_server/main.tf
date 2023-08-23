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
  service           = "kv-server"
  kv_server_address = "xds:///kv-service-host"
}

module "networking" {
  source      = "../../services/networking"
  service     = local.service
  environment = var.environment
  regions     = var.regions
}

module "security" {
  source      = "../../services/security"
  service     = local.service
  environment = var.environment
  network_id  = module.networking.network_id
  subnets     = module.networking.subnets
}

module "autoscaling" {
  source                                = "../../services/autoscaling"
  gcp_image_tag                         = var.environment
  gcp_image_repo                        = var.gcp_image_repo
  service                               = local.service
  environment                           = var.environment
  vpc_id                                = module.networking.network_id
  subnets                               = module.networking.subnets
  service_account_email                 = var.service_account_email
  service_port                          = var.kv_service_port
  min_replicas_per_service_region       = var.min_replicas_per_service_region
  max_replicas_per_service_region       = var.max_replicas_per_service_region
  use_confidential_space_debug_image    = var.use_confidential_space_debug_image
  vm_startup_delay_seconds              = var.vm_startup_delay_seconds
  machine_type                          = var.machine_type
  instance_template_waits_for_instances = var.instance_template_waits_for_instances
  cpu_utilization_percent               = var.cpu_utilization_percent
}

module "service_mesh" {
  source            = "../../services/service_mesh"
  service           = local.service
  environment       = var.environment
  service_port      = var.kv_service_port
  kv_server_address = local.kv_server_address
  project_id        = var.project_id
  instance_groups   = module.autoscaling.kv_server_instance_groups
}

module "parameter" {
  source        = "../../services/parameter"
  service       = local.service
  environment   = var.environment
  runtime_flags = var.runtime_flags
}
