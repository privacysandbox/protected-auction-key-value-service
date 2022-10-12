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

#include <fstream>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "glog/logging.h"
#include "tools/data_cli/commands/command.h"
#include "tools/data_cli/commands/generate_data_command.h"

using fledge::kv_server::Command;
using fledge::kv_server::GenerateDataCommand;

ABSL_FLAG(std::string, input_file, "-", "Input file to convert records from.");
ABSL_FLAG(std::string, input_format, "CSV",
          "Format of the input file. Possible options=(CSV|DELTA)");
ABSL_FLAG(std::string, output_file, "-",
          "Output file to write converted records to.");
ABSL_FLAG(std::string, output_format, "DELTA",
          "Format of output file. Possible options=(CSV|DELTA)");
ABSL_FLAG(std::string, key_namespace, "KEYS",
          "Namespace of the key-value pair");

constexpr std::string_view kUsageMessage = R"(
Usage: data_cli <command> <flags>

Commands:
- generate             Converts input to output type.
    [--input_file]     (Optional) Defaults to stdin. Input file to convert records from.
    [--input_format]   (Optional) Defaults to "CSV". Possible options=(CSV|DELTA)
    [--output_file]    (Optional) Defaults to stdout. Output file to write converted records to.
    [--output_format]  (Optional) Defaults to "DELTA". Possible options=(CSV|DELTA).
    [--key_namespace]  (Optional) Defaults to "KEYS". Used if output_format="delta-file".

Examples:

Convert between file types.
  (1) Generate a csv file to a delta file and write output records to std::cout.
  - data_cli generate --input_file="$PWD/data.csv"

  (2) Generate a delta file back to csv file and write output to a file.
  - data_cli generate --input_file="$PWD/delta" --input_format=DELTA --output_file="$PWD/delta.csv" --output_format=CSV

  (3) Pipe csv records and generate delta file records and write to std cout.
  - cat "$PWD/data.csv" | data_cli generate --input_format=CSV

Try --help to see detailed flag descriptions and associated default values.
)";

constexpr std::string_view kStdioSymbol = "-";
constexpr std::string_view kGenerateDataCommand = "generate";
constexpr std::array<std::string_view, 1> kSupportedCommands = {
    kGenerateDataCommand};

bool IsSupportedCommand(std::string_view command) {
  auto exists =
      std::find(kSupportedCommands.begin(), kSupportedCommands.end(), command);
  return exists != kSupportedCommands.end();
}

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::SetProgramUsageMessage(kUsageMessage);
  const std::vector<char*> commands = absl::ParseCommandLine(argc, argv);
  if (commands.size() < 2) {
    LOG(ERROR) << "You must specify a command.\n"
               << absl::ProgramUsageMessage();
    return -1;
  }
  const std::string command_name = commands[1];
  if (!IsSupportedCommand(commands[1])) {
    LOG(ERROR) << "Command: [" << command_name << "] is not supported.\n"
               << absl::ProgramUsageMessage();
    return -1;
  }
  if (command_name == kGenerateDataCommand) {
    std::ifstream i_fstream(absl::GetFlag(FLAGS_input_file));
    std::istream* i_stream = kStdioSymbol == absl::GetFlag(FLAGS_input_file)
                                 ? &std::cin
                                 : &i_fstream;
    std::ofstream o_fstream(absl::GetFlag(FLAGS_output_file));
    std::ostream* o_stream = kStdioSymbol == absl::GetFlag(FLAGS_output_file)
                                 ? &std::cout
                                 : &o_fstream;
    auto generate_data_command = GenerateDataCommand::Create(
        GenerateDataCommand::Params{
            .input_format = absl::GetFlag(FLAGS_input_format),
            .output_format = absl::GetFlag(FLAGS_output_format),
            .key_namespace = absl::GetFlag(FLAGS_key_namespace)},
        *i_stream, *o_stream);
    if (!generate_data_command.ok()) {
      LOG(ERROR) << "Failed to create command to generate data. "
                 << generate_data_command.status();
      return -1;
    }
    if (absl::Status status = (*generate_data_command)->Execute();
        !status.ok()) {
      LOG(ERROR) << "Failed to generate data. " << status;
      return -1;
    }
  }
  return 0;
}
