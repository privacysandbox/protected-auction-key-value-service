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

#include "tools/data_cli/commands/format_data_command.h"

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "public/data_loading/csv/csv_delta_record_stream_reader.h"
#include "public/data_loading/csv/csv_delta_record_stream_writer.h"
#include "public/data_loading/readers/avro_delta_record_stream_reader.h"
#include "public/data_loading/readers/delta_record_stream_reader.h"
#include "public/data_loading/writers/avro_delta_record_stream_writer.h"
#include "public/data_loading/writers/delta_record_stream_writer.h"
#include "public/sharding/sharding_function.h"
#include "src/util/status_macro/status_macros.h"

namespace kv_server {
namespace {

constexpr std::string_view kDeltaFormat = "delta";
constexpr std::string_view kCsvFormat = "csv";
constexpr std::string_view kAvroFormat = "avro_delta";
constexpr std::string_view kKeyValueMutationRecord =
    "key_value_mutation_record";
constexpr std::string_view kUserDefinedFunctionsConfig =
    "user_defined_functions_config";
constexpr std::string_view kEncodingPlaintext = "plaintext";
constexpr std::string_view kEncodingBase64 = "base64";
constexpr double kSamplingThreshold = 0.02;
constexpr std::string_view kShardMappingRecord = "shard_mapping_record";

absl::Status ValidateParams(const FormatDataCommand::Params& params) {
  if (params.input_format.empty()) {
    return absl::InvalidArgumentError("Input format cannot be empty.");
  }
  if (params.output_format.empty()) {
    return absl::InvalidArgumentError("Output format cannot be empty.");
  }
  if (params.record_type.empty()) {
    return absl::InvalidArgumentError("Record type cannot be empty.");
  }
  std::string lw_output_format = absl::AsciiStrToLower(params.output_format);
  if (absl::AsciiStrToLower(params.input_format) == lw_output_format) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Input and output format must be different. Input format: ",
        params.input_format, " Output format: ", params.output_format));
  }
  if (params.shard_number >= 0 &&
      params.number_of_shards <= params.shard_number) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Shard metadata is invalid. shard_number is ", params.shard_number,
        " and number_of_shards is ", params.number_of_shards,
        ". Valid inputs must satisfy the requirement: 0 <= shard_number < "
        "number_of_shards"));
  }
  return absl::OkStatus();
}

absl::StatusOr<DataRecordType> GetRecordType(std::string_view record_type) {
  std::string lw_record_type = absl::AsciiStrToLower(record_type);
  if (lw_record_type == kKeyValueMutationRecord) {
    return DataRecordType::kKeyValueMutationRecord;
  }
  if (lw_record_type == kUserDefinedFunctionsConfig) {
    return DataRecordType::kUserDefinedFunctionsConfig;
  }
  if (lw_record_type == kShardMappingRecord) {
    return DataRecordType::kShardMappingRecord;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Record type ", record_type, " is not supported."));
}

absl::StatusOr<Record> GetRecordKind(std::string_view record_type) {
  std::string lw_record_type = absl::AsciiStrToLower(record_type);
  if (lw_record_type == kKeyValueMutationRecord) {
    return Record::KeyValueMutationRecord;
  }
  if (lw_record_type == kUserDefinedFunctionsConfig) {
    return Record::UserDefinedFunctionsConfig;
  }
  if (lw_record_type == kShardMappingRecord) {
    return Record::ShardMappingRecord;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Record type ", record_type, " is not supported."));
}

absl::StatusOr<CsvEncoding> GetCsvEncoding(std::string_view csv_encoding) {
  std::string lw_csv_encoding = absl::AsciiStrToLower(csv_encoding);
  if (lw_csv_encoding == kEncodingPlaintext) {
    return CsvEncoding::kPlaintext;
  }
  if (lw_csv_encoding == kEncodingBase64) {
    return CsvEncoding::kBase64;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Csv encoding ", csv_encoding, " is not supported."));
}

absl::StatusOr<std::unique_ptr<DeltaRecordReader>> CreateRecordReader(
    const FormatDataCommand::Params& params, std::istream& input_stream) {
  std::string lw_input_format = absl::AsciiStrToLower(params.input_format);
  if (lw_input_format == kCsvFormat) {
    PS_ASSIGN_OR_RETURN(auto record_type, GetRecordKind(params.record_type));
    PS_ASSIGN_OR_RETURN(auto csv_encoding, GetCsvEncoding(params.csv_encoding));
    return std::make_unique<CsvDeltaRecordStreamReader<std::istream>>(
        input_stream, CsvDeltaRecordStreamReader<std::istream>::Options{
                          .field_separator = params.csv_column_delimiter,
                          .value_separator = params.csv_value_delimiter,
                          .record_type = std::move(record_type),
                          .csv_encoding = std::move(csv_encoding),
                      });
  }
  if (lw_input_format == kDeltaFormat) {
    return std::make_unique<DeltaRecordStreamReader<std::istream>>(
        input_stream);
  }
  if (lw_input_format == kAvroFormat) {
    return std::make_unique<AvroDeltaRecordStreamReader<std::istream>>(
        input_stream);
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "Input format: ", params.input_format, " is not supported."));
}

absl::StatusOr<std::unique_ptr<DeltaRecordWriter>> CreateRecordWriter(
    const FormatDataCommand::Params& params, std::ostream& output_stream) {
  std::string lw_output_format = absl::AsciiStrToLower(params.output_format);
  if (lw_output_format == kCsvFormat) {
    PS_ASSIGN_OR_RETURN(auto record_type, GetRecordType(params.record_type));
    PS_ASSIGN_OR_RETURN(auto csv_encoding, GetCsvEncoding(params.csv_encoding));
    return std::make_unique<CsvDeltaRecordStreamWriter<std::ostream>>(
        output_stream, CsvDeltaRecordStreamWriter<std::ostream>::Options{
                           .field_separator = params.csv_column_delimiter,
                           .value_separator = params.csv_value_delimiter,
                           .record_type = std::move(record_type),
                           .csv_encoding = std::move(csv_encoding),
                       });
  }
  KVFileMetadata metadata;
  if (params.shard_number >= 0) {
    auto* shard_metadata = metadata.mutable_sharding_metadata();
    shard_metadata->set_shard_num(params.shard_number);
  }
  if (lw_output_format == kDeltaFormat) {
    return DeltaRecordStreamWriter<std::ostream>::Create(
        output_stream, DeltaRecordWriter::Options{.metadata = metadata});
  }
  if (lw_output_format == kAvroFormat) {
    return AvroDeltaRecordStreamWriter::Create(
        output_stream, DeltaRecordWriter::Options{.metadata = metadata});
  }

  return absl::InvalidArgumentError(absl::StrCat(
      "Output format: ", params.output_format, " is not supported."));
}

}  // namespace

absl::StatusOr<std::unique_ptr<FormatDataCommand>> FormatDataCommand::Create(
    Params params, std::istream& input_stream, std::ostream& output_stream) {
  if (absl::Status status = ValidateParams(params); !status.ok()) {
    return status;
  }
  auto record_reader = CreateRecordReader(params, input_stream);
  if (!record_reader.ok()) {
    return record_reader.status();
  }
  auto record_writer = CreateRecordWriter(params, output_stream);
  if (!record_writer.ok()) {
    return record_writer.status();
  }
  return absl::WrapUnique(new FormatDataCommand(
      std::move(*record_reader), std::move(*record_writer), params));
}

absl::Status FormatDataCommand::Execute() {
  LOG(INFO) << "Formatting records ...";
  int64_t records_count = 0;
  ShardingFunction sharding_function(/*seed=*/"");
  absl::Status status =
      record_reader_->ReadRecords([&records_count, &sharding_function,
                                   this](const DataRecord& data_record) {
        if (params_.shard_number >= 0 &&
            data_record.record_type() == Record::KeyValueMutationRecord) {
          auto record_shard_num = sharding_function.GetShardNumForKey(
              data_record.record_as_KeyValueMutationRecord()
                  ->key()
                  ->string_view(),
              params_.number_of_shards);
          if (params_.shard_number != record_shard_num) {
            LOG(INFO) << "Skipping record with key: "
                      << data_record.record_as_KeyValueMutationRecord()
                             ->key()
                             ->string_view()
                      << " . The record belongs to shard: " << record_shard_num
                      << ", but shard_number is " << params_.shard_number;
            return absl::OkStatus();
          }
        }
        records_count++;
        if ((double)std::rand() / RAND_MAX <= kSamplingThreshold) {
          LOG(INFO) << "Formatting record: " << records_count;
        }
        std::unique_ptr<DataRecordT> data_record_native(data_record.UnPack());
        auto status = record_writer_->WriteRecord(*data_record_native);
        if (!status.ok()) {
          LOG(ERROR) << "Failed to write record: " << status;
        }
        return status;
      });
  record_writer_->Close();
  if (status.ok()) {
    LOG(INFO) << "Sucessfully formated records.";
  } else {
    LOG(ERROR) << "Failed to format records: " << status;
  }
  return status;
}

}  //  namespace kv_server
