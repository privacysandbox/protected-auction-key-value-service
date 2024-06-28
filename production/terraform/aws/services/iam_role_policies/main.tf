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
    sid       = "AllowInstancesToCompleteLifecycleAction"
    actions   = ["autoscaling:CompleteLifecycleAction"]
    effect    = "Allow"
    resources = ["*"]
  }
  statement {
    sid       = "AllowInstancesToRecordLifecycleActionHeartbeat"
    actions   = ["autoscaling:RecordLifecycleActionHeartbeat"]
    effect    = "Allow"
    resources = ["*"]
  }
  statement {
    sid       = "AllowInstancesToDescribeAutoScalingInstances"
    actions   = ["autoscaling:DescribeAutoScalingInstances"]
    effect    = "Allow"
    resources = ["*"]
  }
  statement {
    sid       = "AllowInstancesToDescribeAutoScalingGroups"
    actions   = ["autoscaling:DescribeAutoScalingGroups"]
    effect    = "Allow"
    resources = ["*"]
  }
  statement {
    sid       = "AllowInstancesToReadParameters"
    actions   = ["ssm:GetParameter"]
    effect    = "Allow"
    resources = setunion(var.server_parameter_arns, var.coordinator_parameter_arns, var.metrics_collector_endpoint_arns, var.sharding_key_regex_arns, var.consented_debug_token_arns)
  }
  statement {
    sid       = "AllowInstancesToAssumeRole"
    actions   = ["sts:AssumeRole"]
    effect    = "Allow"
    resources = ["*"]
  }
  statement {
    sid = "AllowInstancesToManageSqsQueues"
    actions = [
      "sqs:CreateQueue",
      "sqs:Get*",
      "sqs:ReceiveMessage",
      "sqs:TagQueue",
      "sqs:SetQueueAttributes",
      "sqs:DeleteMessage"
    ]
    resources = ["*"]
  }
  statement {
    sid       = "AllowInstancesToSubscribeToDataUpdates"
    actions   = ["sns:Subscribe"]
    resources = [var.sns_data_updates_topic_arn]
  }
  statement {
    sid       = "AllowInstancesToSubscribeToRealtimeDataUpdates"
    actions   = ["sns:Subscribe"]
    resources = [var.sns_realtime_topic_arn]
  }
  statement {
    sid = "AllowXRay"
    actions = [
      "xray:PutTraceSegments",
      "xray:PutTelemetryRecords",
      "xray:GetSamplingRules",
      "xray:GetSamplingTargets",
      "xray:GetSamplingStatisticSummaries",
    ]
    resources = ["*"]
    effect    = "Allow"
  }
  statement {
    sid = "AllowOtelWriteLogs"
    actions = [
      "logs:CreateLogGroup",
      "logs:CreateLogStream",
      "logs:PutLogEvents",
    ]
    resources = ["*"]
  }
  statement {
    sid = "AllowOtelPutMetricData"
    actions = [
      "cloudwatch:PutMetricData",
    ]
    resources = ["*"]
  }
  statement {
    sid = "AllowPrometheusRemoteWrite"
    actions = [
      "aps:RemoteWrite",
    ]
    resources = ["*"]
  }
  statement {
    sid = "AllowInstancesToSetInstanceHealthForASGandCloudMap"
    actions = [
      "autoscaling:SetInstanceHealth",
      "servicediscovery:UpdateInstanceCustomHealthStatus",
      "servicediscovery:DeregisterInstance",
    ]
    effect    = "Allow"
    resources = ["*"]
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
    resources = ["*"]
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

# Set up policies for using EC2 instance connect.
data "aws_iam_policy_document" "ssh_instance_policy_doc" {
  statement {
    sid       = "AllowSSHInstanceToSendSSHPublicKey"
    actions   = ["ec2-instance-connect:SendSSHPublicKey"]
    resources = ["arn:aws:ec2:*:*:instance/*"]
    condition {
      test     = "StringEquals"
      variable = "aws:ResourceTag/environment"
      values   = [var.environment]
    }
    condition {
      test     = "StringEquals"
      variable = "ec2:osuser"
      values   = ["ec2-user"]
    }
  }
  statement {
    sid       = "AllowSSHInstanceToDescribeInstances"
    actions   = ["ec2:DescribeInstances"]
    resources = ["*"]
  }
}

resource "aws_iam_policy" "ssh_instance_policy" {
  name   = format("%s-%s-sshInstancePolicy", var.service, var.environment)
  policy = data.aws_iam_policy_document.ssh_instance_policy_doc.json
}

resource "aws_iam_role_policy_attachment" "ssh_instance_access" {
  policy_arn = aws_iam_policy.ssh_instance_policy.arn
  role       = var.ssh_instance_role_name
}
