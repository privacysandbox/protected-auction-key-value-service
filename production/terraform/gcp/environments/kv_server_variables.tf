/**
 * Copyright 2023 Google LLC
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

variable "project_id" {
  description = "GCP project id."
  type        = string
}

variable "environment" {
  description = "Assigned environment name to group related resources. Also servers as gcp image tag."
  type        = string
}

variable "service_account_email" {
  description = "Email of the service account that be used by all instances."
  type        = string
}

variable "regions" {
  description = "Regions to deploy to."
  type        = set(string)
}

variable "regions_cidr_blocks" {
  description = "A set of CIDR ranges for all specified regions. The number of blocks here should correspond to the number of regions."
  type        = set(string)
}

variable "regions_use_existing_nat" {
  description = "Regions that use existing nat. No new nats will be created for regions specified here."
  type        = set(string)
}

variable "gcp_image_tag" {
  description = "Tag of the gcp docker image uploaded to the artifact registry."
  type        = string
}

variable "gcp_image_repo" {
  description = "A URL to a docker image repo containing the key-value service."
  type        = string
}

variable "kv_service_port" {
  description = "The grpc port that receives traffic destined for the key-value service."
  type        = number
}

variable "envoy_port" {
  description = "External load balancer will send traffic to this port. Envoy will forward traffic to kv_service_port. Must match envoy.yaml. Ignored if `enable_external_traffic` is false."
  type        = number
}

variable "server_url" {
  description = "Kv-serer URL. Example: kv-server-environment.example.com. Ignored if `enable_external_traffic` is false."
  type        = string
}

variable "server_dns_zone" {
  description = "Google Cloud Dns zone for Kv-serer. Ignored if `enable_external_traffic` is false."
  type        = string
}

variable "server_domain_ssl_certificate_id" {
  description = "Ssl certificate id of the Kv-server URL. Ignored if `enable_external_traffic` is false."
  type        = string
}

variable "tls_key" {
  description = "TLS key. Please specify this variable in a tfvars file (e.g., secrets.auto.tfvars) under the `environments` directory. Ignored if `enable_external_traffic` is false."
  default     = "NOT_PROVIDED"
  type        = string
}

variable "tls_cert" {
  description = "TLS cert. Please specify this variable in a tfvars file (e.g., secrets.auto.tfvars) under the `environments` directory. Ignored if `enable_external_traffic` is false."
  default     = "NOT_PROVIDED"
  type        = string
}

variable "min_replicas_per_service_region" {
  description = "Minimum amount of replicas per each service region (a single managed instance group)."
  type        = number
}

variable "max_replicas_per_service_region" {
  description = "Maximum amount of replicas per each service region (a single managed instance group)."
  type        = number
}

variable "use_confidential_space_debug_image" {
  description = "If true, use the Confidential space debug image. Else use the prod image, which does not allow SSH. The images containing the service logic will run on top of this image and have their own prod and debug builds."
  type        = bool
  default     = false
}

variable "vm_startup_delay_seconds" {
  description = "The time it takes to get a service up and responding to heartbeats (in seconds)."
  type        = number
}

variable "machine_type" {
  description = "Machine type for the key-value service. Must be compatible with confidential compute."
  type        = string
}

variable "instance_template_waits_for_instances" {
  description = "True if terraform should wait for instances before returning from instance template application. False if faster apply is desired."
  type        = bool
  default     = true
}

variable "cpu_utilization_percent" {
  description = "CPU utilization percentage across an instance group required for autoscaler to add instances."
  type        = number
}

variable "data_bucket_id" {
  type        = string
  description = "Directory to watch for files."
}

variable "metrics_export_interval_millis" {
  type        = number
  description = "Export interval for metrics in milliseconds."
}

variable "metrics_export_timeout_millis" {
  type        = number
  description = "Export timeout for metrics in milliseconds."
}

variable "backup_poll_frequency_secs" {
  type        = number
  description = "Backup poll frequency for delta file notifier in seconds."
}

variable "realtime_updater_num_threads" {
  type        = number
  description = "Amount of realtime updates threads locally."
}

variable "data_loading_num_threads" {
  type        = number
  description = "Number of parallel threads for reading and loading data files."
}

variable "num_shards" {
  type        = number
  description = "Total number of shards."
}

variable "udf_num_workers" {
  type        = number
  description = "Number of workers for UDF execution."
}

variable "udf_timeout_millis" {
  type        = number
  default     = 5000
  description = "UDF execution timeout in milliseconds."
}

variable "udf_update_timeout_millis" {
  type        = number
  default     = 30000
  description = "UDF update timeout in milliseconds."
}

variable "udf_min_log_level" {
  type        = number
  default     = 0
  description = "Minimum log level for UDFs. Info = 0, Warn = 1, Error = 2. The UDF will only attempt to log for min_log_level and above. Default is 0(info)."
}

variable "udf_enable_stacktrace" {
  description = "Whether to enable stacktraces from the UDF. The stacktraces will also be included in the V2 response."
  default     = false
  type        = bool
}

variable "route_v1_to_v2" {
  type        = bool
  description = "Whether to route V1 requests through V2."
}

variable "add_missing_keys_v1" {
  type        = bool
  description = "Add missing keys for v1."
}

variable "add_chaff_sharding_clusters" {
  type        = bool
  default     = true
  description = "Add chaff sharding clusters."
}

variable "use_real_coordinators" {
  type        = bool
  description = "Use real coordinators."
}

variable "use_external_metrics_collector_endpoint" {
  type        = bool
  description = "Whether to connect to external metrics collector endpoint"
}

variable "collector_service_name" {
  description = "Metrics collector service name"
  type        = string
}

variable "collector_service_port" {
  description = "The grpc port that receives traffic destined for the OpenTelemetry collector."
  type        = number
}

variable "collector_startup_script_path" {
  description = "Relative path from main.tf to collector service startup script."
  type        = string
  default     = "../../services/metrics_collector_autoscaling/collector_startup.sh"
}

variable "collector_domain_name" {
  description = "Google Cloud domain name for OpenTelemetry collector"
  type        = string
}

variable "collector_machine_type" {
  description = "Machine type for the collector service."
  type        = string
}

variable "collector_dns_zone" {
  description = "Google Cloud DNS zone name for collector."
  type        = string
}

variable "primary_key_service_cloud_function_url" {
  description = "Primary key service cloud function url."
  type        = string
}

variable "primary_workload_identity_pool_provider" {
  description = "Primary workload identity pool provider."
  type        = string
}

variable "secondary_key_service_cloud_function_url" {
  description = "Secondary key service cloud function url."
  type        = string
}

variable "secondary_workload_identity_pool_provider" {
  description = "Secondary workload identity pool provider."
  type        = string
}

variable "primary_coordinator_account_identity" {
  description = "Account identity for the primary coordinator."
  type        = string
}

variable "secondary_coordinator_account_identity" {
  description = "Account identity for the secondary coordinator."
  type        = string
}

variable "tee_impersonate_service_accounts" {
  type        = string
  description = "Tee can impersonate these service accounts. Necessary for coordinators."
}

variable "use_existing_vpc" {
  description = "Whether to use existing vpc network."
  type        = bool
}

variable "existing_vpc_id" {
  description = "Existing vpc id. This would only be used if use_existing_vpc is true."
  type        = string
}

variable "use_existing_service_mesh" {
  description = "Whether to use existing service mesh."
  type        = bool
}

variable "existing_service_mesh" {
  description = "Existing service mesh. This would only be used if use_existing_service_mesh is true."
  type        = string
}

variable "logging_verbosity_level" {
  description = "Logging verbosity level."
  default     = "0"
  type        = string
}

variable "logging_verbosity_backup_poll_frequency_secs" {
  description = "Backup poll frequency in seconds for the logging verbosity parameter."
  default     = 60
  type        = number
}


variable "use_sharding_key_regex" {
  description = "Use sharding key regex. This is useful if you want to use data locality feature for sharding."
  default     = false
  type        = bool
}

variable "sharding_key_regex" {
  description = "Sharding key regex."
  default     = "EMPTY_STRING"
  type        = string
}

variable "service_mesh_address" {
  description = "Service mesh address of the KV server."
  default     = "xds:///kv-service-host"
  type        = string
}

variable "enable_otel_logger" {
  description = "Whether to use otel logger."
  default     = true
  type        = bool
}

variable "enable_external_traffic" {
  description = "Whether to serve external traffic. If disabled, only internal traffic via service mesh will be served."
  default     = true
  type        = bool
}

variable "telemetry_config" {
  description = "Telemetry configuration to control whether metrics are raw or noised. Options are: mode: PROD(noised metrics), mode: EXPERIMENT(raw metrics), mode: COMPARE(both raw and noised metrics), mode: OFF(no metrics)"
  default     = "mode: PROD"
  type        = string
}

variable "data_loading_blob_prefix_allowlist" {
  description = "A comma separated list of prefixes (i.e., directories) where data is loaded from."
  default     = ","
  type        = string
}

variable "primary_coordinator_private_key_endpoint" {
  description = "Primary coordinator private key endpoint."
  type        = string
}

variable "secondary_coordinator_private_key_endpoint" {
  description = "Secondary coordinator private key endpoint."
  type        = string
}

variable "primary_coordinator_region" {
  description = "Primary coordinator region."
  type        = string
}

variable "secondary_coordinator_region" {
  description = "Secondary coordinator region."
  type        = string
}

variable "public_key_endpoint" {
  description = "Public key endpoint. Can only be overriden in non-prod mode."
  type        = string
}

variable "consented_debug_token" {
  description = "Consented debug token to enable the otel collection of consented logs. Empty token means no-op and no logs will be collected for consented requests. The token in the request's consented debug configuration needs to match this debug token to make the server treat the request as consented."
  type        = string
}

variable "enable_consented_log" {
  description = "Enable the logging of consented requests. If it is set to true, the consented debug token parameter value must not be an empty string."
  type        = bool
}
