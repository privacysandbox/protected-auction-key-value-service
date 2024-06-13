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

variable "environment" {
  description = "Assigned environment name to group related resources."
  type        = string
}

variable "vpc_id" {
  description = "VPC ID that will host the instance group."
  type        = string
}

variable "subnets" {
  description = "All subnets to deploy to. Each subnet's region is used to configure a regional instance manager."
  type        = any
}

variable "service_account_email" {
  description = "Email of the service account that be used by all instances"
  type        = string
}

variable "vm_startup_delay_seconds" {
  description = "The time it takes to get a service up and responding to heartbeats (in seconds)."
  type        = number
}

variable "cpu_utilization_percent" {
  description = "CPU utilization percentage across an instance group required for autoscaler to add instances."
  type        = number
}

variable "max_collectors_per_region" {
  description = "Maximum amount of Collectors per each service region (a single managed instance group)."
  type        = number
  default     = 2
}

variable "collector_machine_type" {
  description = "Machine type for the collector service."
  type        = string
}

variable "collector_service_name" {
  description = "Name of the collector service."
  type        = string
}

variable "collector_service_port" {
  description = "The grpc port that receives traffic destined for the OpenTelemetry collector."
  type        = number
}

variable "collector_startup_script_path" {
  description = "Path to collector service startup script."
  type        = string
}

variable "max_replicas_per_service_region" {
  description = "Maximum amount of replicas per each service region (a single managed instance group)."
  type        = number
}
