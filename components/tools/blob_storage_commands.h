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

#ifndef COMPONENTS_TOOLS_BLOB_STORAGE_COMMANDS_H_
#define COMPONENTS_TOOLS_BLOB_STORAGE_COMMANDS_H_

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "components/data/blob_storage/blob_storage_client.h"

// This header has platform-independent commands that are used by
// blob_storage_util.
namespace kv_server {
namespace blob_storage_commands {

// Print the contents of blobs to stdout; returns success.
bool CatObjects(std::string bucket_or_directory, std::string prefix,
                absl::Span<char*> keys);

// Delete the blobs; returns success.
bool DeleteObjects(std::string bucket_or_directory, std::string prefix,
                   absl::Span<char*> keys);

// Print to stdout a list of blobs that are in this location; returns success.
bool ListObjects(std::string bucket_or_directory, std::string prefix);

// Create a new BlobReader that will read from either a file or stdin if the
// source is "-".
absl::StatusOr<std::unique_ptr<BlobReader>> GetStdinOrFileSourceStream(
    std::string directory, std::string source);

// Copy data from a reader into an output that is either a file or stdout if the
// dest is "-".  Returns success.
bool CopyFromReaderToStdoutOrFileDestination(std::unique_ptr<BlobReader> reader,
                                             std::string directory,
                                             std::string dest);
}  // namespace blob_storage_commands
}  // namespace kv_server

#endif  // COMPONENTS_TOOLS_BLOB_STORAGE_COMMANDS_H_
