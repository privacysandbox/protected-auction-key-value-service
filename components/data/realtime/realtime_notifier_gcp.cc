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

#include <mutex>

#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "components/data/common/msg_svc.h"
#include "components/data/common/thread_manager.h"
#include "components/data/realtime/realtime_notifier.h"
#include "glog/logging.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/subscriber.h"
#include "src/cpp/telemetry/metrics_recorder.h"
#include "src/cpp/telemetry/telemetry.h"

namespace kv_server {
namespace {
namespace pubsub = ::google::cloud::pubsub;
namespace cloud = ::google::cloud;
using ::google::cloud::future;
using ::google::cloud::GrpcBackgroundThreadPoolSizeOption;
using ::google::cloud::Options;
using ::google::cloud::pubsub::Subscriber;
using ::privacy_sandbox::server_common::MetricsRecorder;
class RealtimeNotifierGcp : public RealtimeNotifier {
 public:
  explicit RealtimeNotifierGcp(MetricsRecorder& metrics_recorder,
                               std::unique_ptr<Subscriber> gcp_subscriber,
                               std::unique_ptr<SleepFor> sleep_for)
      : thread_manager_(TheadManager::Create("Realtime notifier")),
        metrics_recorder_(metrics_recorder),
        sleep_for_(std::move(sleep_for)),
        gcp_subscriber_(std::move(gcp_subscriber)) {}

  ~RealtimeNotifierGcp() {
    if (const auto s = Stop(); !s.ok()) {
      LOG(ERROR) << "Realtime updater failed to stop: " << s;
    }
  }

  absl::Status Start(
      std::function<absl::StatusOr<DataLoadingStats>(const std::string& key)>
          callback) override {
    return thread_manager_->Start(
        [this, callback = std::move(callback)]() mutable {
          Watch(std::move(callback));
        });
  }

  absl::Status Stop() override {
    absl::Status status;
    LOG(INFO) << "Realtime updater received stop signal.";
    {
      absl::MutexLock lock(&mutex_);
      if (session_.valid()) {
        VLOG(8) << "Session valid.";
        session_.cancel();
        VLOG(8) << "Session cancelled.";
      }
      status = sleep_for_->Stop();
      VLOG(8) << "Sleep for just called stop.";
    }
    status.Update(thread_manager_->Stop());
    LOG(INFO) << "Thread manager just called stop.";
    return status;
  }

  bool IsRunning() const override { return thread_manager_->IsRunning(); }

 private:
  void Watch(
      std::function<absl::StatusOr<DataLoadingStats>(const std::string& key)>
          callback) {
    {
      absl::MutexLock lock(&mutex_);
      session_ = (gcp_subscriber_->Subscribe(
          [&](pubsub::Message const& m, pubsub::AckHandler h) {
            // TODO: publish the e2e delivery metric with time, using the
            // provided metadata.
            std::string string_decoded;
            if (!absl::Base64Unescape(m.data(), &string_decoded)) {
              // TODO: emit a metric
              LOG(ERROR)
                  << "The body of the message is not a base64 encoded string.";

              std::move(h).ack();
              return;
            }
            callback(string_decoded);
            std::move(h).ack();
          }));
    }
    LOG(INFO) << "Realtime updater initialized.";
    sleep_for_->Duration(absl::InfiniteDuration());
    LOG(INFO) << "Realtime updater stopped watching.";
  }

  std::unique_ptr<TheadManager> thread_manager_;
  MetricsRecorder& metrics_recorder_;
  mutable absl::Mutex mutex_;
  future<cloud::Status> session_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<SleepFor> sleep_for_;
  std::unique_ptr<Subscriber> gcp_subscriber_;
};

absl::StatusOr<std::unique_ptr<Subscriber>> CreateSubscriber(
    NotifierMetadata metadata) {
  GcpNotifierMetadata notifier_metadata =
      std::get<GcpNotifierMetadata>(metadata);
  auto realtime_message_service_status = MessageService::Create(
      notifier_metadata,
      (notifier_metadata.num_shards > 1
           ? std::optional<int32_t>(notifier_metadata.shard_num)
           : std::nullopt));
  if (!realtime_message_service_status.ok()) {
    return realtime_message_service_status.status();
  }
  auto realtime_message_service = std::move(*realtime_message_service_status);
  auto queue_setup_result = realtime_message_service->SetupQueue();
  if (!queue_setup_result.ok()) {
    return queue_setup_result;
  }
  auto queue_metadata =
      std::get<GcpQueueMetadata>(realtime_message_service->GetQueueMetadata());
  LOG(INFO) << "Listening to queue_id " << queue_metadata.queue_id
            << " project id " << notifier_metadata.project_id << " with "
            << notifier_metadata.num_threads << " threads.";
  return std::make_unique<Subscriber>(pubsub::MakeSubscriberConnection(
      pubsub::Subscription(notifier_metadata.project_id,
                           queue_metadata.queue_id),
      Options{}
          .set<pubsub::MaxConcurrencyOption>(notifier_metadata.num_threads)
          .set<GrpcBackgroundThreadPoolSizeOption>(
              notifier_metadata.num_threads)));
}
}  // namespace

absl::StatusOr<std::unique_ptr<RealtimeNotifier>> RealtimeNotifier::Create(
    MetricsRecorder& metrics_recorder, NotifierMetadata metadata,
    RealtimeNotifierMetadata realtime_metadata) {
  auto realtime_notifier_metadata =
      std::get_if<GcpRealtimeNotifierMetadata>(&realtime_metadata);
  std::unique_ptr<SleepFor> sleep_for;
  if (realtime_notifier_metadata &&
      realtime_notifier_metadata->maybe_sleep_for) {
    sleep_for = std::move(realtime_notifier_metadata->maybe_sleep_for);
  } else {
    sleep_for = std::make_unique<SleepFor>();
  }
  std::unique_ptr<Subscriber> gcp_subscriber;
  if (realtime_notifier_metadata &&
      realtime_notifier_metadata->gcp_subscriber_for_unit_testing) {
    gcp_subscriber.reset(
        realtime_notifier_metadata->gcp_subscriber_for_unit_testing);
  } else {
    auto maybe_gcp_subscriber = CreateSubscriber(metadata);
    if (!maybe_gcp_subscriber.ok()) {
      return maybe_gcp_subscriber.status();
    }
    gcp_subscriber = std::move(*maybe_gcp_subscriber);
  }
  return std::make_unique<RealtimeNotifierGcp>(
      metrics_recorder, std::move(gcp_subscriber), std::move(sleep_for));
}

}  // namespace kv_server
