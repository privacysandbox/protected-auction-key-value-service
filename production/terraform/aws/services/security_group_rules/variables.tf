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
  description = "VPC id where security groups will be created."
  type        = string
}

variable "elb_security_group_id" {
  description = "Id of the load balancer listener security group."
  type        = string
}

variable "instances_security_group_id" {
  description = "Id of the security group for server ec2 instances."
  type        = string
}

variable "vpce_security_group_id" {
  description = "Id of the security group for backend vpc interface endpoints."
  type        = string
}

variable "ssh_security_group_id" {
  description = "Id of the security group for the ssh ec2 instance."
}

variable "gateway_endpoints_prefix_list_ids" {
  description = "Prefix lists for backend vpc gateway endpoints."
  type        = set(string)
}

variable "server_instance_port" {
  description = "The port on which EC2 server instances listen for connections."
  type        = number
}
