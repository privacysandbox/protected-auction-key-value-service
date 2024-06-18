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
#ifndef COMPONENTS_DATA_SERVER_CACHE_NOOP_KEY_VALUE_CACHE_H_
#define COMPONENTS_DATA_SERVER_CACHE_NOOP_KEY_VALUE_CACHE_H_

#include <memory>
#include <string>

#include "components/data_server/cache/cache.h"

namespace kv_server {
class NoOpKeyValueCache : public Cache {
 public:
  absl::flat_hash_map<std::string, std::string> GetKeyValuePairs(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const override {
    return {};
  };
  std::unique_ptr<kv_server::GetKeyValueSetResult> GetKeyValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const override {
    return std::make_unique<NoOpGetKeyValueSetResult>();
  }
  std::unique_ptr<GetKeyValueSetResult> GetUInt32ValueSet(
      const RequestContext& request_context,
      const absl::flat_hash_set<std::string_view>& key_set) const override {
    return std::make_unique<NoOpGetKeyValueSetResult>();
  }
  void UpdateKeyValue(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::string_view key, std::string_view value, int64_t logical_commit_time,
      std::string_view prefix) override {}
  void UpdateKeyValueSet(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::string_view key, absl::Span<std::string_view> value_set,
      int64_t logical_commit_time, std::string_view prefix) override {}
  void UpdateKeyValueSet(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::string_view key, absl::Span<uint32_t> value_set,
      int64_t logical_commit_time, std::string_view prefix = "") override {}
  void DeleteKey(privacy_sandbox::server_common::log::PSLogContext& log_context,
                 std::string_view key, int64_t logical_commit_time,
                 std::string_view prefix) override {}
  void DeleteValuesInSet(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::string_view key, absl::Span<std::string_view> value_set,
      int64_t logical_commit_time, std::string_view prefix) override {}
  void DeleteValuesInSet(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      std::string_view key, absl::Span<uint32_t> value_set,
      int64_t logical_commit_time, std::string_view prefix = "") override {}
  void RemoveDeletedKeys(
      privacy_sandbox::server_common::log::PSLogContext& log_context,
      int64_t logical_commit_time, std::string_view prefix) override {}
  static std::unique_ptr<Cache> Create() {
    return std::make_unique<NoOpKeyValueCache>();
  }

 private:
  class NoOpGetKeyValueSetResult : public GetKeyValueSetResult {
    absl::flat_hash_set<std::string_view> GetValueSet(
        std::string_view key) const override {
      return {};
    }
    const UInt32ValueSet* GetUInt32ValueSet(
        std::string_view key) const override {
      return nullptr;
    }
    void AddKeyValueSet(
        std::string_view key, absl::flat_hash_set<std::string_view> value_set,
        std::unique_ptr<absl::ReaderMutexLock> key_lock) override {}
    void AddUInt32ValueSet(
        std::string_view key,
        ThreadSafeHashMap<std::string, UInt32ValueSet>::ConstLockedNodePtr
            value_set_node) override {}
  };
};

}  // namespace kv_server

#endif  // COMPONENTS_DATA_SERVER_CACHE_NOOP_KEY_VALUE_CACHE_H_
