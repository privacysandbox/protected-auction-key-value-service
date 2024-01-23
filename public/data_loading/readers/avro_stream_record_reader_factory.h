/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PUBLIC_DATA_LOADING_READERS_AVRO_STREAM_RECORD_READER_FACTORY_H_
#define PUBLIC_DATA_LOADING_READERS_AVRO_STREAM_RECORD_READER_FACTORY_H_

#include <memory>
#include <utility>

#include "public/data_loading/readers/avro_stream_io.h"
#include "public/data_loading/readers/stream_record_reader_factory.h"

namespace kv_server {

// Factory class to create Avro readers. For each input that represents one
// file, one reader should be created.
class AvroStreamRecordReaderFactory : public StreamRecordReaderFactory {
 public:
  explicit AvroStreamRecordReaderFactory(
      AvroConcurrentStreamRecordReader::Options options =
          AvroConcurrentStreamRecordReader::Options())
      : options_(std::move(options)) {}

  std::unique_ptr<StreamRecordReader> CreateReader(
      std::istream& data_input) const override;

  std::unique_ptr<StreamRecordReader> CreateConcurrentReader(
      std::function<std::unique_ptr<RecordStream>()> stream_factory)
      const override;

 private:
  AvroConcurrentStreamRecordReader::Options options_;
};

}  // namespace kv_server

#endif  // PUBLIC_DATA_LOADING_READERS_AVRO_STREAM_RECORD_READER_FACTORY_H_
