// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "components/data_server/request_handler/partitions/single_partition_processor.h"

#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "components/data_server/request_handler/status/status_tag.h"
#include "components/udf/udf_client.h"
#include "components/util/request_context.h"
#include "public/api_schema.pb.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"

namespace kv_server {

SinglePartitionProcessor::SinglePartitionProcessor(
    const RequestContextFactory& request_context_factory,
    const UdfClient& udf_client)
    : request_context_factory_(request_context_factory),
      udf_client_(udf_client) {}

absl::Status SinglePartitionProcessor::Process(
    const v2::GetValuesRequest& request, v2::GetValuesResponse& response,
    ExecutionMetadata& execution_metadata) const {
  if (request.partitions().size() > 1) {
    return V2RequestFormatErrorAsExternalHttpError(absl::InvalidArgumentError(
        "This use case only accepts single partitions, but "
        "multiple partitions were found."));
  }
  if (request.partitions().empty()) {
    return V2RequestFormatErrorAsExternalHttpError(
        absl::InvalidArgumentError("No partitions in request."));
  }

  // TODO(b/355434272): Return early on CBOR content type (not supported)
  const auto& req_partition = request.partitions(0);
  v2::ResponsePartition* resp_partition = response.mutable_single_partition();
  resp_partition->set_id(req_partition.id());

  // UDF Metadata
  UDFExecutionMetadata udf_metadata;
  *udf_metadata.mutable_request_metadata() = request.metadata();
  if (!req_partition.metadata().fields().empty()) {
    *udf_metadata.mutable_partition_metadata() = req_partition.metadata();
  }
  const auto maybe_output_string =
      udf_client_.ExecuteCode(request_context_factory_, std::move(udf_metadata),
                              req_partition.arguments(), execution_metadata);
  if (!maybe_output_string.ok()) {
    resp_partition->mutable_status()->set_code(
        static_cast<int>(maybe_output_string.status().code()));
    resp_partition->mutable_status()->set_message(
        maybe_output_string.status().message());
    return maybe_output_string.status();
  }
  PS_VLOG(5, request_context_factory_.Get().GetPSLogContext())
      << "UDF output: " << maybe_output_string.value();
  resp_partition->set_string_output(std::move(maybe_output_string).value());
  return absl::OkStatus();
}

}  // namespace kv_server
