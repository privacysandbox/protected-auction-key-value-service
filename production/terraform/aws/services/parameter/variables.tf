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

variable "mode_parameter_value" {
  description = "DSP or SSP."
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

variable "metrics_export_interval_millis_parameter_value" {
  description = "Export interval for metrics in milliseconds."
  type        = number
}

variable "metrics_export_timeout_millis_parameter_value" {
  description = "Export timeout for metrics in milliseconds."
  type        = number
}
