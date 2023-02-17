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

output "s3_data_bucket_arn" {
  value = aws_s3_bucket.bucket.arn
}

output "s3_data_bucket_id" {
  value = aws_s3_bucket.bucket.id
}

output "sns_data_updates_topic_arn" {
  value = aws_sns_topic.sns_topic.arn
}

output "sns_realtime_topic_arn" {
  value = aws_sns_topic.realtime_sns_topic.arn
}
