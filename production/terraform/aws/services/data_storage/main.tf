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

# S3 bucket where delta files and snapshot files are stored.
resource "aws_s3_bucket" "bucket" {
  bucket = var.s3_delta_file_bucket_name

  tags = {
    Name        = var.s3_delta_file_bucket_name
    service     = var.service
    environment = var.environment
  }
}

# Enable server side encryption.
resource "aws_s3_bucket_server_side_encryption_configuration" "bucket_encryption" {
  bucket = aws_s3_bucket.bucket.bucket

  rule {
    apply_server_side_encryption_by_default {
      sse_algorithm = "AES256"
    }
  }
}

# Block public access for the data bucket
resource "aws_s3_bucket_public_access_block" "bucket_public_access" {
  bucket = aws_s3_bucket.bucket.id

  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true
}

# Subscribe to data updates using SNS.
resource "aws_s3_bucket_notification" "bucket_notification" {
  bucket     = aws_s3_bucket.bucket.id
  depends_on = [var.bucket_notification_dependency]

  topic {
    events = [
      "s3:ObjectCreated:*"
    ]
    topic_arn = aws_sns_topic.sns_topic.arn
  }
}

# SNS topic for listening to data bucket updates.
resource "aws_sns_topic" "sns_topic" {
  name = "${aws_s3_bucket.bucket.bucket}-sns-topic"

  tags = {
    Name        = "${aws_s3_bucket.bucket.bucket}-sns-topic"
    service     = var.service
    environment = var.environment
  }
}

# Allow S3 to publish events to SNS topic.
data "aws_iam_policy_document" "sns_topic_policy_doc" {
  statement {
    principals {
      identifiers = [
        "s3.amazonaws.com"
      ]
      type = "Service"
    }

    actions = [
      "SNS:Publish"
    ]

    resources = [
      aws_sns_topic.sns_topic.arn
    ]

    condition {
      test = "ArnLike"
      values = [
        aws_s3_bucket.bucket.arn
      ]
      variable = "aws:SourceArn"
    }
  }
}

resource "aws_sns_topic_policy" "sns_topic_policy" {
  arn    = aws_sns_topic.sns_topic.arn
  policy = data.aws_iam_policy_document.sns_topic_policy_doc.json
}

# SNS topic for listening to realtime updates.
resource "aws_sns_topic" "realtime_sns_topic" {
  name = "${aws_s3_bucket.bucket.bucket}-realtime-sns-topic"

  tags = {
    Name        = "${aws_s3_bucket.bucket.bucket}-realtime-sns-topic"
    service     = var.service
    environment = var.environment
  }
}
