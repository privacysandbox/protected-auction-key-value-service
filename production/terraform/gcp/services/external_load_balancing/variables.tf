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

variable "service" {
  description = "Assigned name of the KV server."
  type        = string
}

variable "environment" {
  description = "Assigned environment name to group related resources."
  type        = string
}

variable "service_port" {
  description = "The grpc port that receives traffic destined for the key-value service."
  type        = number
}

variable "server_url" {
  description = "Kv-serer URL. Example: kv-server-environment.example.com"
  type        = string
}

variable "server_dns_zone" {
  description = "Google Cloud Dns zone for Kv-serer."
  type        = string
}

variable "server_domain_ssl_certificate_id" {
  description = "Ssl certificate id of the Kv-server domain."
  type        = string
}

variable "server_ip_address" {
  description = "IP address of the Kv-server."
  type        = string
}

variable "instance_groups" {
  description = "Kv server instance group URsL created by instance group managers."
  type        = set(string)
}

variable "internal_load_balancer" {
  description = "Service mesh."
  type        = string
}

variable "grpc_route" {
  description = "Google network services grpc route for kv_server"
  type        = string
}
