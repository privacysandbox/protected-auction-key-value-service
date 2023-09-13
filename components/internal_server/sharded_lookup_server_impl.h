/*
 * Copyright 2023 Google LLC
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

#ifndef COMPONENTS_INTERNAL_SERVER_SHARDED_LOOKUP_SERVER_IMPL_H_
#define COMPONENTS_INTERNAL_SERVER_SHARDED_LOOKUP_SERVER_IMPL_H_

#include <future>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "components/internal_server/lookup.grpc.pb.h"
#include "components/internal_server/lookup.h"
#include "components/internal_server/remote_lookup_client.h"
#include "components/sharding/shard_manager.h"
#include "grpcpp/grpcpp.h"
#include "pir/hashing/sha256_hash_family.h"
#include "src/cpp/telemetry/metrics_recorder.h"
#include "src/cpp/telemetry/telemetry.h"

namespace kv_server {

// Implements the internal lookup service for the data store.
// TODO(b/290640967): Remove since it's no longer necessary to have internal
// server
class ShardedLookupServiceImpl final
    : public kv_server::InternalLookupService::Service {
 public:
  ShardedLookupServiceImpl(const Lookup& lookup) : lookup_(lookup) {}

  virtual ~ShardedLookupServiceImpl() = default;

  // Iterates over all keys specified in the `request` and assigns them to shard
  // buckets. Then for each bucket it queries the underlying data shard. At the
  // moment, for the shard number matching the current server shard number, the
  // logic will lookup data in its own cache. Eventually, this will change when
  // we have two types of servers: UDF and data servers. Then the responses are
  // combined and the result is returned. If any underlying request fails -- we
  // return an empty response and `Internal` error as the status for the gRPC
  // status code.
  grpc::Status InternalLookup(
      grpc::ServerContext* context,
      const kv_server::InternalLookupRequest* request,
      kv_server::InternalLookupResponse* response) override;

  grpc::Status InternalRunQuery(
      grpc::ServerContext* context,
      const kv_server::InternalRunQueryRequest* request,
      kv_server::InternalRunQueryResponse* response) override;

  const Lookup& lookup_;
};

}  // namespace kv_server

#endif  // COMPONENTS_INTERNAL_SERVER_SHARDED_LOOKUP_SERVER_IMPL_H_
