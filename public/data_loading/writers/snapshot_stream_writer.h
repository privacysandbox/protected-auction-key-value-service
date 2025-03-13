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

#ifndef PUBLIC_DATA_LOADING_WRITERS_SNAPSHOT_STREAM_WRITER_
#define PUBLIC_DATA_LOADING_WRITERS_SNAPSHOT_STREAM_WRITER_

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "public/data_loading/aggregation/record_aggregator.h"
#include "public/data_loading/filename_utils.h"
#include "public/data_loading/readers/delta_record_stream_reader.h"
#include "public/data_loading/record_utils.h"
#include "public/data_loading/riegeli_metadata.pb.h"
#include "public/data_loading/writers/delta_record_stream_writer.h"
#include "public/data_loading/writers/delta_record_writer.h"

namespace kv_server {

// A `SnapshotStreamWriter` writes flatbuffer native `DataRecordT` records to a
// destination snapshot stream. The `SnapshotStreamWriter` can be used to:
// (1) merge multiple delta files into a single snapshot file or
// (2) merge a base snapshot file with multiple delta files into a single
//     snapshot file
// as follows:
//
// ```
//  std::ofstream snapshot_stream(...);
//  auto snapshot_writer =
//    SnapshotStreamWriter<std::ofstream>::Create(Options{...},
//       snapshot_stream);
//  std::optional<std::ifstream> base_snapshot_stream = ...;
//  if (base_snapshot_stream.has_value()) {
//    auto status =
//      (*snapshot_writer)->WriteRecordStream(*base_snapshot_stream);
//    HandleErrorStatus(status);
//  }
//  auto delta_file_streams = ...
//  for (auto& src_stream : delta_file_streams) {
//    auto status = (*snapshot_writer)->WriteRecordStream(src_stream);
//    HandleErrorStatus(status);
//  }
//  status = (*snapshot_writer)->Finalize();
//  HandleErrorStatus(status);
// ```
// NOTE: This class is not thread safe.
template <typename DestStreamT = std::iostream>
class SnapshotStreamWriter {
 public:
  struct Options {
    // Metadata required for writing snapshot files. This is validated when the
    // `SnapshotStreamWriter` is created and initialized.
    KVFileMetadata metadata;
    // File used to store temporary data generated when writing records or
    // record streams to the output snapshot stream. If empty, then snapshot
    // generation is done completely in memory.
    std::string temp_data_file;
    // Whether to compress the snapshot stream or not.
    bool compress_snapshot;
  };

  ~SnapshotStreamWriter();
  SnapshotStreamWriter(const SnapshotStreamWriter&) = delete;
  SnapshotStreamWriter& operator=(const SnapshotStreamWriter&) = delete;

  static absl::StatusOr<std::unique_ptr<SnapshotStreamWriter>> Create(
      Options options, DestStreamT& dest_snapshot_stream);
  absl::Status WriteRecord(const DataRecordT& record);
  // Writes `DataRecordT` records from `src_stream` to the
  // output snapshot stream, `dest_snapshot_stream`. Valid source streams can be
  // snapshot files generated using `SnapshotStreamWriter` instances or
  // delta files generated using `DeltaRecordStreamWriter` instances.
  template <typename SrcStreamT = std::iostream>
  absl::Status WriteRecordStream(SrcStreamT& src_stream);
  // Finalizes the snapshot writer and flushes written records and makes them
  // visible in the destination snapshot stream.
  //
  // NOTE: This function must be called after all records have been written and
  // attempting to write records after calling this function results in error.
  //
  // Returns:
  // - `absl::OkStatus()` if the snapshot is successfully finalized.
  // - `!absl::OkStatus()` if there are any errors. The returned status
  // contains a detailed error message.
  absl::Status Finalize();

 private:
  SnapshotStreamWriter(
      std::unique_ptr<DeltaRecordStreamWriter<DestStreamT>> record_writer,
      std::unique_ptr<RecordAggregator> record_aggregator, Options options);

  absl::Status InsertOrUpdateRecord(const DataRecordT& record);
  template <typename SrcStreamT>
  absl::Status InsertOrUpdateRecords(SrcStreamT& src_stream);
  static absl::StatusOr<std::unique_ptr<RecordAggregator>>
  CreateRecordAggregator(std::string_view temp_data_file);
  static DeltaRecordWriter::Options CreateDeltaRecordWriterOptions(
      const Options& options);
  static absl::Status ValidateRequiredSnapshotMetadata(
      const KVFileMetadata& metadata);

  std::unique_ptr<DeltaRecordStreamWriter<DestStreamT>> record_writer_;
  std::unique_ptr<RecordAggregator> record_aggregator_;
  Options options_;
  bool is_finalized_ = false;
  std::unique_ptr<UserDefinedFunctionsConfigT> udf_config_;
};

template <typename DestStreamT>
SnapshotStreamWriter<DestStreamT>::SnapshotStreamWriter(
    std::unique_ptr<DeltaRecordStreamWriter<DestStreamT>> record_writer,
    std::unique_ptr<RecordAggregator> record_aggregator, Options options)
    : record_writer_(std::move(record_writer)),
      record_aggregator_(std::move(record_aggregator)),
      options_(std::move(options)) {}

template <typename DestStreamT>
SnapshotStreamWriter<DestStreamT>::~SnapshotStreamWriter() {
  if (auto status = Finalize(); !status.ok()) {
    LOG(ERROR) << "Failed to finalize: " << status;
  }
}

template <typename DestStreamT>
absl::StatusOr<std::unique_ptr<SnapshotStreamWriter<DestStreamT>>>
SnapshotStreamWriter<DestStreamT>::Create(Options options,
                                          DestStreamT& dest_snapshot_stream) {
  if (absl::Status status = ValidateRequiredSnapshotMetadata(options.metadata);
      !status.ok()) {
    return status;
  }
  auto record_aggregator = CreateRecordAggregator(options.temp_data_file);
  if (!record_aggregator.ok()) {
    return record_aggregator.status();
  }
  auto record_writer = DeltaRecordStreamWriter<DestStreamT>::Create(
      dest_snapshot_stream, CreateDeltaRecordWriterOptions(options));
  if (!record_writer.ok()) {
    return record_writer.status();
  }
  return absl::WrapUnique(new SnapshotStreamWriter<DestStreamT>(
      std::move(*record_writer), std::move(*record_aggregator),
      std::move(options)));
}

template <typename DestStreamT>
DeltaRecordWriter::Options
SnapshotStreamWriter<DestStreamT>::CreateDeltaRecordWriterOptions(
    const Options& options) {
  return DeltaRecordWriter::Options{
      .enable_compression = options.compress_snapshot,
      // TODO: Think about the best way to handle failed records. Should this be
      // exposed as a field of `SnapshotStreamWriter::Options`?
      .fb_struct_recovery_function =
          [](const DataRecordT& data_record) {
            if (data_record.record.type == Record::KeyValueMutationRecord) {
              LOG(ERROR) << "Failed to write record to snapshot stream. (key: "
                         << data_record.record.AsKeyValueMutationRecord()->key
                         << ")";
              return;
            }
            if (data_record.record.type == Record::UserDefinedFunctionsConfig) {
              LOG(ERROR) << "Failed to write record to snapshot stream. "
                            "(udf_code_snippet: "
                         << data_record.record.AsUserDefinedFunctionsConfig()
                                ->code_snippet
                         << ")";
              return;
            }
            if (data_record.record.type == Record::ShardMappingRecord) {
              LOG(ERROR)
                  << "Failed to write record to snapshot stream. "
                     "(logical_shard: "
                  << data_record.record.AsShardMappingRecord()->logical_shard
                  << ")";
              return;
            }
            LOG(ERROR) << "Failed to write record to snapshot stream. "
                          "No valid record type specified. ";
          },
      .metadata = options.metadata,
  };
}

template <typename DestStreamT>
absl::StatusOr<std::unique_ptr<RecordAggregator>>
SnapshotStreamWriter<DestStreamT>::CreateRecordAggregator(
    std::string_view temp_data_file) {
  return temp_data_file.empty()
             ? RecordAggregator::CreateInMemoryAggregator()
             : RecordAggregator::CreateFileBackedAggregator(temp_data_file);
}

template <typename DestStreamT>
absl::Status SnapshotStreamWriter<DestStreamT>::InsertOrUpdateRecord(
    const DataRecordT& data_record) {
  if (data_record.record.type == Record::KeyValueMutationRecord) {
    const auto* kv_record = data_record.record.AsKeyValueMutationRecord();
    return record_aggregator_->InsertOrUpdateRecord(
        absl::HashOf(kv_record->key), *kv_record);
  }
  if (data_record.record.type == Record::UserDefinedFunctionsConfig) {
    const auto udf_config = *data_record.record.AsUserDefinedFunctionsConfig();
    if (udf_config_ == nullptr ||
        udf_config_->logical_commit_time < udf_config.logical_commit_time) {
      udf_config_ = std::make_unique<UserDefinedFunctionsConfigT>(udf_config);
    }
    return absl::OkStatus();
  }
  return absl::OkStatus();
}

template <typename DestStreamT>
template <typename SrcStreamT>
absl::Status SnapshotStreamWriter<DestStreamT>::InsertOrUpdateRecords(
    SrcStreamT& src_stream) {
  DeltaRecordStreamReader record_reader(src_stream);
  absl::StatusOr<KVFileMetadata> metadata = record_reader.ReadMetadata();
  if (!metadata.ok()) {
    return metadata.status();
  }
  return record_reader.ReadRecords(
      [this](const DataRecord& data_record) -> absl::Status {
        std::unique_ptr<DataRecordT> data_record_struct(data_record.UnPack());
        return InsertOrUpdateRecord(*data_record_struct);
      });
}

template <typename DestStreamT>
absl::Status SnapshotStreamWriter<DestStreamT>::WriteRecord(
    const DataRecordT& data_record) {
  if (is_finalized_) {
    return absl::FailedPreconditionError(
        "Cannot write records after finalizing the snapshot.");
  }
  return InsertOrUpdateRecord(data_record);
}

template <typename DestStreamT>
template <typename SrcStreamT>
absl::Status SnapshotStreamWriter<DestStreamT>::WriteRecordStream(
    SrcStreamT& src_stream) {
  if (is_finalized_) {
    return absl::FailedPreconditionError(
        "Cannot write records after finalizing the snapshot.");
  }
  return InsertOrUpdateRecords(src_stream);
}

template <typename DestStreamT>
absl::Status SnapshotStreamWriter<DestStreamT>::Finalize() {
  if (is_finalized_) {
    return absl::OkStatus();
  }
  if (absl::Status status = record_aggregator_->ReadRecords(
          [record_writer = record_writer_.get()](
              KeyValueMutationRecordT kv_mutation_record) {
            // By definition, snapshots do NOT contain DELETE mutations.
            if (kv_mutation_record.mutation_type ==
                KeyValueMutationType::Delete) {
              return absl::OkStatus();
            }
            DataRecordT data_record;
            data_record.record.Set(std::move(kv_mutation_record));
            return record_writer->WriteRecord(data_record);
          });
      !status.ok()) {
    return status;
  }
  if (udf_config_ != nullptr) {
    DataRecordT data_record;
    data_record.record.Set(std::move(*udf_config_));
    if (absl::Status status = record_writer_->WriteRecord(data_record);
        !status.ok()) {
      return status;
    }
  }
  if (absl::Status status = record_writer_->Flush(); !status.ok()) {
    return status;
  }
  is_finalized_ = true;
  return absl::OkStatus();
}

template <typename DestStreamT>
absl::Status
SnapshotStreamWriter<DestStreamT>::ValidateRequiredSnapshotMetadata(
    const KVFileMetadata& metadata) {
  if (!metadata.has_snapshot()) {
    return absl::InvalidArgumentError(
        "Snapshot metadata is required for writing snapshots.");
  }
  if (!metadata.snapshot().has_ending_delta_file()) {
    return absl::InvalidArgumentError(
        "Snapshot metadata must contain ending delta filename.");
  }
  if (!IsDeltaFilename(metadata.snapshot().ending_delta_file())) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Snapshot ending delta filename: ",
        metadata.snapshot().ending_delta_file(), "  is not valid."));
  }
  if (!metadata.snapshot().has_starting_file()) {
    return absl::InvalidArgumentError(
        "Snapshot metadata must contain starting filename.");
  }
  if (!IsDeltaFilename(metadata.snapshot().starting_file()) &&
      !IsSnapshotFilename(metadata.snapshot().starting_file())) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Snapshot starting filename: ", metadata.snapshot().starting_file(),
        " must either be a valid delta filename or valid snapshot "
        "filename."));
  }
  return absl::OkStatus();
}
}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_WRITERS_SNAPSHOT_STREAM_WRITER_
