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

variable "gcp_image_tag" {
  description = "Tag of the gcp docker image uploaded to the artifact registry."
  type        = string
}

variable "gcp_image_repo" {
  description = "A URL to a docker image repo containing the key-value service"
  type        = string
}

variable "service" {
  description = "Assigned name of the KV server."
  type        = string
}

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

variable "service_port" {
  description = "The grpc port that receives traffic destined for the key-value service."
  type        = number
}

variable "envoy_port" {
  description = "External load balancer will send traffic to this port. Envoy will forward traffic to kv_service_port. Must match envoy.yaml."
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

variable "parameters" {
  type        = map(string)
  description = "Kv-server runtime parameters"
}

variable "tee_impersonate_service_accounts" {
  type        = string
  description = "Tee can impersonate these service accounts. Necessary for coordinators."
}

variable "shard_num" {
  description = "Shard number."
  type        = string
}

variable "enable_external_traffic" {
  description = "Whether to serve external traffic. If disabled, only internal traffic via service mesh will be served."
  default     = true
  type        = bool
}
