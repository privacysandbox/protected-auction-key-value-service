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

resource "aws_ssm_parameter" "mode_parameter" {
  name      = "${var.service}-${var.environment}-mode"
  type      = "String"
  value     = var.mode_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "s3_bucket_parameter" {
  name      = "${var.service}-${var.environment}-data-bucket-id"
  type      = "String"
  value     = var.s3_bucket_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "bucket_update_sns_arn_parameter" {
  name      = "${var.service}-${var.environment}-bucket-sns-arn"
  type      = "String"
  value     = var.bucket_update_sns_arn_parameter_value
  overwrite = true
}

resource "aws_ssm_parameter" "launch_hook_parameter" {
  name      = "${var.service}-${var.environment}-launch-hook"
  type      = "String"
  value     = var.launch_hook_parameter_value
  overwrite = true
}
