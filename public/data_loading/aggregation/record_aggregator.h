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

#ifndef PUBLIC_DATA_LOADING_AGGREGATION_RECORD_AGGREGATOR_
#define PUBLIC_DATA_LOADING_AGGREGATION_RECORD_AGGREGATOR_

#include <functional>
#include <istream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "public/data_loading/records_utils.h"

#include "sqlite3.h"

namespace kv_server {
// A `RecordAggregator` aggregates flatbuffer native `KeyValueMutationRecord`
// records added to an aggregator instance from potentially multiple record
// streams. Records can be aggregated by repeatedly calling
// `InsertOrUpdateRecord(...)` as follows:
//
//```
// auto record_aggregator = RecordAggregator::CreateInMemoryAggregator();
// if (!record_aggregator.ok()) {
//   return record_aggregator.status();
// }
// for (auto record_stream : records_streams) {
//   DeltaRecordStreamReader record_reader(record_stream);
//   absl::Status status = record_reader.ReadRecords(
//   [&](const KeyValueMutationRecord& record) {
//      return record_aggregator->InsertOrUpdateRecord(GetRecordKey(record),
//        record);
//   });
//   if (!status.ok()) {
//      HandleError(status);
//   }
// }
//```
class RecordAggregator {
 public:
  ~RecordAggregator() = default;
  RecordAggregator(const RecordAggregator&) = delete;
  RecordAggregator& operator=(const RecordAggregator&) = delete;

  // Creates a `RecordAggregator` that uses RAM to aggregate and store
  // records.
  //
  // Returns a `not ok()` status if creating the aggregator fails.
  static absl::StatusOr<std::unique_ptr<RecordAggregator>>
  CreateInMemoryAggregator();
  // Creates a `RecordAggregator` that uses a user provided backup file to
  // aggregate and store records.
  //
  // Returns a `not ok()` status if creating the aggregator fails.
  static absl::StatusOr<std::unique_ptr<RecordAggregator>>
  CreateFileBackedAggregator(std::string_view data_file);
  // Inserts new record into aggregator or updates the existing record if the
  // record to be inserted is newer than the existing record, otherwise the
  // record is ignored. Newer records are defined as having a larger
  // `logical_commit_time`.
  //
  // Returns:
  // - absl::OkStatus() - if record is inserted, updated or ignored
  // successfully.
  // - !absl::OkStatus() - if there are any errors. The returned status
  // contains a detailed error message.
  absl::Status InsertOrUpdateRecord(int64_t record_key,
                                    const KeyValueMutationRecordT& record);
  // Reads a record keyed by `record_key` and calls the provided
  // `record_callback` function with the record. If no record keyed by
  // `record_key` exists, then `record_callback` is never called.
  //
  // Returns:
  // - absl::OkStatus() - if record is read and processed successfully.
  // - !absl::OkStatus() - if there are any errors. The returned status
  // contains a detailed error message.
  absl::Status ReadRecord(
      int64_t record_key,
      std::function<absl::Status(KeyValueMutationRecordT)> record_callback);
  // Reads all records currently in the aggregator. The `record_callback`
  // function is called exactly once for each record.
  //
  // Returns:
  // - absl::OkStatus() - if all records are read and processed successfully.
  // - !absl::OkStatus() - if there are any errors. The returned status
  // contains a detailed error message.
  absl::Status ReadRecords(
      std::function<absl::Status(KeyValueMutationRecordT)> record_callback);

  // Deletes record keyed by `record_key`. Silently succeeds if the record
  // does not exist.
  //
  // Returns:
  // - absl::OkStatus() - if record is deleted successfully.
  // - !absl::OkStatus() - if there are any errors. The returned status
  // contains a detailed error message.
  absl::Status DeleteRecord(int64_t record_key);
  // Deletes all records in the aggregator.
  //
  // Returns:
  // - absl::OkStatus() - if all records are deleted successfully.
  // - !absl::OkStatus() - if there are any errors. The returned status
  // contains a detailed error message.
  absl::Status DeleteRecords();

 private:
  // Frees up resources associated with the sqlite3 connection object.
  struct DbDeleter {
    // Does not take ownership of the db pointer.
    void operator()(sqlite3* db) noexcept;
  };
  explicit RecordAggregator(std::unique_ptr<sqlite3, DbDeleter> db)
      : db_(std::move(db)) {}

  absl::StatusOr<std::vector<std::string>> MergeSetValueIfRecordExists(
      int64_t record_key, const KeyValueMutationRecordT& record);

  std::unique_ptr<sqlite3, DbDeleter> db_;
};
}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_AGGREGATION_RECORD_AGGREGATOR_
