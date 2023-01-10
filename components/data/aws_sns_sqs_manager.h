/*
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

#ifndef COMPONENTS_DATA_AWS_SNS_SQS_MANAGER_H_
#define COMPONENTS_DATA_AWS_SNS_SQS_MANAGER_H_

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "aws/sns/SNSClient.h"
#include "aws/sqs/SQSClient.h"

namespace kv_server {

class AwsSnsSqsManager {
 public:
  // `prefix` is the prefix of randomly generated SQS Queue name.
  // The queue is subscribed to the topic at `sns_arn`.
  AwsSnsSqsManager(std::string prefix, std::string sns_arn)
      : prefix_(std::move(prefix)), sns_arn_(std::move(sns_arn)) {}

  bool IsSetupComplete() const { return is_set_up_; }

  // Returns url if `IsSetupComplete` is true, or empty string otherwise.
  const std::string& GetSqsUrl() const { return sqs_url_; }

  // Creates an SQS Queue with 80 character random name starting with `prefix`
  // Queue is subscribed to the `sns_arn`.
  // On failure, `SetupQueue` can be recalled.
  absl::Status SetupQueue();

  // Resets setup state, allowing `SetupQueue` to be recalled if previously
  // successful.
  void Reset();

 private:
  Aws::SQS::SQSClient sqs_client_;
  Aws::SNS::SNSClient sns_client_;
  const std::string prefix_;
  const std::string sns_arn_;
  bool is_set_up_ = false;
  std::string sqs_url_;
  std::string sqs_arn_;
  bool are_attributes_set_ = false;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_AWS_SNS_SQS_MANAGER_H_
