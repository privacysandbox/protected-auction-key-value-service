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

variable "server_instance_role_name" {
  description = "Role for server EC2 instance profile."
  type        = string
}

variable "sqs_cleanup_lambda_role_name" {
  description = "Role for SQS cleanup lambda."
  type        = string
}

variable "s3_delta_file_bucket_arn" {
  description = "ARN for the S3 delta file bucket."
  type        = string
}

variable "server_parameter_arns" {
  description = "A set of arns for server parameters."
  type        = set(string)
}

variable "coordinator_parameter_arns" {
  description = "A set of arns for server parameters."
  type        = set(string)
}

variable "metrics_collector_endpoint_arns" {
  description = "A set of arns for metrics collector endpoint"
  type        = set(string)
}

variable "sharding_key_regex_arns" {
  description = "A set of arns for sharding key regex"
  type        = set(string)
}

variable "consented_debug_token_arns" {
  description = "A set of arns for consented debug token"
  type        = set(string)
}


variable "sns_data_updates_topic_arn" {
  description = "ARN for the sns topic that receives s3 delta file updates."
  type        = string
}

variable "sns_realtime_topic_arn" {
  description = "ARN for the sns topic that receives realtime updates."
  type        = string
}

variable "logging_verbosity_updates_topic_arn" {
  description = "ARN for the sns topic that receives logging verbosity parameter updates."
  type        = string
}

variable "ssh_instance_role_name" {
  description = "Role for SSH instance (bastion)."
}
