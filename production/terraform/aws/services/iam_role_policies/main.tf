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

data "aws_iam_policy_document" "instance_policy_doc" {
  statement {
    sid       = "AllowInstancesToListS3DataFiles"
    actions   = ["s3:ListBucket"]
    effect    = "Allow"
    resources = [var.s3_delta_file_bucket_arn]
  }
  statement {
    sid       = "AllowInstancesToReadS3DataFiles"
    actions   = ["s3:GetObject"]
    effect    = "Allow"
    resources = ["${var.s3_delta_file_bucket_arn}/*"]
  }
  statement {
    sid       = "AllowInstancesToReadTags"
    actions   = ["ec2:Describe*"]
    effect    = "Allow"
    resources = ["*"]
  }
  statement {
    sid       = "AllowInstancesToReadParameters"
    actions   = ["ssm:GetParameter"]
    effect    = "Allow"
    resources = var.server_parameter_arns
  }
  statement {
    sid     = "AllowInstancesToManageSqsQueues"
    actions = [
      "sqs:CreateQueue",
      "sqs:Get*",
      "sqs:ReceiveMessage",
      "sqs:TagQueue"
    ]
    resources = ["*"]
  }
  statement {
    sid       = "AllowInstancesToSubscribeToDataUpdates"
    actions   = ["sns:Subscribe"]
    resources = [var.sns_data_updates_topic_arn]
  }
}

resource "aws_iam_policy" "instance_policy" {
  name   = format("%s-%s-InstancePolicy", var.service, var.environment)
  policy = data.aws_iam_policy_document.instance_policy_doc.json
}

resource "aws_iam_role_policy_attachment" "instance_role_policy_attachment" {
  policy_arn = aws_iam_policy.instance_policy.arn
  role       = var.server_instance_role_name
}

# Set up access policies for the SQS cleanup lambda function.
data "aws_iam_policy_document" "sqs_cleanup_lambda_policy_doc" {
  statement {
    sid = "AllowLambdaToManageDataSubscriptions"
    actions = [
      "sns:Get*",
      "sns:List*",
      "sns:Unsubscribe"
    ]
    resources = [var.sns_data_updates_topic_arn]
  }
  statement {
    sid = "AllowLambdaToManageSQSQueues"
    actions = [
      "sqs:List*",
      "sqs:Get*",
      "sqs:DeleteQueue"
    ]
    resources = ["*"]
  }
  # Equivalent to managed policy "service-role/AWSLambdaBasicExecutionRole"
  statement {
    sid = "AllowLambdaToWriteLogs"
    actions = [
      "logs:CreateLogGroup",
      "logs:CreateLogStream",
      "logs:PutLogEvents"
    ]
    resources = ["*"]
  }
}

resource "aws_iam_policy" "sqs_cleanup_lambda_policy" {
  name   = format("%s-%s-SQSCleanupLambaPolicy", var.service, var.environment)
  policy = data.aws_iam_policy_document.sqs_cleanup_lambda_policy_doc.json
}

resource "aws_iam_role_policy_attachment" "sqs_cleanup_lambda_access" {
  policy_arn = aws_iam_policy.sqs_cleanup_lambda_policy.arn
  role       = var.sqs_cleanup_lambda_role_name
}
