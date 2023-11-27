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

#ifndef PUBLIC_TEST_UTIL_MOCKS_H_
#define PUBLIC_TEST_UTIL_MOCKS_H_

#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "public/data_loading/readers/riegeli_stream_io.h"
#include "public/data_loading/readers/stream_record_reader.h"
#include "public/data_loading/riegeli_metadata.pb.h"
#include "src/cpp/telemetry/mocks.h"

namespace kv_server {

// We have to specialize the template due to the problem that MOCK_METHOD can't
// work well with template.
class MockStreamRecordReader : public StreamRecordReader {
 public:
  MOCK_METHOD(absl::StatusOr<KVFileMetadata>, GetKVFileMetadata, (),
              (override));

  MOCK_METHOD(
      absl::Status, ReadStreamRecords,
      (const std::function<absl::Status(const std::string_view&)>& callback),
      (override));
};

class MockStreamRecordReaderFactory : public StreamRecordReaderFactory {
 public:
  MockStreamRecordReaderFactory()
      : StreamRecordReaderFactory(metrics_recorder_) {}
  MOCK_METHOD(std::unique_ptr<StreamRecordReader>, CreateReader,
              (std::istream & data_input), (const, override));
  MOCK_METHOD(std::unique_ptr<StreamRecordReader>, CreateConcurrentReader,
              (std::function<std::unique_ptr<RecordStream>()>),
              (const, override));

 private:
  privacy_sandbox::server_common::MockMetricsRecorder metrics_recorder_;
};

}  // namespace kv_server

#endif  // PUBLIC_TEST_UTIL_MOCKS_H_
