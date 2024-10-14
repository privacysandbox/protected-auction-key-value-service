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

variable "service" {
  description = "Assigned name of the KV server."
  type        = string
}

variable "environment" {
  description = "Assigned environment name to group related resources."
  type        = string
}

variable "lambda_role_arn" {
  description = "ARN for clean up lambda functions execution role."
  type        = string
}

variable "sqs_cleanup_image_uri" {
  description = "Uri for the SQS cleanup image."
  type        = string
}

variable "sqs_cleanup_schedule" {
  # https://docs.aws.amazon.com/AmazonCloudWatch/latest/events/ScheduledEvents.html
  description = "Schedule for cleaning up SQS queues, e.g., rate(5 minutes)."
  type        = string
}

variable "sqs_queue_timeout_secs" {
  description = "Clean up queues not updated within the timeout period."
  type        = number
}

variable "sns_data_updates_topic_arn" {
  description = "SNS topic where S3 delta file updates are pushed to."
  type        = string
}

variable "sns_realtime_topic_arn" {
  description = "SNS topic where realtime updates are pushed to."
  type        = string
}

variable "sns_logging_verbosity_updates_topic_arn" {
  description = "SNS topic where logging verbosity updates are pushed to."
  type        = string
}
