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

output "mode_parameter_arn" {
  value = aws_ssm_parameter.mode_parameter.arn
}

output "s3_bucket_parameter_arn" {
  value = aws_ssm_parameter.s3_bucket_parameter.arn
}

output "bucket_update_sns_arn_parameter_arn" {
  value = aws_ssm_parameter.bucket_update_sns_arn_parameter.arn
}

output "launch_hook_parameter_arn" {
  value = aws_ssm_parameter.launch_hook_parameter.arn
}
