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

variable "gcp_image_repo" {
  description = "A URL to a docker image repo containing the key-value service."
  type        = string
}

variable "kv_service_port" {
  description = "The grpc port that receives traffic destined for the frontend service."
  type        = number
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

variable "directory" {
  type        = string
  description = "Directory to watch for files."
}

variable "data_bucket_id" {
  type        = string
  description = "Directory to watch for files."
}

variable "launch_hook" {
  type        = string
  description = "Launch hook."
}

variable "realtime_directory" {
  type        = string
  description = "Local directory to watch for realtime file changes."
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

variable "s3client_max_connections" {
  type        = number
  description = "S3Client max connections for reading data files."
}

variable "s3client_max_range_bytes" {
  type        = number
  description = "S3Client max range bytes for reading data files."
}

variable "num_shards" {
  type        = number
  description = "Total number of shards."
}

variable "udf_num_workers" {
  type        = number
  description = "Number of workers for UDF execution."
}

variable "route_v1_to_v2" {
  type        = bool
  description = "Whether to route V1 requests through V2."
}

variable "use_real_coordinators" {
  type        = bool
  description = "Use real coordinators."
}
