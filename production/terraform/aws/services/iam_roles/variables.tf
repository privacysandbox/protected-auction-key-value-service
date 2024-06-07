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

variable "use_existing_vpc" {
  description = "Whether to use existing VPC. If true, only internal traffic via mesh will be served; variable vpc_operator and vpc_environment will be requried."
  type        = bool
}

variable "existing_vpc_operator" {
  description = "Operator of the existing VPC. Ingored if use_existing_vpc is false."
  type        = string
}

variable "existing_vpc_environment" {
  description = "Environment of the existing VPC. Ingored if use_existing_vpc is false."
  type        = string
}
