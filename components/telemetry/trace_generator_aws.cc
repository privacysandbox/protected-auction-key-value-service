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

#include "components/telemetry/trace_generator_aws.h"

#include <arpa/inet.h>

#include <chrono>
#include <functional>
#include <limits>
#include <utility>

#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "opentelemetry/sdk/trace/id_generator.h"
#include "opentelemetry/sdk/trace/random_id_generator_factory.h"

namespace kv_server {
namespace {
class XRayIdGenerator : public opentelemetry::sdk::trace::IdGenerator {
 public:
  explicit XRayIdGenerator(std::function<absl::Time()> now_func)
      : random_generator_(
            opentelemetry::sdk::trace::RandomIdGeneratorFactory::Create()),
        now_func_(std::move(now_func)) {}

  opentelemetry::trace::SpanId GenerateSpanId() noexcept override {
    return random_generator_->GenerateSpanId();
  }

  // Format definition: https://aws-otel.github.io/docs/getting-started/x-ray
  // Writing bytes of timestamp + random value is fine in network byte order.
  // example:
  // https://github.com/alfianabdi/opentelemetry-cpp/blob/main/sdk/src/trace/aws_xray_id_generator.cc
  opentelemetry::trace::TraceId GenerateTraceId() noexcept override {
    const uint32_t seconds_nl = htonl(absl::ToUnixSeconds(now_func_()));
    const uint64_t lo =
        absl::Uniform(bitgen_, 0u, std::numeric_limits<decltype(lo)>::max());
    const uint32_t hi =
        absl::Uniform(bitgen_, 0u, std::numeric_limits<decltype(hi)>::max());
    uint8_t buf[opentelemetry::trace::TraceId::kSize];
    memcpy(buf, &seconds_nl, sizeof(seconds_nl));
    memcpy(buf + sizeof(seconds_nl), &hi, sizeof(hi));
    memcpy(buf + sizeof(hi) + sizeof(seconds_nl), &lo, sizeof(lo));
    return opentelemetry::trace::TraceId(buf);
  }

 private:
  std::unique_ptr<opentelemetry::sdk::trace::IdGenerator> random_generator_;
  std::function<absl::Time()> now_func_;
  absl::BitGen bitgen_;
};

}  // namespace

std::unique_ptr<opentelemetry::sdk::trace::IdGenerator> CreateXrayIdGenerator(
    std::function<absl::Time()> now_func) {
  return std::make_unique<XRayIdGenerator>(std::move(now_func));
}

}  // namespace kv_server
