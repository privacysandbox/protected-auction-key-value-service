// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "public/data_loading/readers/avro_stream_record_reader_factory.h"

#include "public/data_loading/readers/avro_stream_io.h"

namespace kv_server {

std::unique_ptr<StreamRecordReader> AvroStreamRecordReaderFactory::CreateReader(
    std::istream& data_input) const {
  return std::make_unique<AvroStreamReader>(data_input, options_.log_context);
}

std::unique_ptr<StreamRecordReader>
AvroStreamRecordReaderFactory::CreateConcurrentReader(
    std::function<std::unique_ptr<RecordStream>()> stream_factory) const {
  return std::make_unique<AvroConcurrentStreamRecordReader>(stream_factory,
                                                            options_);
}

}  // namespace kv_server
