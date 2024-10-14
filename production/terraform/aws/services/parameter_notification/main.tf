# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# SNS topic for listening to logging verbosity update
resource "aws_sns_topic" "logging_verbosity_level_sns_topic" {
  name = "${var.service}-${var.environment}-logging-verbosity-update-sns-topic"

  tags = {
    Name = "${var.service}-${var.environment}-logging-verbosity-update-sns-topic"
  }
}

resource "aws_cloudwatch_event_rule" "parameter_update_event_rule" {
  name        = "${var.service}-${var.environment}-parameter-update-event-rule"
  description = "Event rule to trigger events of parameter update notification"
  event_pattern = jsonencode({
    "source" : ["aws.ssm"],
    "detail-type" : ["Parameter Store Change"],
    "detail" : {
      "name" : [
        "${var.service}-${var.environment}-logging-verbosity-level"
      ],
      "operation" : [
        "Update"
      ]
    }
  })
}

# Allow Event bridge to publish events to SNS topic.
data "aws_iam_policy_document" "sns_topic_policy_doc" {
  statement {
    principals {
      identifiers = [
        "events.amazonaws.com"
      ]
      type = "Service"
    }

    actions = [
      "SNS:Publish"
    ]

    resources = [
      aws_sns_topic.logging_verbosity_level_sns_topic.arn
    ]
  }
}

resource "aws_sns_topic_policy" "sns_topic_policy" {
  arn    = aws_sns_topic.logging_verbosity_level_sns_topic.arn
  policy = data.aws_iam_policy_document.sns_topic_policy_doc.json
}

resource "aws_cloudwatch_event_target" "logging_parameter_update_target" {
  target_id = "${var.service}-${var.environment}-logging-verbosity-update-target"
  rule      = aws_cloudwatch_event_rule.parameter_update_event_rule.name
  arn       = aws_sns_topic.logging_verbosity_level_sns_topic.arn
  depends_on = [
    aws_sns_topic.logging_verbosity_level_sns_topic,
    aws_cloudwatch_event_rule.parameter_update_event_rule
  ]
}
