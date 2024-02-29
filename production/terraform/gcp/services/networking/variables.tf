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

variable "service" {
  description = "Assigned name of the KV server."
  type        = string
}

variable "environment" {
  description = "Assigned environment name to group related resources."
  type        = string
}

variable "regions" {
  description = "Regions to deploy to."
  type        = set(string)
}

variable "regions_cidr_blocks" {
  description = "A set of CIDR ranges for all specified regions. The number of blocks here should correspond to the number of regions."
  type        = set(string)
}

variable "regions_use_existing_nat" {
  description = "Regions that use existing nat. No new nats will be created for regions specified here."
  type        = set(string)
}

variable "collector_service_name" {
  description = "Assigned name of metrics collection service"
  type        = string
}

variable "use_existing_vpc" {
  description = "Whether to use existing vpc network."
  type        = bool
}

variable "existing_vpc_id" {
  description = "Existing vpc id. This would only be used if use_existing_vpc is true."
  type        = string
}

variable "enable_external_traffic" {
  description = "Whether to serve external traffic. If disabled, only internal traffic via service mesh will be served."
  default     = true
  type        = bool
}
