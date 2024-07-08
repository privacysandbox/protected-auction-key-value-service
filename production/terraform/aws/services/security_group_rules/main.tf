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

# Ingress and egress rules for the load balancer listener.
resource "aws_security_group_rule" "allow_all_elb_ingress" {
  count             = var.use_existing_vpc ? 0 : 1
  from_port         = 443
  protocol          = "TCP"
  security_group_id = var.elb_security_group_id
  to_port           = 443
  type              = "ingress"
  cidr_blocks       = ["0.0.0.0/0"]
}

resource "aws_security_group_rule" "allow_all_elb_ingress2" {
  from_port         = 8443
  protocol          = "TCP"
  security_group_id = var.elb_security_group_id
  to_port           = 8443
  type              = "ingress"
  cidr_blocks       = ["0.0.0.0/0"]
}

resource "aws_security_group_rule" "allow_elb_to_ec2_egress" {
  from_port                = var.server_instance_port
  protocol                 = "TCP"
  security_group_id        = var.elb_security_group_id
  to_port                  = var.server_instance_port
  type                     = "egress"
  source_security_group_id = var.instances_security_group_id
}

# Ingress and egress rules for SSH.
resource "aws_security_group_rule" "allow_all_ssh_ingress" {
  count             = var.use_existing_vpc ? 0 : 1
  from_port         = 22
  protocol          = "TCP"
  security_group_id = var.ssh_security_group_id
  to_port           = 22
  type              = "ingress"
  cidr_blocks       = var.ssh_source_cidr_blocks
}

resource "aws_security_group_rule" "allow_ssh_to_ec2_egress" {
  count                    = var.use_existing_vpc ? 0 : 1
  from_port                = 22
  protocol                 = "TCP"
  security_group_id        = var.ssh_security_group_id
  to_port                  = 22
  type                     = "egress"
  source_security_group_id = var.instances_security_group_id
}

resource "aws_security_group_rule" "allow_ssh_secure_tcp_egress" {
  count             = var.use_existing_vpc ? 0 : 1
  from_port         = 443
  protocol          = "TCP"
  security_group_id = var.ssh_security_group_id
  to_port           = 443
  type              = "egress"
  cidr_blocks       = ["0.0.0.0/0"]
}

# Ingress and egress rules for server ec2 instances.
resource "aws_security_group_rule" "allow_elb_to_ec2_ingress" {
  from_port                = var.server_instance_port
  protocol                 = "TCP"
  security_group_id        = var.instances_security_group_id
  to_port                  = var.server_instance_port
  type                     = "ingress"
  source_security_group_id = var.elb_security_group_id
}

resource "aws_security_group_rule" "allow_ssh_to_ec2_ingress" {
  count                    = var.use_existing_vpc ? 0 : 1
  from_port                = 22
  protocol                 = "TCP"
  security_group_id        = var.instances_security_group_id
  to_port                  = 22
  type                     = "ingress"
  source_security_group_id = var.ssh_security_group_id
}

resource "aws_security_group_rule" "allow_ec2_to_vpc_endpoint_egress" {
  count                    = var.use_existing_vpc ? 0 : 1
  from_port                = 443
  protocol                 = "TCP"
  security_group_id        = var.instances_security_group_id
  to_port                  = 443
  type                     = "egress"
  source_security_group_id = var.vpce_security_group_id
}

resource "aws_security_group_rule" "allow_ec2_to_vpc_ge_egress" {
  count             = var.use_existing_vpc ? 0 : 1
  from_port         = 443
  protocol          = "TCP"
  security_group_id = var.instances_security_group_id
  to_port           = 443
  type              = "egress"
  prefix_list_ids   = [for id in var.gateway_endpoints_prefix_list_ids : id]
}

data "aws_ip_ranges" "ec2_instance_connect_ip_ranges" {
  regions  = [var.region]
  services = ["ec2_instance_connect"]
}

resource "aws_security_group_rule" "allow_ec2_instance_connect_ingress" {
  count             = var.use_existing_vpc ? 0 : 1
  from_port         = 22
  protocol          = "TCP"
  security_group_id = var.instances_security_group_id
  to_port           = 22
  type              = "ingress"
  cidr_blocks       = data.aws_ip_ranges.ec2_instance_connect_ip_ranges.cidr_blocks
}

# Ingress and egress rules for backend vpc interface endpoints.
resource "aws_security_group_rule" "allow_ec2_to_vpce_ingress" {
  count                    = var.use_existing_vpc ? 0 : 1
  from_port                = 443
  protocol                 = "TCP"
  security_group_id        = var.vpce_security_group_id
  to_port                  = 443
  type                     = "ingress"
  source_security_group_id = var.instances_security_group_id
}

resource "aws_security_group_rule" "allow_ssh_instance_to_vpce_ingress" {
  count                    = var.use_existing_vpc ? 0 : 1
  from_port                = 443
  protocol                 = "TCP"
  security_group_id        = var.vpce_security_group_id
  to_port                  = 443
  type                     = "ingress"
  source_security_group_id = var.ssh_security_group_id
}

resource "aws_security_group_rule" "allow_ec2_to_ec2_endpoint_egress" {
  from_port                = 50100
  protocol                 = "TCP"
  security_group_id        = var.instances_security_group_id
  to_port                  = 50100
  type                     = "egress"
  source_security_group_id = var.instances_security_group_id
}

resource "aws_security_group_rule" "allow_ec2_to_ec2_endpoint_ingress" {
  from_port                = 50100
  protocol                 = "TCP"
  security_group_id        = var.instances_security_group_id
  to_port                  = 50100
  type                     = "ingress"
  source_security_group_id = var.instances_security_group_id
}

resource "aws_security_group_rule" "allow_ec2_secure_tcp_egress" {
  count             = var.use_existing_vpc ? 0 : 1
  from_port         = 443
  protocol          = "TCP"
  security_group_id = var.instances_security_group_id
  to_port           = 443
  type              = "egress"
  cidr_blocks       = ["0.0.0.0/0"]
}
