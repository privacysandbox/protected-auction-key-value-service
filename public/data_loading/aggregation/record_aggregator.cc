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

#include "public/data_loading/aggregation/record_aggregator.h"

#include <iostream>
#include <sstream>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "glog/logging.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/writers/delta_record_stream_writer.h"
#include "public/data_loading/writers/delta_record_writer.h"

#include "sqlite3.h"

namespace fledge::kv_server {
namespace {

constexpr std::string_view kInMemoryPath = ":memory:";
constexpr std::string_view kCreateTableSql = R"(
  CREATE TABLE IF NOT EXISTS DeltaFileRecords (
      record_key          INTEGER PRIMARY KEY NOT NULL,
      logical_commit_time INTEGER NOT NULL,
      record_blob         BLOB NOT NULL
);)";
constexpr std::string_view kInsertRecordSql = R"(
  INSERT INTO DeltaFileRecords(
      record_key,
      logical_commit_time,
      record_blob
  ) VALUES (
      :record_key,
      :logical_commit_time,
      :record_blob
  ) ON CONFLICT (record_key)
  DO UPDATE SET
      logical_commit_time = :logical_commit_time,
      record_blob = :record_blob
  WHERE logical_commit_time <= :logical_commit_time;
)";
constexpr std::string_view kSelectRecordSql = R"(
  SELECT record_key, record_blob
  FROM DeltaFileRecords
  WHERE record_key = :record_key;
)";
constexpr std::string_view kDeleteAllRecordsSql = R"(
  DELETE FROM DeltaFileRecords;
)";
constexpr std::string_view kDeleteRecordSql = R"(
  DELETE FROM DeltaFileRecords WHERE record_key = '%d';
)";
constexpr std::string_view kBatchSelectRecordsSql = R"(
  SELECT record_key, record_blob
  FROM DeltaFileRecords
  WHERE record_key > :record_key
  ORDER BY record_key
  LIMIT :max_records;
)";

// Maximum number of rows to query at a time in kBatchSelectRecordsSql statement
// above.
constexpr int64_t kRecordsQueryBatchSize = 1000;
// Index of the ":record_key" bind parameter in kInsertRecordSql,
// kSelectRecordSql and kBatchSelectRecordsSql statements above.
constexpr int kRecordKeyBindValueIdx = 1;
// Index of the ":logical_commit_time" bind parameter in kInsertRecordSql
// statement above.
constexpr int kLogicalCommitTimeBindValueIdx = 2;
// Index of the ":record_blob" bind parameter in kInsertRecordSql statement
// above.
constexpr int kRecordBlobBindValueIdx = 3;
// Index of the ":max_records" bind parameter in kBatchSelectRecordsSql
// statement above.
constexpr int kRecordsQueryBatchSizeBindIdx = 2;
// Index of the "record_key" column in kSelectRecordSql and
// kBatchSelectRecordsSql statements above.
constexpr int kRecordKeyResultColumnIdx = 0;
// Index of the "record_blob" column in kSelectRecordSql and
// kBatchSelectRecordsSql statements above.
constexpr int kRecordBlobResultColumnIdx = 1;

struct StmtDeleter {
  void operator()(sqlite3_stmt* stmt) noexcept { sqlite3_finalize(stmt); }
};

struct RecordRow {
  int64_t record_key;
  std::string_view record_blob;
};

class RecordRowCallback {
 public:
  virtual ~RecordRowCallback() = default;
  virtual absl::Status operator()(RecordRow record_row) = 0;
};

class DeltaRecordCallback : public RecordRowCallback {
 public:
  explicit DeltaRecordCallback(
      std::function<const absl::Status(DeltaFileRecordStruct)> delta_callback)
      : delta_callback_(std::move(delta_callback)) {}
  absl::Status operator()(RecordRow record_row) override {
    auto verifier = flatbuffers::Verifier(
        reinterpret_cast<const uint8_t*>(record_row.record_blob.data()),
        record_row.record_blob.size());
    auto record =
        flatbuffers::GetRoot<DeltaFileRecord>(record_row.record_blob.data());
    if (!record->Verify(verifier)) {
      return absl::InvalidArgumentError(
          "Record flatbuffer format is not valid.");
    }
    return delta_callback_(DeltaFileRecordStruct{
        .mutation_type = record->mutation_type(),
        .logical_commit_time = record->logical_commit_time(),
        .key = record->key()->string_view(),
        .subkey = record->subkey()->string_view(),
        .value = record->value()->string_view()});
  }

 private:
  std::function<absl::Status(DeltaFileRecordStruct)> delta_callback_;
};

class ReadRecordsCallback : public RecordRowCallback {
 public:
  explicit ReadRecordsCallback(
      std::function<absl::Status(DeltaFileRecordStruct)> delta_callback)
      : delta_callback_(DeltaRecordCallback(std::move(delta_callback))) {}
  const int64_t& LastProcessedRecordKey() const { return last_processed_key_; }
  absl::Status operator()(RecordRow record_row) {
    last_processed_key_ = record_row.record_key;
    return delta_callback_(std::move(record_row));
  }

 private:
  DeltaRecordCallback delta_callback_;
  int64_t last_processed_key_ = std::numeric_limits<int64_t>::min();
};

std::string GetErrorMessage(sqlite3* db) {
  return absl::StrCat(" error msg: ", sqlite3_errmsg(db),
                      " error code: ", sqlite3_errstr(sqlite3_errcode(db)));
}

absl::Status CreateRecordsTable(sqlite3* db) {
  if (sqlite3_exec(db, kCreateTableSql.data(), /*callback=*/nullptr,
                   /*callback_arg0=*/0, /*errmsg=*/nullptr) != SQLITE_OK) {
    return absl::InternalError(absl::StrCat(
        "Failed to create delta file records table. ", GetErrorMessage(db)));
  }
  return absl::OkStatus();
}

absl::Status ValidateRecord(const DeltaFileRecordStruct& record) {
  if (record.key.empty()) {
    return absl::InvalidArgumentError("Record key must not be empty.");
  }
  if (record.value.empty()) {
    return absl::InvalidArgumentError("Record value must not be empty.");
  }
  return absl::OkStatus();
}

absl::Status PrepareStatement(std::string_view sql_statement,
                              sqlite3_stmt** statement, sqlite3* db) {
  if (int error_code = sqlite3_prepare_v2(db, sql_statement.data(),
                                          sql_statement.size(), &*statement,
                                          /*pzTail=*/nullptr);
      error_code != SQLITE_OK) {
    sqlite3_finalize(*statement);
    return absl::InternalError(
        absl::StrCat("Failed to prepare sql statement: ", sql_statement,
                     " error: ", sqlite3_errstr(error_code)));
  }
  return absl::OkStatus();
}

absl::Status ResetPreparedStatement(sqlite3_stmt* prepared_stmt) {
  if (int error_code = sqlite3_reset(prepared_stmt); error_code != SQLITE_OK) {
    return absl::InternalError(
        absl::StrCat("Failed to reset read records stmt. error: ",
                     sqlite3_errstr(error_code)));
  }
  if (int error_code = sqlite3_clear_bindings(prepared_stmt);
      error_code != SQLITE_OK) {
    return absl::InternalError(
        absl::StrCat("Failed to clear read records stmt bindings. error: ",
                     sqlite3_errstr(error_code)));
  }
  return absl::OkStatus();
}

absl::Status BindBlob(std::string_view blob_bytes, int bind_column_idx,
                      sqlite3_stmt* prepared_stmt) {
  if (int error_code =
          sqlite3_bind_blob(prepared_stmt, bind_column_idx, blob_bytes.data(),
                            blob_bytes.size(), SQLITE_STATIC);
      error_code != SQLITE_OK) {
    return absl::InternalError(absl::StrCat("Failed to bind blob, error: ",
                                            sqlite3_errstr(error_code)));
  }
  return absl::OkStatus();
}

absl::Status BindInt64(int64_t value, int bind_column_idx,
                       sqlite3_stmt* prepared_stmt) {
  if (int error_code =
          sqlite3_bind_int64(prepared_stmt, bind_column_idx, value);
      error_code != SQLITE_OK) {
    return absl::InternalError(absl::StrCat("Failed to bind number. error: ",
                                            sqlite3_errstr(error_code)));
  }
  return absl::OkStatus();
}

absl::StatusOr<int> ProcessRecordRow(RecordRowCallback& row_callback,
                                     sqlite3_stmt* prepared_stmt) {
  int error_code = sqlite3_step(prepared_stmt);
  if (error_code != SQLITE_ROW) {
    return error_code;
  }
  RecordRow record_row;
  record_row.record_key =
      sqlite3_column_int64(prepared_stmt, kRecordKeyResultColumnIdx);
  const char* blob_ptr = (const char*)sqlite3_column_blob(
      prepared_stmt, kRecordBlobResultColumnIdx);
  record_row.record_blob = absl::string_view(
      blob_ptr,
      sqlite3_column_bytes(prepared_stmt, kRecordBlobResultColumnIdx));
  if (absl::Status status = row_callback(record_row); !status.ok()) {
    return status;
  }
  return error_code;
}

}  // namespace

void RecordAggregator::DbDeleter::operator()(sqlite3* db) noexcept {
  sqlite3_close(db);
}

absl::StatusOr<std::unique_ptr<RecordAggregator>>
RecordAggregator::CreateInMemoryAggregator() {
  return CreateFileBackedAggregator(kInMemoryPath);
}

absl::StatusOr<std::unique_ptr<RecordAggregator>>
RecordAggregator::CreateFileBackedAggregator(std::string_view data_file) {
  sqlite3* db;
  DbDeleter db_deleter;
  if (sqlite3_open(data_file.data(), &db) != SQLITE_OK) {
    std::string error_msg = GetErrorMessage(db);
    db_deleter(db);
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to open data file: ", data_file, error_msg));
  }
  auto db_owner =
      std::unique_ptr<sqlite3, DbDeleter>(db, std::move(db_deleter));
  if (absl::Status status = CreateRecordsTable(db_owner.get()); !status.ok()) {
    return status;
  }
  return absl::WrapUnique(new RecordAggregator(std::move(db_owner)));
}

absl::Status RecordAggregator::InsertOrUpdateRecord(
    int64_t record_key, const DeltaFileRecordStruct& record) {
  if (absl::Status status = ValidateRecord(record); !status.ok()) {
    return status;
  }
  sqlite3_stmt* insert_stmt;
  if (absl::Status status =
          PrepareStatement(kInsertRecordSql, &insert_stmt, db_.get());
      !status.ok()) {
    return status;
  }
  auto owned_stmt =
      std::unique_ptr<sqlite3_stmt, StmtDeleter>(insert_stmt, StmtDeleter{});
  if (absl::Status status =
          BindInt64(record_key, kRecordKeyBindValueIdx, owned_stmt.get());
      !status.ok()) {
    return status;
  }
  if (absl::Status status =
          BindInt64(record.logical_commit_time, kLogicalCommitTimeBindValueIdx,
                    owned_stmt.get());
      !status.ok()) {
    return status;
  }
  std::string_view record_blob = ToStringView(record.ToFlatBuffer());
  if (absl::Status status =
          BindBlob(record_blob, kRecordBlobBindValueIdx, owned_stmt.get());
      !status.ok()) {
    return status;
  }
  if (int error_code = sqlite3_step(owned_stmt.get());
      error_code != SQLITE_OK && error_code != SQLITE_DONE) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Failed to insert or replace record: ", sqlite3_errstr(error_code)));
  }
  return absl::OkStatus();
}

absl::Status RecordAggregator::ReadRecord(
    int64_t record_key,
    std::function<absl::Status(DeltaFileRecordStruct)> record_callback) {
  sqlite3_stmt* select_stmt;
  if (absl::Status status =
          PrepareStatement(kSelectRecordSql, &select_stmt, db_.get());
      !status.ok()) {
    return status;
  }
  auto owned_stmt =
      std::unique_ptr<sqlite3_stmt, StmtDeleter>(select_stmt, StmtDeleter{});
  if (absl::Status status =
          BindInt64(record_key, kRecordKeyBindValueIdx, owned_stmt.get());
      !status.ok()) {
    return status;
  }
  DeltaRecordCallback callback(std::move(record_callback));
  auto result = ProcessRecordRow(callback, owned_stmt.get());
  if (!result.ok()) {
    return result.status();
  }
  if (*result == SQLITE_DONE || *result == SQLITE_OK || *result == SQLITE_ROW) {
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Failed to query row for key: ", record_key,
                   " error: ", sqlite3_errstr(*result)));
}

absl::Status RecordAggregator::ReadRecords(
    std::function<absl::Status(DeltaFileRecordStruct)> record_callback) {
  sqlite3_stmt* batch_select_stmt;
  if (absl::Status status = PrepareStatement(kBatchSelectRecordsSql,
                                             &batch_select_stmt, db_.get());
      !status.ok()) {
    return status;
  }
  auto owned_stmt = std::unique_ptr<sqlite3_stmt, StmtDeleter>(
      batch_select_stmt, StmtDeleter{});
  ReadRecordsCallback callback(std::move(record_callback));
  // Loop through all batches of records until we have read all avaiable rows.
  // Each batch has a size of `kRecordsQueryBatchSizeBindIdx` and we know we are
  // done if we process less than `kRecordsQueryBatchSizeBindIdx` in a batch
  // successfuly.
  while (true) {
    if (absl::Status status = ResetPreparedStatement(owned_stmt.get());
        !status.ok()) {
      return status;
    }
    if (absl::Status status =
            BindInt64(callback.LastProcessedRecordKey(), kRecordKeyBindValueIdx,
                      owned_stmt.get());
        !status.ok()) {
      return status;
    }
    if (absl::Status status =
            BindInt64(kRecordsQueryBatchSize, kRecordsQueryBatchSizeBindIdx,
                      owned_stmt.get());
        !status.ok()) {
      return status;
    }
    // Loop through all rows in a batch until we get a SQLITE_DONE or SQLITE_OK
    // meaning that we have finished processing rows.
    int64_t num_processed_records = 0;
    while (true) {
      auto result = ProcessRecordRow(callback, owned_stmt.get());
      if (!result.ok()) {
        return result.status();
      }
      if (*result == SQLITE_ROW) {
        num_processed_records++;
        continue;
      }
      if (*result == SQLITE_DONE || *result == SQLITE_OK) {
        break;
      }
      return absl::InternalError(
          absl::StrCat("Failed to read records ", GetErrorMessage(db_.get())));
    }
    if (num_processed_records < kRecordsQueryBatchSize) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::Status RecordAggregator::DeleteRecord(int64_t record_key) {
  auto sql = absl::StrFormat(kDeleteRecordSql, record_key);
  if (sqlite3_exec(db_.get(), sql.c_str(), /*callback=*/nullptr,
                   /*callback_arg0=*/0, /*errmsg=*/nullptr) != SQLITE_OK) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to delete record: ", GetErrorMessage(db_.get())));
  }
  return absl::OkStatus();
}

absl::Status RecordAggregator::DeleteRecords() {
  if (sqlite3_exec(db_.get(), kDeleteAllRecordsSql.data(),
                   /*callback=*/nullptr,
                   /*callback_arg0=*/0, /*errmsg=*/nullptr) != SQLITE_OK) {
    return absl::InternalError(absl::StrCat("Failed to delete all records: ",
                                            GetErrorMessage(db_.get())));
  }
  return absl::OkStatus();
}

}  // namespace fledge::kv_server
