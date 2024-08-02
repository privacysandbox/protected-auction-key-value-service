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

variable "telemetry_config" {
  description = "Telemetry config for exporting raw or noised metrics"
  type        = string
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

variable "add_missing_keys_v1_parameter_value" {
  description = "Add missing keys v1."
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

variable "data_loading_file_format_parameter_value" {
  description = "Data file format for blob storage and realtime updates. See /public/constants.h for possible values."
  type        = string
}

variable "logging_verbosity_level_parameter_value" {
  description = "Logging verbosity."
  type        = number
}

variable "logging_verbosity_update_sns_arn_parameter_value" {
  description = "Value for the logging verbosity update SNS ARN parameter."
  type        = string
}

variable "logging_verbosity_backup_poll_frequency_secs_parameter_value" {
  description = "Backup poll frequency in seconds for the logging verbosity parameter"
  type        = number
}

variable "use_sharding_key_regex_parameter_value" {
  description = "Use sharding key regex. This is useful if you want to use data locality feature for sharding."
  type        = bool
}

variable "sharding_key_regex_parameter_value" {
  description = "Sharding key regex."
  type        = string
}

variable "udf_timeout_millis_parameter_value" {
  description = "UDF execution timeout in milliseconds."
  type        = number
}

variable "udf_update_timeout_millis_parameter_value" {
  description = "UDF update timeout in milliseconds."
  type        = number
}

variable "udf_min_log_level_parameter_value" {
  description = "Minimum log level for UDFs. Info = 0, Warn = 1, Error = 2. The UDF will only attempt to log for min_log_level and above. Default is 0(info)."
  type        = number
}

variable "enable_otel_logger_parameter_value" {
  description = "Whether to enable otel logger."
  type        = bool
}

variable "data_loading_blob_prefix_allowlist" {
  description = "A comma separated list of prefixes (i.e., directories) where data is loaded from."
  type        = string
}

variable "primary_coordinator_private_key_endpoint_parameter_value" {
  description = "Primary coordinator private key endpoint."
  type        = string
}

variable "secondary_coordinator_private_key_endpoint_parameter_value" {
  description = "Secondary coordinator private key endpoint."
  type        = string
}

variable "primary_coordinator_region_parameter_value" {
  description = "Primary coordinator region."
  type        = string
}

variable "secondary_coordinator_region_parameter_value" {
  description = "Secondary coordinator region."
  type        = string
}

variable "public_key_endpoint_parameter_value" {
  description = "Public key endpoint. Can only be overriden in non-prod mode."
  type        = string
}

variable "consented_debug_token_parameter_value" {
  description = "Consented debug token to enable the otel collection of consented logs. Empty token means no-op and no logs will be collected for consented requests. The token in the request's consented debug configuration needs to match this debug token to make the server treat the request as consented."
  type        = string
}

variable "enable_consented_log_parameter_value" {
  description = "Enable the logging of consented requests. If it is set to true, the consented debug token parameter value must not be an empty string."
  type        = bool
}
