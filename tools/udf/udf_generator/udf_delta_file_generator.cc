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

#include <fcntl.h>

#include <fstream>
#include <iostream>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/log.h"
#include "absl/strings/substitute.h"
#include "google/protobuf/text_format.h"
#include "public/constants.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/filename_utils.h"
#include "public/data_loading/records_utils.h"
#include "public/data_loading/writers/avro_delta_record_stream_writer.h"
#include "public/data_loading/writers/delta_record_stream_writer.h"
#include "public/data_loading/writers/delta_record_writer.h"
#include "public/udf/constants.h"
#include "src/util/status_macro/status_macros.h"

ABSL_FLAG(std::string, udf_file_path, "", "UDF file path");
ABSL_FLAG(std::string, udf_handler_name, "HandleRequest", "UDF handler_name");
ABSL_FLAG(std::string, output_dir, "",
          "Output file directory. Ignored if output_path is specified.");
ABSL_FLAG(std::string, output_path, "",
          "Output path. If specified, output_dir is ignored. If '-', output is "
          "written to "
          "console.");
ABSL_FLAG(int64_t, logical_commit_time, absl::ToUnixMicros(absl::Now()),
          "Record logical_commit_time. Default is current timestamp.");
ABSL_FLAG(int64_t, code_snippet_version, 2, "UDF version. Default is 2.");
ABSL_FLAG(std::string, data_loading_file_format,
          std::string(kv_server::kFileFormats[static_cast<int>(
              kv_server::FileFormat::kRiegeli)]),
          "File format of the input data files.");

using kv_server::DataRecordT;
using kv_server::DeltaRecordStreamWriter;
using kv_server::DeltaRecordWriter;
using kv_server::KVFileMetadata;
using kv_server::ToDeltaFileName;
using kv_server::UserDefinedFunctionsConfigT;
using kv_server::UserDefinedFunctionsLanguage;

absl::StatusOr<std::string> ReadCodeSnippetAsString(std::string udf_file_path) {
  std::ifstream ifs(udf_file_path);
  if (!ifs) {
    return absl::NotFoundError(absl::StrCat("File not found: ", udf_file_path));
  }
  std::string udf((std::istreambuf_iterator<char>(ifs)),
                  (std::istreambuf_iterator<char>()));
  return udf;
}

absl::Status WriteUdfConfig(std::ostream* output_stream) {
  if (!*output_stream) {
    return absl::NotFoundError("Invalid output");
  }
  const std::string udf_file_path = absl::GetFlag(FLAGS_udf_file_path);
  const std::string udf_handler_name = absl::GetFlag(FLAGS_udf_handler_name);
  int64_t logical_commit_time = absl::GetFlag(FLAGS_logical_commit_time);
  int64_t version = absl::GetFlag(FLAGS_code_snippet_version);
  absl::StatusOr<std::string> code_snippet =
      ReadCodeSnippetAsString(std::move(udf_file_path));
  if (!code_snippet.ok()) {
    return code_snippet.status();
  }

  KVFileMetadata metadata;
  std::unique_ptr<DeltaRecordWriter> delta_record_writer;
  if (absl::GetFlag(FLAGS_data_loading_file_format) ==
      kv_server::kFileFormats[static_cast<int>(
          kv_server::FileFormat::kRiegeli)]) {
    PS_ASSIGN_OR_RETURN(
        delta_record_writer,
        DeltaRecordStreamWriter<std::ostream>::Create(
            *output_stream, DeltaRecordWriter::Options{.metadata = metadata}));
  } else if (absl::GetFlag(FLAGS_data_loading_file_format) ==
             kv_server::kFileFormats[static_cast<int>(
                 kv_server::FileFormat::kAvro)]) {
    PS_ASSIGN_OR_RETURN(
        delta_record_writer,
        kv_server::AvroDeltaRecordStreamWriter::Create(
            *output_stream, DeltaRecordWriter::Options{.metadata = metadata}));
  }

  UserDefinedFunctionsConfigT udf_config = {
      .language = UserDefinedFunctionsLanguage::Javascript,
      .code_snippet = std::move(*code_snippet),
      .handler_name = std::move(udf_handler_name),
      .logical_commit_time = logical_commit_time,
      .version = version,
  };
  DataRecordT data_record;
  data_record.record.Set(std::move(udf_config));
  PS_RETURN_IF_ERROR(delta_record_writer->WriteRecord(data_record));
  delta_record_writer->Close();
  return absl::OkStatus();
}

absl::StatusOr<std::string> CreateDeltaFileName(std::string_view output_dir) {
  absl::Time now = absl::Now();
  const auto maybe_name = ToDeltaFileName(absl::ToUnixMicros(now));
  if (!maybe_name.ok()) {
    return maybe_name.status();
  }
  return absl::StrCat(output_dir, "/", maybe_name.value());
}

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  const std::string output_path = absl::GetFlag(FLAGS_output_path);
  const std::string output_dir = absl::GetFlag(FLAGS_output_dir);

  if (output_path == "-" || (output_path.empty() && output_dir.empty())) {
    LOG(INFO) << "Writing records to console";
    const auto write_status = WriteUdfConfig(&std::cout);
    if (!write_status.ok()) {
      LOG(ERROR) << "Error writing records: " << write_status;
      return -1;
    }
    return 0;
  }

  std::string outfile;
  if (!output_path.empty()) {
    outfile = output_path;
  } else {
    absl::Time now = absl::Now();
    if (const auto maybe_name = ToDeltaFileName(absl::ToUnixMicros(now));
        !maybe_name.ok()) {
      LOG(ERROR) << "Unable to construct file name: " << maybe_name.status();
      return -1;
    } else {
      outfile = absl::StrCat(output_dir, "/", maybe_name.value());
    }
  }

  LOG(INFO) << "Writing records to " << outfile;
  std::ofstream ofs(outfile);
  const auto write_status = WriteUdfConfig(&ofs);
  ofs.close();
  if (!write_status.ok()) {
    LOG(ERROR) << "Error writing records: " << write_status;
    return -1;
  }
  return 0;
}
