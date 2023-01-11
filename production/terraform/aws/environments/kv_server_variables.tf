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

variable "mode" {
  description = "DSP or SSP."
  type        = string
  validation {
    condition = contains([
      "DSP",
    "SSP"], var.mode)
    error_message = "The mode value must be either \"DSP\" or \"SSP\"."
  }
}

variable "region" {
  description = "AWS region to deploy to."
  type        = string
}

variable "environment" {
  description = "Assigned environment name to group related resources."
  type        = string
  validation {
    condition     = length(var.environment) <= 10
    error_message = "Due to current naming scheme limitations, environment must not be longer than 10."
  }
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

variable "certificate_arn" {
  description = "ARN for a certificate to be attached to the ALB listener."
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

variable "s3_delta_file_bucket_name" {
  description = "Globally unique name for S3 delta file bucket."
  type        = string
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

variable "vpc_interface_endpoint_services" {
  description = "List of AWS services to create vpc interface endpoints for."
  type        = set(string)
}

variable "vpc_gateway_endpoint_services" {
  description = "List of AWS services to create vpc gateway endpoints for."
  type        = set(string)
}

variable "enclave_memory_mib" {
  description = "Amount of memory to allocate to the enclave."
  type        = number
}

variable "enclave_cpu_count" {
  description = "The number of vcpus to allocate to the enclave."
  type        = number
}

variable "vpc_cidr_block" {
  description = "CIDR range for the VPC where KV server will be deployed."
  type        = string
}

variable "server_port" {
  description = "Port on which the enclave listens for TCP connections."
  type        = number
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

variable "poll_frequency_mins" {
  description = "Poll frequency for delta file notifier in minutes."
  type        = string
}
