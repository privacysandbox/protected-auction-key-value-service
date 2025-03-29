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

#include "components/data_server/request_handler/partitions/partition_processor.h"

#include <memory>

#include "components/data_server/request_handler/content_type/encoder.h"
#include "components/data_server/request_handler/partitions/multi_partition_processor.h"
#include "components/data_server/request_handler/partitions/single_partition_processor.h"
#include "components/udf/udf_client.h"
#include "components/util/request_context.h"

namespace kv_server {

std::unique_ptr<PartitionProcessor> PartitionProcessor::Create(
    bool single_partition_use_case,
    const RequestContextFactory& request_context_factory,
    const UdfClient& udf_client, const V2EncoderDecoder& v2_codec) {
  if (single_partition_use_case) {
    return std::make_unique<SinglePartitionProcessor>(request_context_factory,
                                                      udf_client);
  } else {
    return std::make_unique<MultiPartitionProcessor>(request_context_factory,
                                                     udf_client, v2_codec);
  }
}

}  // namespace kv_server
