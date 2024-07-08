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


################################################################################
# If use_existing_vpc is true, we need to use existing iam roles.
################################################################################

data "aws_iam_role" "existing_instance_role" {
  count = var.use_existing_vpc ? 1 : 0
  name  = format("%s-%s-InstanceRole", var.existing_vpc_operator, var.existing_vpc_environment)
}

data "aws_iam_instance_profile" "existing_instance_profile" {
  count = var.use_existing_vpc ? 1 : 0
  name  = format("%s-%s-InstanceProfile", var.existing_vpc_operator, var.existing_vpc_environment)
}

data "aws_iam_role" "existing_ssh_instance_role" {
  count = var.use_existing_vpc ? 1 : 0
  name  = format("%s-%s-sshInstanceRole", var.existing_vpc_operator, var.existing_vpc_environment)
}

data "aws_iam_instance_profile" "existing_ssh_instance_profile" {
  count = var.use_existing_vpc ? 1 : 0
  name  = format("%s-%s-sshInstanceProfile", var.existing_vpc_operator, var.existing_vpc_environment)
}

################################################################################
# If use_existing_vpc is false, create EC2 instance profile.
################################################################################
data "aws_iam_policy_document" "ec2_assume_role_policy" {
  count = var.use_existing_vpc ? 0 : 1
  statement {
    actions = [
      "sts:AssumeRole"
    ]
    principals {
      identifiers = [
        "ec2.amazonaws.com"
      ]
      type = "Service"
    }
  }
}

resource "aws_iam_role" "instance_role" {
  count              = var.use_existing_vpc ? 0 : 1
  name               = format("%s-%s-InstanceRole", var.service, var.environment)
  assume_role_policy = data.aws_iam_policy_document.ec2_assume_role_policy[0].json

  tags = {
    Name = format("%s-%s-InstanceRole", var.service, var.environment)
  }
}

resource "aws_iam_instance_profile" "instance_profile" {
  count = var.use_existing_vpc ? 0 : 1
  name  = format("%s-%s-InstanceProfile", var.service, var.environment)
  role  = aws_iam_role.instance_role[0].name

  tags = {
    Name = format("%s-%s-InstanceProfile", var.service, var.environment)
  }
}

################################################################################
# If use_existing_vpc is false, create SSH role for using EC2 instance connect.
################################################################################
resource "aws_iam_role" "ssh_instance_role" {
  count              = var.use_existing_vpc ? 0 : 1
  name               = format("%s-%s-sshInstanceRole", var.service, var.environment)
  assume_role_policy = data.aws_iam_policy_document.ec2_assume_role_policy[0].json

  tags = {
    Name = format("%s-%s-sshInstanceRole", var.service, var.environment)
  }
}

resource "aws_iam_instance_profile" "ssh_instance_profile" {
  count = var.use_existing_vpc ? 0 : 1
  name  = format("%s-%s-sshInstanceProfile", var.service, var.environment)
  role  = aws_iam_role.ssh_instance_role[0].name

  tags = {
    Name = format("%s-%s-sshInstanceProfile", var.service, var.environment)
  }
}

####################################################
# Create Lambda role required for SQS cleanup.
####################################################
data "aws_iam_policy_document" "lambda_assume_role_policy" {
  statement {
    actions = [
      "sts:AssumeRole"
    ]
    principals {
      identifiers = [
        "lambda.amazonaws.com"
      ]
      type = "Service"
    }
  }
}

resource "aws_iam_role" "lambda_role" {
  name               = format("%s-%s-LambdaRole", var.service, var.environment)
  assume_role_policy = data.aws_iam_policy_document.lambda_assume_role_policy.json

  tags = {
    Name = format("%s-%s-LambdaRole", var.service, var.environment)
  }
}
