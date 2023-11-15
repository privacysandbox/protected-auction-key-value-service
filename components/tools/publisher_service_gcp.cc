// Copyright 2022 Google LLC
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

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "components/errors/error_util_gcp.h"
#include "components/tools/publisher_service.h"
#include "components/util/platform_initializer.h"
#include "google/cloud/pubsub/publisher.h"

ABSL_FLAG(std::string, gcp_topic_id, "", "GCP topic id");

namespace kv_server {
namespace {

const char kQueuePrefix[] = "QueueNotifier_";

namespace pubsub = ::google::cloud::pubsub;
using ::google::cloud::future;
using ::google::cloud::StatusOr;

class GcpPublisherService : public PublisherService {
 public:
  GcpPublisherService(std::string project_id, std::string topic_id)
      : publisher_(pubsub::Publisher(pubsub::MakePublisherConnection(
            pubsub::Topic(project_id, topic_id)))) {}

  absl::Status Publish(const std::string& body) {
    std::string nanos_since_epoch =
        std::to_string(absl::ToUnixNanos(absl::Now()));
    auto id = publisher_
                  .Publish(pubsub::MessageBuilder{}
                               .SetData(body)
                               .SetAttribute("time_sent", nanos_since_epoch)
                               .Build())
                  .get();
    if (!id.ok()) {
      return GoogleErrorStatusToAbslStatus(id.status());
    }
    return absl::OkStatus();
  }

  absl::StatusOr<NotifierMetadata> BuildNotifierMetadataAndSetQueue() {
    auto maybe_notifier_metadata = PublisherService::GetNotifierMetadata();
    if (!maybe_notifier_metadata.ok()) {
      return maybe_notifier_metadata;
    }
    GcpNotifierMetadata metadata =
        std::get<GcpNotifierMetadata>(maybe_notifier_metadata.value());
    metadata.queue_prefix = kQueuePrefix;
    return metadata;
  }

 private:
  pubsub::Publisher publisher_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<PublisherService>> PublisherService::Create(
    NotifierMetadata notifier_metadata) {
  auto metadata = std::get<GcpNotifierMetadata>(notifier_metadata);
  return std::make_unique<GcpPublisherService>(std::move(metadata.project_id),
                                               std::move(metadata.topic_id));
}

absl::StatusOr<NotifierMetadata> PublisherService::GetNotifierMetadata() {
#if defined(CLOUD_PLATFORM_GCP)
  const std::string gcp_project_id = absl::GetFlag(FLAGS_gcp_project_id);
  const std::string gcp_topic_id = absl::GetFlag(FLAGS_gcp_topic_id);
  if (gcp_project_id.empty() || gcp_topic_id.empty()) {
    return absl::InvalidArgumentError(
        "Please specify a full set of parameters for the GCP platform.");
  }
  return GcpNotifierMetadata{.project_id = gcp_project_id,
                             .topic_id = gcp_topic_id};
#endif
}
}  // namespace kv_server
