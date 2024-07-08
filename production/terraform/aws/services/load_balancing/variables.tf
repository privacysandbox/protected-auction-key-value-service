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

variable "vpc_id" {
  description = "VPC id where instances will be created."
  type        = string
}

variable "elb_subnet_ids" {
  description = "A list of subnets to associate with the NLB."
  type        = list(string)
}

variable "certificate_arn" {
  description = "ARN for a certificate to be attached to the NLB listener. Ingored if enable_external_traffic is false."
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

variable "server_port" {
  description = "Port on which the KV server listens for TCP connections."
  type        = number
}

variable "http_api_paths" {
  type = set(string)
  default = [
    "/v1/*",
    "/v2/*",
    "/healthcheck"
  ]
}

variable "grpc_api_paths" {
  type = set(string)
  default = [
    "/kv_server.v1.KeyValueService/*",
    "/kv_server.v2.KeyValueService/*",
    "/grpc.health.v1.Health/*"
  ]
}

variable "http_healthcheck_path" {
  description = "The HTTP path for KV server health checks."
  type        = string
  default     = "/healthcheck"
}

variable "grpc_healthcheck_path" {
  type    = string
  default = "/grpc.health.v1.Health/healthcheck"
}

variable "elb_security_group_id" {
  description = "Security group for the ALB."
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
