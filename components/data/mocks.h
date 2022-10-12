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

#ifndef COMPONENTS_DATA_MOCKS_H_
#define COMPONENTS_DATA_MOCKS_H_

#include <memory>
#include <string>
#include <vector>

#include "components/data/blob_storage_client.h"
#include "components/data/delta_file_notifier.h"
#include "gmock/gmock.h"

namespace fledge::kv_server {

class MockBlobStorageClient : public BlobStorageClient {
 public:
  MOCK_METHOD(std::unique_ptr<BlobReader>, GetBlobReader,
              (DataLocation location), (override));
  MOCK_METHOD(absl::Status, PutBlob, (BlobReader&, DataLocation location),
              (override));
  MOCK_METHOD(absl::Status, DeleteBlob, (DataLocation location), (override));
  MOCK_METHOD(absl::StatusOr<std::vector<std::string>>, ListBlobs,
              (DataLocation location, ListOptions options), (override));
};

class MockBlobStorageChangeNotifier : public BlobStorageChangeNotifier {
 public:
  MOCK_METHOD(absl::StatusOr<std::vector<std::string>>, GetNotifications,
              (absl::Duration max_wait,
               const std::function<bool()>& should_stop_callback),
              (override));
};

class MockDeltaFileNotifier : public DeltaFileNotifier {
 public:
  MOCK_METHOD(absl::Status, StartNotify,
              (BlobStorageChangeNotifier & change_notifier,
               BlobStorageClient::DataLocation location,
               std::string start_after,
               std::function<void(const std::string& key)> callback),
              (override));
  MOCK_METHOD(absl::Status, StopNotify, (), (override));
  MOCK_METHOD(bool, IsRunning, (), (const, override));
};

class MockBlobReader : public BlobReader {
 public:
  MOCK_METHOD(std::istream&, Stream, (), (override));
  // True if the istream returned by `Stream` supports `seek`.
  MOCK_METHOD(bool, CanSeek, (), (const, override));
};

}  // namespace fledge::kv_server

#endif  // COMPONENTS_DATA_MOCKS_H_
