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

variable "service" {
  description = "Assigned name of the KV server."
  type        = string
}

variable "environment" {
  description = "Assigned environment name to group related resources."
  type        = string
}

variable "s3_bucket_parameter_value" {
  description = "Value for S3 data bucket parameter stored in Parameter Store."
  type        = string
}

variable "bucket_update_sns_arn_parameter_value" {
  description = "Value for data bucket update SNS ARN parameter."
  type        = string
}

variable "realtime_sns_arn_parameter_value" {
  description = "Value for realtime update SNS ARN parameter."
  type        = string
}

variable "backup_poll_frequency_secs_parameter_value" {
  description = "Backup poll frequency for delta file notifier in seconds."
  type        = number
}

variable "metrics_collector_endpoint" {
  description = "Metrics collector endpoint"
  type        = string
}

variable "metrics_export_interval_millis_parameter_value" {
  description = "Export interval for metrics in milliseconds."
  type        = number
}

variable "metrics_export_timeout_millis_parameter_value" {
  description = "Export timeout for metrics in milliseconds."
  type        = number
}

variable "realtime_updater_num_threads_parameter_value" {
  description = "Amount of realtime notifier threads."
  type        = number
}

variable "data_loading_num_threads_parameter_value" {
  description = "Number of parallel threads for reading and loading data files."
  type        = number
}

variable "s3client_max_connections_parameter_value" {
  description = "S3Client max connections for reading data files."
  type        = number
}

variable "s3client_max_range_bytes_parameter_value" {
  description = "S3Client max range bytes for reading data files."
  type        = number
}

variable "num_shards_parameter_value" {
  description = "Total shards numbers."
  type        = number
}

variable "udf_num_workers_parameter_value" {
  description = "Total number of workers for UDF execution."
  type        = number
}

variable "route_v1_requests_to_v2_parameter_value" {
  description = "Whether to route V1 requests through V2."
  type        = bool
}

variable "use_real_coordinators_parameter_value" {
  description = "Number of parallel threads for reading and loading data files."
  type        = bool
}

variable "primary_coordinator_account_identity_parameter_value" {
  description = "Account identity for the primary coordinator."
  type        = string
}

variable "secondary_coordinator_account_identity_parameter_value" {
  description = "Account identity for the secondary coordinator."
  type        = string
}

variable "use_external_metrics_collector_endpoint" {
  description = "Whether to connect external metrics collector endpoint"
  type        = bool
}
