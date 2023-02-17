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

#include "public/data_loading/writers/delta_record_stream_writer.h"

#include "riegeli/bytes/ostream_writer.h"
#include "riegeli/records/record_writer.h"

namespace kv_server {
riegeli::RecordWriterBase::Options GetRecordWriterOptions(
    const DeltaRecordWriter::Options& options) {
  riegeli::RecordWriterBase::Options writer_options;
  if (!options.enable_compression) {
    writer_options.set_uncompressed();
  }
  riegeli::RecordsMetadata metadata;
  *metadata.MutableExtension(kv_server::kv_file_metadata) = options.metadata;
  writer_options.set_metadata(std::move(metadata));
  return writer_options;
}
}  // namespace kv_server
