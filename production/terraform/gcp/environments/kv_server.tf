# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

provider "google" {
  project = var.project_id
  default_labels = {
    environment = var.environment
  }
}

provider "google-beta" {
  project = var.project_id
  default_labels = {
    environment = var.environment
  }
}

locals {
  kv_service = "kv-server"
}

module "kv_server" {
  source                                = "../modules/kv_server"
  project_id                            = var.project_id
  environment                           = var.environment
  service                               = local.kv_service
  service_account_email                 = var.service_account_email
  regions                               = var.regions
  regions_cidr_blocks                   = var.regions_cidr_blocks
  regions_use_existing_nat              = var.regions_use_existing_nat
  gcp_image_tag                         = var.gcp_image_tag
  gcp_image_repo                        = var.gcp_image_repo
  kv_service_port                       = var.kv_service_port
  envoy_port                            = var.envoy_port
  server_url                            = var.server_url
  server_dns_zone                       = var.server_dns_zone
  server_domain_ssl_certificate_id      = var.server_domain_ssl_certificate_id
  min_replicas_per_service_region       = var.min_replicas_per_service_region
  max_replicas_per_service_region       = var.max_replicas_per_service_region
  use_confidential_space_debug_image    = var.use_confidential_space_debug_image
  vm_startup_delay_seconds              = var.vm_startup_delay_seconds
  machine_type                          = var.machine_type
  instance_template_waits_for_instances = var.instance_template_waits_for_instances
  cpu_utilization_percent               = var.cpu_utilization_percent
  collector_service_name                = var.collector_service_name
  collector_machine_type                = var.collector_machine_type
  collector_service_port                = var.collector_service_port
  collector_startup_script_path         = var.collector_startup_script_path
  collector_domain_name                 = var.collector_domain_name
  collector_dns_zone                    = var.collector_dns_zone
  data_bucket_id                        = var.data_bucket_id
  tee_impersonate_service_accounts      = var.tee_impersonate_service_accounts
  use_existing_vpc                      = var.use_existing_vpc
  existing_vpc_id                       = var.existing_vpc_id
  num_shards                            = var.num_shards
  use_existing_service_mesh             = var.use_existing_service_mesh
  existing_service_mesh                 = var.existing_service_mesh
  service_mesh_address                  = var.service_mesh_address
  enable_external_traffic               = var.enable_external_traffic

  parameters = {
    data-bucket-id                               = var.data_bucket_id
    launch-hook                                  = "${local.kv_service}-${var.environment}-launch-hook"
    use-external-metrics-collector-endpoint      = var.use_external_metrics_collector_endpoint
    metrics-collector-endpoint                   = "${var.environment}-${var.collector_service_name}.${var.collector_domain_name}:${var.collector_service_port}"
    metrics-export-interval-millis               = var.metrics_export_interval_millis
    metrics-export-timeout-millis                = var.metrics_export_timeout_millis
    backup-poll-frequency-secs                   = var.backup_poll_frequency_secs
    realtime-updater-num-threads                 = var.realtime_updater_num_threads
    data-loading-num-threads                     = var.data_loading_num_threads
    num-shards                                   = var.num_shards
    udf-num-workers                              = var.udf_num_workers
    udf-timeout-millis                           = var.udf_timeout_millis
    udf-update-timeout-millis                    = var.udf_update_timeout_millis
    udf-min-log-level                            = var.udf_min_log_level
    udf-enable-stacktrace                        = var.udf_enable_stacktrace
    route-v1-to-v2                               = var.route_v1_to_v2
    add-missing-keys-v1                          = var.add_missing_keys_v1
    add-chaff-sharding-clusters                  = var.add_chaff_sharding_clusters
    use-real-coordinators                        = var.use_real_coordinators
    environment                                  = var.environment
    project-id                                   = var.project_id
    primary-key-service-cloud-function-url       = var.primary_key_service_cloud_function_url
    primary-workload-identity-pool-provider      = var.primary_workload_identity_pool_provider
    secondary-key-service-cloud-function-url     = var.secondary_key_service_cloud_function_url
    secondary-workload-identity-pool-provider    = var.secondary_workload_identity_pool_provider
    primary-coordinator-account-identity         = var.primary_coordinator_account_identity
    secondary-coordinator-account-identity       = var.secondary_coordinator_account_identity
    primary-coordinator-private-key-endpoint     = var.primary_coordinator_private_key_endpoint
    primary-coordinator-region                   = var.primary_coordinator_region
    secondary-coordinator-private-key-endpoint   = var.secondary_coordinator_private_key_endpoint
    secondary-coordinator-region                 = var.secondary_coordinator_region
    public-key-endpoint                          = var.public_key_endpoint
    logging-verbosity-backup-poll-frequency-secs = var.logging_verbosity_backup_poll_frequency_secs
    logging-verbosity-level                      = var.logging_verbosity_level
    use-sharding-key-regex                       = var.use_sharding_key_regex
    sharding-key-regex                           = var.sharding_key_regex
    tls-key                                      = var.tls_key
    tls-cert                                     = var.tls_cert
    enable-otel-logger                           = var.enable_otel_logger
    enable-external-traffic                      = var.enable_external_traffic
    telemetry-config                             = var.telemetry_config
    data-loading-blob-prefix-allowlist           = var.data_loading_blob_prefix_allowlist
    consented-debug-token                        = var.consented_debug_token
    enable-consented-log                         = var.enable_consented_log
  }
}
