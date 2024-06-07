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

# Existing aws_vpc_endpoint from existing VPC
data "aws_vpc_endpoint" "existing_vpc_gateway_endpoint" {
  count        = var.use_existing_vpc ? 1 : 0
  service_name = "com.amazonaws.${var.region}.s3"
  filter {
    name   = "tag:operator"
    values = [var.existing_vpc_operator]
  }
  filter {
    name   = "tag:environment"
    values = [var.existing_vpc_environment]
  }
}

# Restrict VPC endpoint access only to instances in this environment and service.
data "aws_iam_policy_document" "vpce_policy_doc" {
  statement {
    sid       = "Sid1"
    actions   = ["*"]
    effect    = "Allow"
    resources = ["*"]
    principals {
      type        = "AWS"
      identifiers = ["*"]
    }
    condition {
      test     = "ArnEquals"
      variable = "aws:PrincipalArn"
      values   = [var.server_instance_role_arn, var.ssh_instance_role_arn]
    }
  }
}

# Create gateway endpoints for accessing AWS services.
resource "aws_vpc_endpoint" "vpc_gateway_endpoint" {
  count             = var.use_existing_vpc ? 0 : 1
  service_name      = "com.amazonaws.${var.region}.s3"
  vpc_id            = var.vpc_id
  vpc_endpoint_type = "Gateway"
  route_table_ids   = var.vpc_endpoint_route_table_ids
  policy            = data.aws_iam_policy_document.vpce_policy_doc.json

  tags = {
    Name = "${var.service}-${var.environment}-s3-endpoint"
  }
}

# Create interface endpoints for accessing AWS services.
resource "aws_vpc_endpoint" "vpc_interface_endpoint" {
  for_each = toset(concat([
    "sns",
    "sqs",
    "logs",
    ], var.prometheus_service_region == var.region ? ["aps-workspaces"] : []
    , var.use_existing_vpc ? [] : [
      "ec2",
      "ssm",
      "autoscaling",
      "xray",
  ]))
  service_name        = "com.amazonaws.${var.region}.${each.key}"
  vpc_id              = var.vpc_id
  vpc_endpoint_type   = "Interface"
  subnet_ids          = var.vpc_endpoint_subnet_ids
  private_dns_enabled = true
  policy              = data.aws_iam_policy_document.vpce_policy_doc.json

  security_group_ids = [
    var.vpc_endpoint_sg_id
  ]

  tags = {
    Name = "${var.service}-${var.environment}-${each.key}-endpoint"
  }
}
