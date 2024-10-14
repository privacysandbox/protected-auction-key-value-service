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

#ifndef PUBLIC_CONSTANTS_H_
#define PUBLIC_CONSTANTS_H_

#include <regex>
#include <string_view>

#include "public/base_types.pb.h"

namespace kv_server {

// File basenames of the given type should start with this prefix.
template <FileType::Enum file_type>
constexpr std::string_view FilePrefix() {
  static_assert(file_type != FileType::FILE_TYPE_UNSPECIFIED,
                "Please specify file type");
  return FileType::Enum_Name(file_type);
}

// Used to join components in a file name.
constexpr std::string_view kFileComponentDelimiter = "_";

// Number of digits in logical time in file basename.
constexpr int kLogicalTimeDigits = 16;

// Number of digits used to represent the index of a file in a file group.
constexpr int kFileGroupFileIndexDigits = 5;

// Number of digits used to represent the size of a file group.
constexpr int kFileGroupSizeDigits = 6;

// Minimum size of the returned response in bytes.
constexpr int kMinResponsePaddingBytes = 0;

// "DELTA_\d{16}"
// The first component represents the file type.
//
// The second component represents Logical time which must
// contain 16 digits (To accommodate Unix timestamp with microsecond precision).
// At this time, there is no expectation on the content of the logical time,
// except that a larger number indicates a more recent file.
//
// '_' is used to join components.
//
// For example, "DELTA_1659978505000000"
std::string_view DeltaFileFormatRegex();

// In FLEDGE API, for a list of keys, they can be written as keys=key1,key2
// This requires the server code to parse and the delimiter is documented here.
constexpr char kQueryArgDelimiter = ',';

// Returns a compiled snapshot file name regex defined as follows:
//
// Compiled regex = "SNAPSHOT_\d{16}"
// Regex parts:
// - prefix = "SNAPSHOT"
// - component delimiter = "_"
// - suffix = a 16 digit number that represents logical time. A larger number
//            indicates a more recent snapshot.
const std::regex& SnapshotFileFormatRegex();

// Returns a compiled file group file name regex defined as follows:
//
// Compiled regex = "(DELTA|SNAPSHOT)_\d{16}_\d{5}_OF_\d{6}". Regex parts
// correspond to the following parts:
// NAME REGEX:
//   <FILE_TYPE>_<LOGICAL_TIMESTAMP>_<PART_FILE_INDEX>_OF_<NUM_PART_FILES>
// WHERE:
//   (1) FILE_TYPE - type of file. Valid values: [DELTA, SNAPSHOT].
//   (2) LOGICAL_TIMESTAMP - strictly increasing 16 digit number that represents
//   logical time, a larger value means a more recent file.
//   (3) PART_FILE_INDEX - zero-based 5 digit index of file in the file group.
//   Valid range: [0..NUM_PART_FILES).
//   (4) NUM_PART_FILES = a 6 digit number representing the total number of
//   files in a file group. Valid range: [1..100,000]
const std::regex& FileGroupFilenameFormatRegex();

// X25519 public key used to test/debug/demo the ObliviousGetValues query API.
// ObliviousGetValues requests encrypted with this key can be processed by the
// server.
// For cross code base consistency, matches the test key we use in the common
// repo, see ../encryption/key_fetcher/src/fake_key_fetcher_manager.h
constexpr std::string_view kTestPublicKey =
    "f3b7b2f1764f5c077effecad2afd86154596e63f7375ea522761b881e6c3c323";

// Parameters used to configure Oblivious HTTP according to
// https://github.com/WICG/turtledove/blob/main/FLEDGE_Key_Value_Server_API.md#encryption
//
// KEM: DHKEM(X25519, HKDF-SHA256)
const uint16_t kKEMParameter = 0x0020;
// KDF: HKDF-SHA256
const uint16_t kKDFParameter = 0x0001;
// AEAD: AES-256-GCM
const uint16_t kAEADParameter = 0x0002;

// Custom media types for KV. Used as input for ohttp request/response
// encryption/decryption.
inline constexpr absl::string_view kKVOhttpRequestLabel =
    "message/ad-auction-trusted-signals-request";
inline constexpr absl::string_view kKVOhttpResponseLabel =
    "message/ad-auction-trusted-signals-response";

constexpr std::string_view kServiceName = "kv-server";

// Returns a compiled logical sharding config file name regex defined as
// follows:
//
// Compiled regex = "LOGICAL_SHARDING_CONFIG_\d{16}"
// Regex parts:
// - prefix = "LOGICAL_SHARDING_CONFIG"
// - suffix = a 16 digit number that represents logical time. A larger number
//            indicates a more recent logical sharding config.
const std::regex& LogicalShardingConfigFileFormatRegex();

// Store one of these as parameter to instruct the server to load data files in
// the corresponding format. If none is specified, Riegeli is the default.
enum class FileFormat { kAvro, kRiegeli };
constexpr std::array<std::string_view, 2> kFileFormats{"avro", "riegeli"};

}  // namespace kv_server

#endif  // PUBLIC_CONSTANTS_H_
