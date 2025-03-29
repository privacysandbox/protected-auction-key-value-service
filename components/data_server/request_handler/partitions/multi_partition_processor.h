/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMPONENTS_DATA_SERVER_REQUEST_HANDLER_PARTITIONS_MULTI_PARTITION_PROCESSOR_H_
#define COMPONENTS_DATA_SERVER_REQUEST_HANDLER_PARTITIONS_MULTI_PARTITION_PROCESSOR_H_

#include "components/data_server/request_handler/content_type/encoder.h"
#include "components/data_server/request_handler/partitions/partition_processor.h"
#include "components/udf/udf_client.h"
#include "components/util/request_context.h"
#include "public/api_schema.pb.h"
#include "public/query/v2/get_values_v2.grpc.pb.h"

namespace kv_server {

// Processor for v2::GetValuesRequest with multiple partitions
class MultiPartitionProcessor : public PartitionProcessor {
 public:
  MultiPartitionProcessor(const RequestContextFactory& request_context_factory,
                          const UdfClient& udf_client,
                          const V2EncoderDecoder& v2_codec);

  // Passes input to UDF and populates GetValuesResponse.compression_groups
  absl::Status Process(const v2::GetValuesRequest& request,
                       v2::GetValuesResponse& response,
                       ExecutionMetadata& execution_metadata) const override;

 private:
  const RequestContextFactory& request_context_factory_;
  const UdfClient& udf_client_;
  const V2EncoderDecoder& v2_codec_;
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_REQUEST_HANDLER_PARTITIONS_MULTI_PARTITION_PROCESSOR_H_
