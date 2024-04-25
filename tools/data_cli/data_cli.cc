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
#include "absl/log/flags.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "components/util/platform_initializer.h"
#include "tools/data_cli/commands/command.h"
#include "tools/data_cli/commands/format_data_command.h"
#include "tools/data_cli/commands/generate_snapshot_command.h"

using kv_server::FormatDataCommand;
using kv_server::GenerateSnapshotCommand;

ABSL_FLAG(std::string, input_file, "-", "Input file to convert records from.");
ABSL_FLAG(std::string, input_format, "CSV",
          "Format of the input file. Possible options=(CSV|DELTA)");
ABSL_FLAG(std::string, output_file, "-",
          "Output file to write converted records to.");
ABSL_FLAG(std::string, output_format, "DELTA",
          "Format of output file. Possible options=(CSV|DELTA)");
ABSL_FLAG(std::string, starting_file, "",
          "Name of the file to use as the starting file for a snapshot, it can "
          "be an existing snapshot or delta file.");
ABSL_FLAG(std::string, ending_delta_file, "",
          "Name of the most recent delta file to be included in the snapshot.");
ABSL_FLAG(std::string, working_dir, "/tmp",
          "Directory to save temp data files.");
ABSL_FLAG(std::string, snapshot_file, "-",
          "Output file to write snapshot to. Defaults to stdout.");
ABSL_FLAG(
    std::string, data_dir, "",
    "Data directory with files to be combined into a snapshot. This can be "
    "an AWS S3 bucket or local directory.");
ABSL_FLAG(
    bool, in_memory_compaction, true,
    "If true, delta file compaction to generate snapshots is done in memory.");
ABSL_FLAG(std::string, csv_column_delimiter, ",",
          "Column delimiter for csv files");
ABSL_FLAG(std::string, csv_value_delimiter, "|",
          "Value delimiter for csv files");
ABSL_FLAG(std::string, record_type, "key_value_mutation_record",
          "Data record type. Possible "
          "options=(KEY_VALUE_MUTATION_RECORD|USER_DEFINED_FUNCTIONS_CONFIG|"
          "SHARD_MAPPING_RECORD).");
ABSL_FLAG(std::string, csv_encoding, "plaintext",
          "Encoding for KEY_VALUE_MUTATION_RECORD values for "
          "CSVs. options=(PLAINTEXT|BASE64)."
          "If the values are binary, BASE64 is recommended.");
ABSL_FLAG(int64_t, shard_number, -1,
          "The shard number for output DELTA or SNAPSHOT files.");
ABSL_FLAG(
    int64_t, number_of_shards, -1,
    "Total number of shards. Must be > --shard_number if shard_number >= 0.");

constexpr std::string_view kUsageMessage = R"(
Usage: data_cli <command> <flags>

Commands:
- format_data          Converts input to output format.
    [--input_file]       (Optional) Defaults to stdin. Input file to convert records from.
    [--input_format]     (Optional) Defaults to "CSV". Possible options=(CSV|DELTA)
    [--output_file]      (Optional) Defaults to stdout. Output file to write converted records to.
    [--output_format]    (Optional) Defaults to "DELTA". Possible options=(CSV|DELTA).
    [--record_type]      (Optional) Defaults to "KEY_VALUE_MUTATION_RECORD". Possible
                                  options=(KEY_VALUE_MUTATION_RECORD|USER_DEFINED_FUNCTIONS_CONFIG|SHARD_MAPPING_RECORD).
                                  If reading/writing a UDF config, use "USER_DEFINED_FUNCTIONS_CONFIG".
                                  If reading/writing a shard mapping config, use "SHARD_MAPPING_RECORD".
    [--csv_encoding]     (Optional) Defaults to "PLAINTEXT". Encoding for KEY_VALUE_MUTATION_RECORD values for CSVs.
                                  Possible options=(PLAINTEXT|BASE64).
                                  If the values are binary, BASE64 is recommended.
    [--shard_number]     (Optional) Defaults to -1 (i.e., not specified).
    [--number_of_shards] (Optional) Defaults to -1 (i.e., not specified). Must be > --shard_number if shard_number >= 0.
  Examples:
    (1) Generate a csv file to a delta file and write output records to std::cout.
    - data_cli format_data --input_file="$PWD/data.csv"

    (2) Generate a delta file back to csv file and write output to a file.
    - data_cli format_data --input_file="$PWD/delta" --input_format=DELTA --output_file="$PWD/delta.csv" --output_format=CSV

    (3) Pipe csv records and generate delta file records and write to std cout.
    - cat "$PWD/data.csv" | data_cli format_data --input_format=CSV

    (4) Generate a delta file with UDF configs back to csv file and write output to a file.
    - data_cli format_data --input_file="$PWD/delta" --input_format=DELTA --output_file="$PWD/delta.csv" --output_format=CSV \
         --record_type=USER_DEFINED_FUNCTIONS_CONFIG

- generate_snapshot             Compacts a range of delta files into a single snapshot file.
    [--starting_file]           (Required) Oldest delta file or base snapshot to include in compaction.
    [--ending_delta_file]       (Required) Most recent delta file to include compaction.
    [--snapshot_file]           (Optional) Defaults to stdout. Output snapshot file.
    [--data_dir]                (Required) Directory with input delta files.
    [--working_dir]             (Optional) Defaults to "/tmp". Directory used to write temporary data.
    [--in_memory_compaction]    (Optional) Defaults to true. If false, file backed compaction is used.
    [--shard_number]            (Optional) Defaults to -1 (i.e., not specified).
    [--number_of_shards]        (Optional) Defaults to -1 (i.e., not specified). Must be > --shard_number if shard_number >= 0.
  Examples:
    (1) Generate snapshot using delta files from local disk.
    - data_cli generate_snapshot --data_dir="$DATA_DIR" --starting_file="DELTA_1670532228628680" \
        --ending_delta_file="DELTA_1670532717393878" --snapshot_file="SNAPSHOT_0000000000000003"

Try --help to see detailed flag descriptions and associated default values.
)";

constexpr std::string_view kStdioSymbol = "-";
constexpr std::string_view kFormatDataCommand = "format_data";
constexpr std::string_view kGenerateSnapshotCommand = "generate_snapshot";
constexpr std::array kSupportedCommands = {
    kFormatDataCommand,
    kGenerateSnapshotCommand,
};

bool IsSupportedCommand(std::string_view command) {
  auto exists =
      std::find(kSupportedCommands.begin(), kSupportedCommands.end(), command);
  return exists != kSupportedCommands.end();
}

// Sample run using bazel:
//
//   bazel run \
//   //tools/data_cli:data_cli \
//   --config=local_instance --config=local_platform -- \
//   format_data \
//    --input_file=/data/DELTA_1689344645643610 \
//    --input_format=DELTA \
//    --output_format=CSV \
//    --output_file=/data/DELTA_1689344645643610.csv \
//    --v=3 --stderrthreshold=0
int main(int argc, char** argv) {
  kv_server::PlatformInitializer initializer;

  absl::InitializeLog();
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
  if (command_name == kFormatDataCommand) {
    std::ifstream i_fstream(absl::GetFlag(FLAGS_input_file));
    std::istream* i_stream = kStdioSymbol == absl::GetFlag(FLAGS_input_file)
                                 ? &std::cin
                                 : &i_fstream;
    std::ofstream o_fstream(absl::GetFlag(FLAGS_output_file));
    std::ostream* o_stream = kStdioSymbol == absl::GetFlag(FLAGS_output_file)
                                 ? &std::cout
                                 : &o_fstream;
    auto format_data_command = FormatDataCommand::Create(
        FormatDataCommand::Params{
            .input_format = absl::GetFlag(FLAGS_input_format),
            .output_format = absl::GetFlag(FLAGS_output_format),
            .csv_column_delimiter =
                absl::GetFlag(FLAGS_csv_column_delimiter)[0],
            .csv_value_delimiter = absl::GetFlag(FLAGS_csv_value_delimiter)[0],
            .record_type = absl::GetFlag(FLAGS_record_type),
            .csv_encoding = absl::GetFlag(FLAGS_csv_encoding),
            .shard_number = absl::GetFlag(FLAGS_shard_number),
            .number_of_shards = absl::GetFlag(FLAGS_number_of_shards),
        },
        *i_stream, *o_stream);
    if (!format_data_command.ok()) {
      LOG(ERROR) << "Failed to create command to format data. "
                 << format_data_command.status();
      return -1;
    }
    if (absl::Status status = (*format_data_command)->Execute(); !status.ok()) {
      LOG(ERROR) << "Failed to execute format data command. " << status;
      return -1;
    }
  }
  if (command_name == kGenerateSnapshotCommand) {
    auto generate_snapshot_command =
        GenerateSnapshotCommand::Create(GenerateSnapshotCommand::Params{
            .data_dir = absl::GetFlag(FLAGS_data_dir),
            .working_dir = absl::GetFlag(FLAGS_working_dir),
            .starting_file = absl::GetFlag(FLAGS_starting_file),
            .ending_delta_file = absl::GetFlag(FLAGS_ending_delta_file),
            .snapshot_file = absl::GetFlag(FLAGS_snapshot_file),
            .in_memory_compaction = absl::GetFlag(FLAGS_in_memory_compaction),
            .shard_number = absl::GetFlag(FLAGS_shard_number),
            .number_of_shards = absl::GetFlag(FLAGS_number_of_shards),
        });
    if (!generate_snapshot_command.ok()) {
      LOG(ERROR) << "Failed to create command to generate snapshot. "
                 << generate_snapshot_command.status();
      return -1;
    }
    if (absl::Status status = (*generate_snapshot_command)->Execute();
        !status.ok()) {
      LOG(ERROR) << "Failed to execute generate snapshot command. " << status;
      return -1;
    }
  }
  return 0;
}
