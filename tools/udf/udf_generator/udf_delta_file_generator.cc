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
#include "absl/strings/substitute.h"
#include "glog/logging.h"
#include "google/protobuf/text_format.h"
#include "public/data_loading/data_loading_generated.h"
#include "public/data_loading/filename_utils.h"
#include "public/data_loading/records_utils.h"
#include "public/data_loading/riegeli_metadata.pb.h"
#include "public/udf/constants.h"
#include "riegeli/bytes/ostream_writer.h"
#include "riegeli/records/record_writer.h"

ABSL_FLAG(std::string, udf_file_path, "", "UDF file path");
ABSL_FLAG(std::string, udf_handler_name, "HandleRequest", "UDF handler_name");
ABSL_FLAG(std::string, output_dir, "",
          "Output file directory. Ignored if output_path is specified.");
ABSL_FLAG(std::string, output_path, "",
          "Output path. If specified, output_dir is ignored. If '-', output is "
          "written to "
          "console.");
ABSL_FLAG(int64_t, timestamp, 123123123,
          "Record timestamp. Default is 123123123.");

using kv_server::DeltaFileRecordStruct;
using kv_server::DeltaMutationType;
using kv_server::kUdfCodeSnippetKey;
using kv_server::kUdfHandlerNameKey;
using kv_server::KVFileMetadata;
using kv_server::ToDeltaFileName;
using kv_server::ToStringView;

absl::StatusOr<std::string> ReadCodeSnippetAsString(std::string udf_file_path) {
  std::ifstream ifs(udf_file_path);
  if (!ifs) {
    return absl::NotFoundError(absl::StrCat("File not found: ", udf_file_path));
  }
  std::string udf((std::istreambuf_iterator<char>(ifs)),
                  (std::istreambuf_iterator<char>()));
  return udf;
}

absl::Status WriteRecord(std::string udf_file_path,
                         std::string_view udf_handler_name,
                         riegeli::RecordWriterBase& writer) {
  const int64_t timestamp = absl::GetFlag(FLAGS_timestamp);
  absl::StatusOr<std::string> code_snippet =
      ReadCodeSnippetAsString(std::move(udf_file_path));
  if (!code_snippet.ok()) {
    return code_snippet.status();
  }

  writer.WriteRecord(ToStringView(
      DeltaFileRecordStruct{DeltaMutationType::Update, timestamp,
                            kUdfCodeSnippetKey, std::move(code_snippet.value())}
          .ToFlatBuffer()));
  writer.WriteRecord(
      ToStringView(DeltaFileRecordStruct{DeltaMutationType::Update, timestamp,
                                         kUdfHandlerNameKey, udf_handler_name}
                       .ToFlatBuffer()));
  LOG(INFO) << "write done";
  return absl::OkStatus();
}

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  const std::string output_path = absl::GetFlag(FLAGS_output_path);
  const std::string output_dir = absl::GetFlag(FLAGS_output_dir);

  auto write_records = [](std::ostream* os) {
    if (!*os) {
      return absl::NotFoundError("Invalid output path");
    }
    const std::string udf_file_path = absl::GetFlag(FLAGS_udf_file_path);
    const std::string udf_handler_name = absl::GetFlag(FLAGS_udf_handler_name);

    auto os_writer = riegeli::OStreamWriter(os);
    riegeli::RecordWriterBase::Options options;
    options.set_uncompressed();
    riegeli::RecordsMetadata metadata;
    KVFileMetadata file_metadata;
    *metadata.MutableExtension(kv_server::kv_file_metadata) = file_metadata;
    options.set_metadata(std::move(metadata));
    auto record_writer = riegeli::RecordWriter(std::move(os_writer), options);
    const auto write_status =
        WriteRecord(std::move(udf_file_path), udf_handler_name, record_writer);
    record_writer.Close();
    return write_status;
  };

  absl::Status write_status;
  if (output_path == "-" || (output_path.empty() && output_dir.empty())) {
    LOG(INFO) << "Writing records to console";
    write_status = write_records(&std::cout);
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
  write_status = write_records(&ofs);
  ofs.close();
  if (!write_status.ok()) {
    LOG(ERROR) << "Error writing records: " << write_status;
    return -1;
  }
  return 0;
}
