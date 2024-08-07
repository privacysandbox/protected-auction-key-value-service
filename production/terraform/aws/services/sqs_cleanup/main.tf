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

resource "aws_lambda_function" "sqs_cleanup_function" {
  function_name = "${var.service}-${var.environment}-sqs-cleanup"
  role          = var.lambda_role_arn
  image_uri     = var.sqs_cleanup_image_uri
  package_type  = "Image"
  timeout       = 900
  memory_size   = 512

  tags = {
    Name = "${var.service}-${var.environment}-sqs-cleanup"
  }
}

# Schedule clean up function to run periodically.
resource "aws_cloudwatch_event_rule" "sqs_cleanup_schedule" {
  name                = "${var.service}-${var.environment}-sqs-cleanup-schedule"
  schedule_expression = var.sqs_cleanup_schedule

  depends_on = [
    aws_lambda_function.sqs_cleanup_function
  ]

  tags = {
    Name = "${var.service}-${var.environment}-sqs-cleanup-schedule"
  }
}

resource "aws_cloudwatch_event_target" "sqs_cleanup_target" {
  arn  = aws_lambda_function.sqs_cleanup_function.arn
  rule = aws_cloudwatch_event_rule.sqs_cleanup_schedule.name

  input = <<JSON
  {
    "sns_topic": "${var.sns_data_updates_topic_arn}",
    "queue_prefix": "BlobNotifier_",
    "timeout_secs": "${var.sqs_queue_timeout_secs}",
    "realtime_sns_topic": "${var.sns_realtime_topic_arn}",
    "realtime_queue_prefix": "QueueNotifier_",
    "logging_verbosity_updates_sns_topic": "${var.sns_logging_verbosity_updates_topic_arn}",
    "parameter_queue_prefix": "ParameterNotifier_"
  }
  JSON
}

# Set up permissions to allow CloudWatch to invoke the SQS cleanup lambda.
resource "aws_lambda_permission" "allow_sqs_cleanup_execution" {
  statement_id  = "AllowSQSCleanupExecutionFromCloudWatch"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.sqs_cleanup_function.function_name
  principal     = "events.amazonaws.com"
  source_arn    = aws_cloudwatch_event_rule.sqs_cleanup_schedule.arn
}
