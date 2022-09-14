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

data "aws_ami" "amazon_linux" {
  most_recent = true
  owners = [
    "amazon"
  ]

  filter {
    name = "name"

    values = [
      "amzn2-ami-hvm*-x86_64-gp2",
    ]
  }

  filter {
    name = "owner-alias"

    values = [
      "amazon",
    ]
  }
}

# Create an instance that we can use to SSH into KV server instances.
resource "aws_instance" "ssh_instance" {
  ami                  = data.aws_ami.amazon_linux.id
  instance_type        = "t2.micro"
  subnet_id            = var.ssh_instance_subnet_ids[0]
  iam_instance_profile = var.instance_profile_name

  vpc_security_group_ids = [
    var.instance_sg_id
  ]

  # Enforce IMDSv2.
  metadata_options {
    http_endpoint          = "enabled"
    http_tokens            = "required"
    instance_metadata_tags = "enabled"
  }

  user_data = <<EOF
    #!/bin/bash

    pip3 install ec2instanceconnectcli
  EOF

  tags = {
    Name        = "${var.service}-${var.environment}-ssh-instance"
    service     = var.service
    environment = var.environment
  }
}
