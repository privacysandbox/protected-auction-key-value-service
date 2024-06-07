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

output "instance_security_group_id" {
  value = var.use_existing_vpc ? data.aws_security_group.existing_instance_security_group[0].id : aws_security_group.instance_security_group[0].id
}

output "elb_security_group_id" {
  value = var.use_existing_vpc ? data.aws_security_group.existing_elb_security_group[0].id : aws_security_group.elb_security_group[0].id
}

output "ssh_security_group_id" {
  value = var.use_existing_vpc ? data.aws_security_group.existing_ssh_security_group[0].id : aws_security_group.ssh_security_group[0].id
}

output "vpc_endpoint_security_group_id" {
  value = var.use_existing_vpc ? data.aws_security_group.existing_vpce_security_group[0].id : aws_security_group.vpce_security_group[0].id
}
