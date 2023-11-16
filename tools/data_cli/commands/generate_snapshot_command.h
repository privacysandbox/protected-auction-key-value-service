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

#ifndef TOOLS_DATA_CLI_COMMANDS_GENERATE_SNAPSHOT_COMMAND_H_
#define TOOLS_DATA_CLI_COMMANDS_GENERATE_SNAPSHOT_COMMAND_H_

#include <fstream>
#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "components/data/blob_storage/blob_storage_client.h"
#include "public/data_loading/writers/snapshot_stream_writer.h"
#include "tools/data_cli/commands/command.h"

namespace kv_server {

class GenerateSnapshotCommand : public Command {
 public:
  struct Params {
    std::string data_dir;
    std::string working_dir;
    std::string starting_file;
    std::string ending_delta_file;
    std::string snapshot_file;
    bool in_memory_compaction;
    int64_t shard_number = -1;
    int64_t number_of_shards = -1;
  };

  ~GenerateSnapshotCommand();
  GenerateSnapshotCommand(const GenerateSnapshotCommand&) = delete;
  GenerateSnapshotCommand& operator=(const GenerateSnapshotCommand&) = delete;

  static absl::StatusOr<std::unique_ptr<GenerateSnapshotCommand>> Create(
      Params params);
  absl::Status Execute() override;

 private:
  GenerateSnapshotCommand(Params params,
                          std::unique_ptr<BlobStorageClient> blob_client);

  Params params_;
  std::unique_ptr<BlobStorageClient> blob_client_;
};

}  // namespace kv_server

#endif  // TOOLS_DATA_CLI_COMMANDS_GENERATE_SNAPSHOT_COMMAND_H_
