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

# Set up policies for using EC2 instance connect.
data "aws_iam_policy_document" "ssh_users_group_policy_doc" {
  statement {
    sid       = "AllowSSHUsersToSendSSHPublicKey"
    actions   = ["ec2-instance-connect:SendSSHPublicKey"]
    resources = [var.ssh_instance_arn]
    condition {
      test     = "StringEquals"
      variable = "ec2:osuser"
      values   = ["ec2-user"]
    }
  }
  statement {
    sid       = "AllowSSHUsersToDescribeInstances"
    actions   = ["ec2:DescribeInstances"]
    resources = ["*"]
  }
}

resource "aws_iam_policy" "ssh_users_group_policy" {
  name   = format("%s-%s-sshUsersGroupPolicy", var.service, var.environment)
  policy = data.aws_iam_policy_document.ssh_users_group_policy_doc.json
}

resource "aws_iam_group_policy_attachment" "ssh_users_group_access" {
  policy_arn = aws_iam_policy.ssh_users_group_policy.arn
  group      = var.ssh_users_group_name
}
