/**
 * Copyright 2024 Google LLC
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

variable "service" {
  description = "One of: bidding, auction, bfe, sfe"
  type        = string
}

variable "existing_vpc_operator" {
  description = "Operator of the existing VPC. Ingored if use_existing_vpc is false."
  type        = string
}

variable "existing_vpc_environment" {
  description = "Environment of the existing VPC. Ingored if use_existing_vpc is false."
  type        = string
}

variable "root_domain" {
  description = "Root domain for APIs."
  type        = string
}

variable "service_port" {
  description = "Port on which this service recieves outbound traffic"
  type        = number
}

variable "root_domain_zone_id" {
  description = "Zone id for the root domain."
  type        = string
}

variable "server_instance_role_name" {
  description = "Role for server EC2 instance profile."
  type        = string
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

variable "healthcheck_timeout_sec" {
  description = "Amount of time to wait for a health check response in seconds."
  type        = number
}
