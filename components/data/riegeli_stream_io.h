/*
 * Copyright 2022 Google LLC
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

#ifndef COMPONENTS_DATA_RIEGELI_STREAM_IO_H_
#define COMPONENTS_DATA_RIEGELI_STREAM_IO_H_

#include <utility>

#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "glog/logging.h"
#include "public/data_loading/riegeli_metadata.pb.h"
#include "riegeli/bytes/istream_reader.h"
#include "riegeli/records/record_reader.h"

namespace fledge::kv_server {

// Reader that can be used to load data from one data file.
//
// Subclasses should accept the data source through constructor and store the
// data source as its state.
//
// Not intended to be used by multiple threads.
template <typename RecordT>
class StreamRecordReader {
 public:
  virtual ~StreamRecordReader() = default;

  // Returns the metadata associated with this file. Can only be called once
  // before the first call to `ReadStreamRecords`.
  virtual absl::StatusOr<KVFileMetadata> GetKVFileMetadata() = 0;

  // Given a `data_input` stream representing a stream of RecordT
  // records, parses the records and calls `callback` once per record.
  // If the callback returns a non-OK status, the function continues
  // reading and logs the error at the end.
  virtual absl::Status ReadStreamRecords(
      const std::function<absl::Status(const RecordT&)>& callback) = 0;
};

// Reader that can read streams in Riegeli format.
template <typename RecordT>
class RiegeliStreamReader : public StreamRecordReader<RecordT> {
 public:
  // `data_input` must be at the file beginning when passed in.
  explicit RiegeliStreamReader(
      std::istream& data_input,
      std::function<bool(const riegeli::SkippedRegion&)> recover)
      : reader_(riegeli::RecordReader(
            riegeli::IStreamReader(&data_input),
            riegeli::RecordReaderBase::Options().set_recovery(
                std::move(recover)))) {}

  absl::StatusOr<KVFileMetadata> GetKVFileMetadata() override {
    riegeli::RecordsMetadata metadata;
    if (!reader_.ReadMetadata(metadata)) {
      if (reader_.ok()) {
        return absl::UnavailableError("Metadata not found");
      }
      return reader_.status();
    }

    auto file_metadata = metadata.GetExtension(kv_file_metadata);
    VLOG(2) << "File metadata: " << file_metadata.DebugString();
    return file_metadata;
  }

  ~RiegeliStreamReader() { reader_.Close(); }

  absl::Status ReadStreamRecords(
      const std::function<absl::Status(const RecordT&)>& callback) override {
    RecordT record;
    absl::Status overall_status;
    while (reader_.ReadRecord(record)) {
      overall_status.Update(callback(record));
    }
    if (!overall_status.ok()) {
      LOG(ERROR) << overall_status;
    }
    return reader_.status();
  }

 private:
  riegeli::RecordReader<riegeli::IStreamReader<>> reader_;
};

// Factory class to create readers. For each input that represents one file, one
// reader should be created.
template <typename RecordT>
class StreamRecordReaderFactory {
 public:
  virtual ~StreamRecordReaderFactory() = default;
  static std::unique_ptr<StreamRecordReaderFactory<RecordT>> Create();

  virtual std::unique_ptr<StreamRecordReader<RecordT>> CreateReader(
      std::istream& data_input) const {
    return std::make_unique<RiegeliStreamReader<RecordT>>(
        data_input, [](const riegeli::SkippedRegion& skipped_region) {
          LOG(WARNING) << "Skipping over corrupted region: " << skipped_region;
          return true;
        });
  }
};

template <typename RecordT>
std::unique_ptr<StreamRecordReaderFactory<RecordT>>
StreamRecordReaderFactory<RecordT>::Create() {
  return std::make_unique<StreamRecordReaderFactory<RecordT>>();
}

}  // namespace fledge::kv_server

#endif  // COMPONENTS_DATA_RIEGELI_STREAM_IO_H_
