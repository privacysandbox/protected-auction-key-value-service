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

# Variables common to service components.
variable "region" {
  description = "AWS region to deploy to."
  type        = string
}

# Variables for network services.
variable "environment" {
  description = "Assigned environment name to group related resources."
  type        = string
}

variable "s3_delta_file_bucket_name" {
  description = "Globally unique name for S3 delta file bucket."
  type        = string
}

variable "vpc_cidr_block" {
  description = "CIDR range for the VPC where KV server will be deployed."
  type        = string
}

variable "server_port" {
  description = "Port on which the enclave listens for TCP connections."
  type        = number
}

variable "certificate_arn" {
  description = "ARN for an ACM managed certificate."
  type        = string
}

variable "root_domain" {
  description = "Root domain for APIs."
  type        = string
}

variable "root_domain_zone_id" {
  description = "Zone id for the root domain."
  type        = string
}

variable "instance_ami_id" {
  type = string
}

variable "instance_type" {
  type = string
}

variable "autoscaling_desired_capacity" {
  type = number
}

variable "autoscaling_max_size" {
  type = number
}

variable "autoscaling_min_size" {
  type = number
}

variable "autoscaling_wait_for_capacity_timeout" {
  type    = string
  default = "10m"
}

variable "sqs_cleanup_image_uri" {
  description = "Uri for the SQS cleanup image."
  type        = string
}

variable "sqs_cleanup_schedule" {
  # https://docs.aws.amazon.com/AmazonCloudWatch/latest/events/ScheduledEvents.html
  description = "Schedule for cleaning up SQS queues, e.g., rate(5 minutes)."
  type        = string
}

variable "sqs_queue_timeout_secs" {
  description = "Clean up queues not updated within the timeout period."
  type        = number
}

variable "enclave_memory_mib" {
  description = "Amount of memory to allocate to the enclave."
  type        = number
}

variable "enclave_cpu_count" {
  description = "The number of vcpus to allocate to the enclave."
  type        = number
}

variable "enclave_enable_debug_mode" {
  description = "If you enable debug mode, you can view the enclave's console in read-only mode using the nitro-cli console command. Enclaves booted in debug mode generate attestation documents with PCRs that are made up entirely of zeros (000000000000000000000000000000000000000000000000). More info: https://docs.aws.amazon.com/enclaves/latest/user/cmd-nitro-run-enclave.html"
  type        = bool
}

variable "healthcheck_interval_sec" {
  description = "Amount of time between health check intervals in seconds."
  type        = number
}

variable "healthcheck_healthy_threshold" {
  description = "Consecutive health check successes required to be considered healthy."
  type        = number
}

variable "healthcheck_unhealthy_threshold" {
  description = "Consecutive health check failures required to be considered unhealthy."
  type        = number
}

variable "ssh_source_cidr_blocks" {
  description = "Source ips allowed to send ssh traffic to the ssh instance."
  type        = set(string)
}

variable "backup_poll_frequency_secs" {
  description = "Backup poll frequency for delta file notifier in seconds."
  type        = number
}

variable "use_external_metrics_collector_endpoint" {
  description = "Whether to connect external metrics collector endpoint"
  type        = bool
}

variable "metrics_collector_endpoint" {
  description = "Metrics collector endpoint."
  type        = string
}

variable "metrics_export_interval_millis" {
  description = "Export interval for metrics in milliseconds."
  type        = number
}

variable "metrics_export_timeout_millis" {
  description = "Export timeout for metrics in milliseconds."
  type        = number
}

variable "realtime_updater_num_threads" {
  description = "The number of threads for the realtime updater."
  type        = number
}

variable "prometheus_service_region" {
  description = "Region where prometheus service runs that other services deployed by this file should interact with."
  type        = string
}

variable "prometheus_workspace_id" {
  description = "Workspace ID created for this environment."
  default     = ""
  type        = string
}

variable "data_loading_num_threads" {
  description = "Number of parallel threads for reading and loading data files."
  type        = number
}

variable "s3client_max_connections" {
  description = "S3Client max connections for reading data files."
  type        = number
}

variable "s3client_max_range_bytes" {
  description = "S3Client max range bytes for reading data files."
  type        = number
}

variable "num_shards" {
  description = "Number of shards."
  type        = number
}

variable "udf_num_workers" {
  description = "Number of workers for UDF execution."
  type        = number
}

variable "route_v1_requests_to_v2" {
  description = "Whether to route V1 requests through V2."
  type        = bool
}

variable "use_real_coordinators" {
  description = "Will use real coordinators. `enclave_enable_debug_mode` should be set to `false` if the attestation check is enabled for coordinators. Attestation check is enabled on all production instances, and might be disabled for testing purposes only on staging/dev environments."
  type        = bool
}

variable "primary_coordinator_account_identity" {
  description = "Account identity for the primary coordinator."
  type        = string
}

variable "secondary_coordinator_account_identity" {
  description = "Account identity for the secondary coordinator."
  type        = string
}

variable "data_loading_file_format" {
  description = "Data file format for blob storage and realtime updates. See /public/constants.h for possible values."
  type        = string
}

variable "logging_verbosity_level" {
  description = "Logging verbosity."
  type        = number
}
