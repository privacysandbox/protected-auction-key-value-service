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

#include "public/data_loading/readers/riegeli_stream_record_reader_factory.h"

#include "public/data_loading/readers/riegeli_stream_io.h"

namespace kv_server {

std::unique_ptr<StreamRecordReader>
RiegeliStreamRecordReaderFactory::CreateReader(std::istream& data_input) const {
  return std::make_unique<RiegeliStreamReader<std::string_view>>(
      data_input,
      [log_context = &options_.log_context](
          const riegeli::SkippedRegion& skipped_region,
          riegeli::RecordReaderBase& record_reader) {
        PS_LOG(WARNING, *log_context)
            << "Skipping over corrupted region: " << skipped_region;
        return true;
      },
      options_.log_context);
}

std::unique_ptr<StreamRecordReader>
RiegeliStreamRecordReaderFactory::CreateConcurrentReader(
    std::function<std::unique_ptr<RecordStream>()> stream_factory) const {
  return std::make_unique<ConcurrentStreamRecordReader<std::string_view>>(
      stream_factory, options_);
}
}  // namespace kv_server
