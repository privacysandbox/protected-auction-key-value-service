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

output "instance_profile_arn" {
  value = var.use_existing_vpc ? data.aws_iam_instance_profile.existing_instance_profile[0].arn : aws_iam_instance_profile.instance_profile[0].arn
}

output "instance_role_name" {
  value = var.use_existing_vpc ? data.aws_iam_role.existing_instance_role[0].name : aws_iam_role.instance_role[0].name
}

output "instance_role_arn" {
  value = var.use_existing_vpc ? data.aws_iam_role.existing_instance_role[0].arn : aws_iam_role.instance_role[0].arn
}

output "lambda_role_arn" {
  value = aws_iam_role.lambda_role.arn
}

output "lambda_role_name" {
  value = aws_iam_role.lambda_role.name
}

output "ssh_instance_role_arn" {
  value = var.use_existing_vpc ? data.aws_iam_role.existing_ssh_instance_role[0].arn : aws_iam_role.ssh_instance_role[0].arn
}

output "ssh_instance_role_name" {
  value = var.use_existing_vpc ? data.aws_iam_role.existing_ssh_instance_role[0].name : aws_iam_role.ssh_instance_role[0].name
}

output "ssh_instance_profile_name" {
  value = var.use_existing_vpc ? data.aws_iam_instance_profile.existing_ssh_instance_profile[0].name : aws_iam_instance_profile.ssh_instance_profile[0].name
}
