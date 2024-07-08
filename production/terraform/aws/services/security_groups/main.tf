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

# Setup security groups for various network components.
#
# NOTE that security group rules are managed in "../security_group_rules" module.

################################################################################
# If use_existing_vpc is true, we need to use existing security groups.
################################################################################

data "aws_security_group" "existing_elb_security_group" {
  count = var.use_existing_vpc ? 1 : 0
  name  = "${var.existing_vpc_operator}-${var.existing_vpc_environment}-elb-sg"
}

data "aws_security_group" "existing_ssh_security_group" {
  count = var.use_existing_vpc ? 1 : 0
  name  = "${var.existing_vpc_operator}-${var.existing_vpc_environment}-ssh-sg"
}

data "aws_security_group" "existing_instance_security_group" {
  count = var.use_existing_vpc ? 1 : 0
  name  = "${var.existing_vpc_operator}-${var.existing_vpc_environment}-instance-sg"
}

data "aws_security_group" "existing_vpce_security_group" {
  count = var.use_existing_vpc ? 1 : 0
  name  = "${var.existing_vpc_operator}-${var.existing_vpc_environment}-vpce-sg"
}

################################################################################
# If use_existing_vpc is false, create security groups.
################################################################################

# Security group to control ingress and egress traffic for the load balancer.
resource "aws_security_group" "elb_security_group" {
  count  = var.use_existing_vpc ? 0 : 1
  name   = "${var.service}-${var.environment}-elb-sg"
  vpc_id = var.vpc_id

  tags = {
    Name = "${var.service}-${var.environment}-elb-sg"
  }
}

# Security group to control ingress and egress traffic for the ssh ec2 instance.
resource "aws_security_group" "ssh_security_group" {
  count  = var.use_existing_vpc ? 0 : 1
  name   = "${var.service}-${var.environment}-ssh-sg"
  vpc_id = var.vpc_id

  tags = {
    Name = "${var.service}-${var.environment}-ssh-sg"
  }
}

# Security group to control ingress and egress traffic for the server ec2 instances.
resource "aws_security_group" "instance_security_group" {
  count  = var.use_existing_vpc ? 0 : 1
  name   = "${var.service}-${var.environment}-instance-sg"
  vpc_id = var.vpc_id

  tags = {
    Name = "${var.service}-${var.environment}-instance-sg"
  }
}

# Security group to control ingress and egress traffic to backend vpc endpoints.
resource "aws_security_group" "vpce_security_group" {
  count  = var.use_existing_vpc ? 0 : 1
  name   = "${var.service}-${var.environment}-vpce-sg"
  vpc_id = var.vpc_id

  tags = {
    Name = "${var.service}-${var.environment}-vpce-sg"
  }
}
