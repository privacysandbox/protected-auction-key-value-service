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

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "components/data/common/msg_svc.h"
#include "components/data/common/thread_manager.h"
#include "components/data/realtime/realtime_notifier.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/subscriber.h"
#include "src/telemetry/telemetry.h"

namespace kv_server {
namespace {
namespace pubsub = ::google::cloud::pubsub;
namespace cloud = ::google::cloud;
using ::google::cloud::future;
using ::google::cloud::GrpcBackgroundThreadPoolSizeOption;
using ::google::cloud::Options;
using ::google::cloud::pubsub::Subscriber;

class RealtimeNotifierGcp : public RealtimeNotifier {
 public:
  explicit RealtimeNotifierGcp(
      std::unique_ptr<Subscriber> gcp_subscriber,
      std::unique_ptr<SleepFor> sleep_for,
      privacy_sandbox::server_common::log::PSLogContext& log_context)
      : thread_manager_(ThreadManager::Create("Realtime notifier")),
        sleep_for_(std::move(sleep_for)),
        gcp_subscriber_(std::move(gcp_subscriber)),
        log_context_(log_context) {}

  ~RealtimeNotifierGcp() {
    if (const auto s = Stop(); !s.ok()) {
      PS_LOG(ERROR, log_context_) << "Realtime updater failed to stop: " << s;
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
    PS_LOG(INFO, log_context_) << "Realtime updater received stop signal.";
    {
      absl::MutexLock lock(&mutex_);
      if (session_.valid()) {
        PS_VLOG(8, log_context_) << "Session valid.";
        session_.cancel();
        PS_VLOG(8, log_context_) << "Session cancelled.";
      }
      status = sleep_for_->Stop();
      PS_VLOG(8, log_context_) << "Sleep for just called stop.";
    }
    status.Update(thread_manager_->Stop());
    PS_LOG(INFO, log_context_) << "Thread manager just called stop.";
    return status;
  }

  bool IsRunning() const override { return thread_manager_->IsRunning(); }

 private:
  void RecordGcpSuppliedE2ELatency(pubsub::Message const& m) {
    // The time at which the message was published, populated by the server when
    // it receives the topics.publish call. It must not be populated by the
    // publisher in a topics.publish call.
    LogIfError(
        KVServerContextMap()
            ->SafeMetric()
            .LogHistogram<kReceivedLowLatencyNotificationsE2ECloudProvided>(
                absl::ToDoubleMicroseconds(
                    absl::Now() - absl::FromChrono(m.publish_time()))));
  }

  void RecordProducerSuppliedE2ELatency(pubsub::Message const& m) {
    auto attributes = m.attributes();
    auto it = attributes.find("time_sent");
    if (it == attributes.end() || it->second.empty()) {
      return;
    }
    int64_t value_int64;
    if (!absl::SimpleAtoi(it->second, &value_int64)) {
      return;
    }
    auto e2eDuration = absl::Now() - absl::FromUnixNanos(value_int64);
    LogIfError(KVServerContextMap()
                   ->SafeMetric()
                   .LogHistogram<kReceivedLowLatencyNotificationsE2E>(
                       absl::ToDoubleMicroseconds(e2eDuration)));
  }

  void OnMessageReceived(
      pubsub::Message const& m, pubsub::AckHandler h,
      std::function<absl::StatusOr<DataLoadingStats>(const std::string& key)>&
          callback) {
    auto start = absl::Now();
    std::string string_decoded;
    size_t message_size = m.data().size();
    LogIfError(KVServerContextMap()
                   ->SafeMetric()
                   .LogHistogram<kReceivedLowLatencyNotificationsBytes>(
                       static_cast<int>(message_size)));
    LogIfError(KVServerContextMap()
                   ->SafeMetric()
                   .LogUpDownCounter<kReceivedLowLatencyNotificationsCount>(1));
    if (!absl::Base64Unescape(m.data(), &string_decoded)) {
      LogServerErrorMetric(kRealtimeDecodeMessageFailure);
      PS_LOG(ERROR, log_context_)
          << "The body of the message is not a base64 encoded string.";
      std::move(h).ack();
      return;
    }
    if (auto count = callback(string_decoded); !count.ok()) {
      PS_LOG(ERROR, log_context_)
          << "Data loading callback failed: " << count.status();
      LogServerErrorMetric(kRealtimeMessageApplicationFailure);
    }
    RecordGcpSuppliedE2ELatency(m);
    RecordProducerSuppliedE2ELatency(m);
    std::move(h).ack();
    LogIfError(KVServerContextMap()
                   ->SafeMetric()
                   .LogHistogram<kReceivedLowLatencyNotifications>(
                       absl::ToDoubleMicroseconds(absl::Now() - start)));
  }

  void Watch(
      std::function<absl::StatusOr<DataLoadingStats>(const std::string& key)>
          callback) {
    {
      absl::MutexLock lock(&mutex_);
      session_ = gcp_subscriber_->Subscribe(
          [&](pubsub::Message const& m, pubsub::AckHandler h) {
            OnMessageReceived(m, std::move(h), callback);
          });
    }
    PS_LOG(INFO, log_context_) << "Realtime updater initialized.";
    sleep_for_->Duration(absl::InfiniteDuration());
    PS_LOG(INFO, log_context_) << "Realtime updater stopped watching.";
  }

  std::unique_ptr<ThreadManager> thread_manager_;
  mutable absl::Mutex mutex_;
  future<cloud::Status> session_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<SleepFor> sleep_for_;
  std::unique_ptr<Subscriber> gcp_subscriber_;
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

absl::StatusOr<std::unique_ptr<Subscriber>> CreateSubscriber(
    NotifierMetadata metadata,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  GcpNotifierMetadata notifier_metadata =
      std::get<GcpNotifierMetadata>(metadata);
  auto realtime_message_service_status =
      MessageService::Create(notifier_metadata, log_context);
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
  PS_LOG(INFO, log_context)
      << "Listening to queue_id " << queue_metadata.queue_id << " project id "
      << notifier_metadata.project_id << " with "
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
    NotifierMetadata metadata, RealtimeNotifierMetadata realtime_metadata,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
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
    auto maybe_gcp_subscriber = CreateSubscriber(metadata, log_context);
    if (!maybe_gcp_subscriber.ok()) {
      return maybe_gcp_subscriber.status();
    }
    gcp_subscriber = std::move(*maybe_gcp_subscriber);
  }
  return std::make_unique<RealtimeNotifierGcp>(
      std::move(gcp_subscriber), std::move(sleep_for), log_context);
}

}  // namespace kv_server
