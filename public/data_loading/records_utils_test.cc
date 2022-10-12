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

#include "public/data_loading/records_utils.h"

#include "gtest/gtest.h"

namespace fledge::kv_server {
namespace {

DeltaFileRecordStruct GetDeltaRecord() {
  DeltaFileRecordStruct record;
  record.key = "key";
  record.subkey = "subkey";
  record.value = "value";
  record.logical_commit_time = 1234567890;
  record.mutation_type = DeltaMutationType::Update;
  return record;
}

TEST(DeltaFileRecordStructTest, ValidateEqualsOperator) {
  EXPECT_EQ(GetDeltaRecord(), GetDeltaRecord());
  EXPECT_NE(GetDeltaRecord(), DeltaFileRecordStruct{.key = "key1"});
}

}  // namespace
}  // namespace fledge::kv_server
