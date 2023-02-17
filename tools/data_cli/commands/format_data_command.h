
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

#ifndef TOOLS_DATA_CLI_COMMANDS_FORMAT_DATA_COMMAND_H_
#define TOOLS_DATA_CLI_COMMANDS_FORMAT_DATA_COMMAND_H_

#include <fstream>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "public/data_loading/readers/delta_record_reader.h"
#include "public/data_loading/writers/delta_record_writer.h"
#include "tools/data_cli/commands/command.h"

namespace kv_server {

// A `FormatDataCommand` command reads input data from an input stream and
// generates output data and writes to an output stream. The format of input
// data is specified by `Params.input_format` and the format of output data is
// specified by `Params.output_format`.
//
// The command can be used as follows:
// ```
// FormatDataCommand::Params params{
//  .input_format = "CSV",
//  .output_format = "DELTA"
// }
// std::istringstream input_stream;
// std::ostringstream output_stream;
// auto command = FormatDataCommand::Create(params, input_stream,
// output_stream);
// if (command.ok() && (*command)->Execute().ok()) {
//  ReadAndUseDeltaStream(output_stream);
// }
// ...
// Note that command only supports "CSV" and "DELTA" formats.
// ```
class FormatDataCommand : public Command {
 public:
  struct Params {
    std::string_view input_format;
    std::string_view output_format;
  };

  static absl::StatusOr<std::unique_ptr<FormatDataCommand>> Create(
      const Params& params, std::istream& input_stream,
      std::ostream& output_stream);
  absl::Status Execute() override;

 private:
  FormatDataCommand(std::unique_ptr<DeltaRecordReader> record_reader,
                    std::unique_ptr<DeltaRecordWriter> record_writer)
      : record_reader_(std::move(record_reader)),
        record_writer_(std::move(record_writer)) {}

  std::unique_ptr<DeltaRecordReader> record_reader_;
  std::unique_ptr<DeltaRecordWriter> record_writer_;
};

}  //  namespace kv_server

#endif  // TOOLS_DATA_CLI_COMMANDS_FORMAT_DATA_COMMAND_H_
