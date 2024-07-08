// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <optional>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "components/data/common/msg_svc.h"
#include "components/data/common/msg_svc_util.h"
#include "components/errors/error_util_gcp.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/subscriber.h"
#include "google/cloud/pubsub/subscription_admin_client.h"
#include "google/cloud/pubsub/topic.h"

namespace kv_server {
namespace pubsub = ::google::cloud::pubsub;

namespace {
using ::google::cloud::pubsub::Subscription;
using ::google::cloud::pubsub::SubscriptionBuilder;
using ::google::cloud::pubsub::Topic;

constexpr char kEnvironmentTag[] = "environment";
constexpr char kFilterPolicyTemplate[] = R"(attributes.shard_num="%d")";

class GcpMessageService : public MessageService {
 public:
  GcpMessageService(
      std::string prefix, std::string topic_id, std::string project_id,
      std::string environment,
      pubsub::SubscriptionAdminClient subscription_admin_client,
      std::optional<int32_t> shard_num,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : prefix_(std::move(prefix)),
        topic_id_(std::move(topic_id)),
        project_id_(project_id),
        environment_(environment),
        subscription_admin_client_(subscription_admin_client),
        shard_num_(shard_num),
        log_context_(log_context) {}

  bool IsSetupComplete() const {
    absl::ReaderMutexLock lock(&mutex_);
    return is_set_up_;
  }

  const QueueMetadata GetQueueMetadata() const {
    absl::ReaderMutexLock lock(&mutex_);
    GcpQueueMetadata metadata = {.queue_id = sub_id_};
    return metadata;
  }

  absl::Status SetupQueue() {
    absl::MutexLock lock(&mutex_);
    if (sub_id_.empty()) {
      absl::StatusOr<std::string> sub_id = CreateQueue();
      if (!sub_id.ok()) {
        return sub_id.status();
      }
      sub_id_ = std::move(*sub_id);
    }
    is_set_up_ = true;
    return absl::OkStatus();
  }

  void Reset() {
    absl::MutexLock lock(&mutex_);
    sub_id_ = "";
    is_set_up_ = false;
  }

 private:
  absl::StatusOr<std::string> CreateQueue() {
    std::string subscription_id = GenerateQueueName(prefix_);
    SubscriptionBuilder subscription_builder;
    subscription_builder.add_label(kEnvironmentTag, environment_);
    if (prefix_ == "QueueNotifier_" && shard_num_.has_value()) {
      subscription_builder.set_filter(
          absl::StrFormat(kFilterPolicyTemplate, shard_num_.value()));
    }
    PS_VLOG(1, log_context_)
        << "Creating a subscription for project id " << project_id_
        << " with subsciprition id " << subscription_id;
    auto sub = subscription_admin_client_.CreateSubscription(
        Topic(project_id_, std::move(topic_id_)),
        Subscription(project_id_, subscription_id), subscription_builder);
    if (!sub.status().ok()) {
      return GoogleErrorStatusToAbslStatus(sub.status());
    }

    sub_id_ = subscription_id;
    PS_VLOG(1, log_context_) << "Subscription created " << sub_id_;
    return subscription_id;
  }

  mutable absl::Mutex mutex_;
  const std::string prefix_;
  const std::string topic_id_;
  const std::string project_id_;
  const std::string environment_;
  bool is_set_up_ = false;
  std::string sub_id_;

  pubsub::SubscriptionAdminClient subscription_admin_client_;

  bool are_attributes_set_ = false;
  std::optional<int32_t> shard_num_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<MessageService>> MessageService::Create(
    NotifierMetadata notifier_metadata,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  auto metadata = std::get<GcpNotifierMetadata>(notifier_metadata);
  auto shard_num =
      (metadata.num_shards > 1 ? std::optional<int32_t>(metadata.shard_num)
                               : std::nullopt);
  pubsub::SubscriptionAdminClient subscription_admin_client(
      pubsub::MakeSubscriptionAdminConnection());

  return std::make_unique<GcpMessageService>(
      std::move(metadata.queue_prefix), std::move(metadata.topic_id),
      std::move(metadata.project_id), std::move(metadata.environment),
      std::move(subscription_admin_client), shard_num, log_context);
}
}  // namespace kv_server
