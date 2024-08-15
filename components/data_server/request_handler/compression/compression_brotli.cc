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
#include "components/data_server/request_handler/compression/compression_brotli.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_join.h"
#include "brotli/decode.h"
#include "brotli/encode.h"
#include "quiche/common/quiche_data_writer.h"

namespace kv_server {

namespace {

// Responsible for compressing one compression group.
absl::StatusOr<std::string> CompressOnePartition(std::string_view partition) {
  VLOG(5) << "Compressing " << partition;
  size_t buffer_size = BrotliEncoderMaxCompressedSize(partition.size());
  // The output consists of the size of the compressed data and the compressed
  // data
  std::string partition_output(sizeof(uint32_t) + buffer_size, '\0');

  if (auto rc = BrotliEncoderCompress(
          /*quality=*/BROTLI_DEFAULT_QUALITY,
          /*lgwin=*/BROTLI_DEFAULT_WINDOW,
          /*mode=*/BROTLI_DEFAULT_MODE,
          /*input_size=*/partition.size(),
          /*input_buffer=*/
          reinterpret_cast<const uint8_t*>(partition.data()),
          /*encoded_size=*/&buffer_size,
          /*encoded_buffer=*/
          reinterpret_cast<uint8_t*>(&partition_output.at(sizeof(uint32_t))));
      rc == BROTLI_FALSE) {
    return absl::InternalError(absl::StrCat("Brotli failed to compress"));
  }
  // Shrink the output to only what's used by Brotli.
  // TODO(b/278269394): For compression groups, if the compressed value of one
  // group > original value, we should not do compression. But currently we
  // can only use one compression algo for all groups. So we can't simply stop
  // using the algo for one group in that case.
  partition_output.resize(sizeof(uint32_t) + buffer_size);
  quiche::QuicheDataWriter data_writer(sizeof(uint32_t),
                                       partition_output.data());
  // Rewrite the data size to be the real encoded size
  data_writer.WriteUInt32(buffer_size);
  VLOG(5) << "partition output size: " << partition_output.size();
  return partition_output;
}

class BrotliDecoder {
 public:
  // Owns the decoder memory
  explicit BrotliDecoder(BrotliDecoderState* decoder) : decoder_(decoder) {}
  static absl::StatusOr<std::unique_ptr<BrotliDecoder>> Create() {
    BrotliDecoderState* decoder =
        BrotliDecoderCreateInstance(/* alloc_func= */ nullptr,
                                    /* free_func= */ nullptr,
                                    /* opaque= */ nullptr);
    if (!decoder) {
      return absl::InternalError("Brotli decoder cannot be initialized");
    }

    BrotliDecoderSetParameter(
        decoder, BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION, 1u);
    return std::make_unique<BrotliDecoder>(decoder);
  }

  BrotliDecoder(const BrotliDecoder&) = delete;
  BrotliDecoder& operator=(const BrotliDecoder&) = delete;
  BrotliDecoder(BrotliDecoder&&) = default;

  ~BrotliDecoder() {
    BrotliDecoderDestroyInstance(decoder_);
    decoder_ = nullptr;
  }

  absl::StatusOr<std::string> Decode(quiche::QuicheDataReader& data_reader) {
    std::vector<std::string> outputs;

    while (true) {
      if (BrotliDecoderHasMoreOutput(decoder_)) {
        // Copy the output to our own buffer. We can't let Brotli directly write
        // to our own buffer because we don't know how big the output size is
        size_t data_size = 0;
        const uint8_t* data = BrotliDecoderTakeOutput(decoder_, &data_size);
        outputs.push_back(
            std::string(reinterpret_cast<const char*>(data), data_size));
        VLOG(5) << "outputs size: " << outputs.size()
                << "new item size: " << data_size;
        continue;
      }

      // Decoder reports that stream is over; won't accept/produce more bytes.
      if (BrotliDecoderIsFinished(decoder_) == BROTLI_TRUE) {
        // If input isn't fully consumed and Brotli stops accepting, there is a
        // problem with the input.
        if (!data_reader.IsDoneReading()) {
          return absl::DataLossError("corrupted (exuberant) input");
        } else {
          break;
        }
      }

      if (data_reader.IsDoneReading()) {
        // If input is fully consumed but Brotli still needs more data, there is
        // a problem with the input.
        return absl::DataLossError("corrupted (truncated) input");
      }

      // Give the remaining data to Brotli
      absl::string_view chunk = data_reader.PeekRemainingPayload();
      const uint8_t* next_in = reinterpret_cast<const uint8_t*>(chunk.data());
      size_t available_in = chunk.size();
      size_t available_out = 0;
      BrotliDecoderResult result = BrotliDecoderDecompressStream(
          decoder_, &available_in, &next_in, &available_out, nullptr, nullptr);
      // Advance by the amount that Brotli has consumed
      data_reader.Seek(chunk.size() - available_in);
      VLOG(6) << "chunk size: " << chunk.size()
              << "available_in: " << available_in;

      switch (result) {
        case BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT:
          ABSL_FALLTHROUGH_INTENDED;

        case BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT:
          ABSL_FALLTHROUGH_INTENDED;

        case BROTLI_DECODER_RESULT_SUCCESS:
          // Will be addressed in the next iteration.
          break;

        case BROTLI_DECODER_RESULT_ERROR:
          return absl::DataLossError(absl::StrCat(
              "corrupted input: ",
              BrotliDecoderErrorString(BrotliDecoderGetErrorCode(decoder_))));

        default:
          return absl::InternalError(
              absl::StrCat("unexpected brotli decoder result: ", result));
      }
    }
    return absl::StrJoin(outputs, "");
  }

 private:
  BrotliDecoderState* decoder_;
};

}  // namespace

absl::StatusOr<std::string> BrotliCompressionGroupConcatenator::Build() const {
  std::vector<std::string> compression_groups;
  // Go through every partition to compress them one by one.
  for (const auto& partition : Partitions()) {
    if (auto maybe_partition_output = CompressOnePartition(partition);
        !maybe_partition_output.ok()) {
      return maybe_partition_output.status();
    } else {
      compression_groups.push_back(std::move(maybe_partition_output).value());
    }
  }
  return absl::StrJoin(compression_groups, "");
}

absl::StatusOr<std::string>
BrotliCompressionBlobReader::ExtractOneCompressionGroup() {
  uint32_t compression_group_size = 0;
  if (!data_reader_.ReadUInt32(&compression_group_size)) {
    return absl::InvalidArgumentError("Failed to read compression group size");
  }
  VLOG(9) << "compression_group_size: " << compression_group_size;
  std::string_view compressed_data;
  if (!data_reader_.ReadStringPiece(&compressed_data, compression_group_size)) {
    return absl::InvalidArgumentError("Failed to read compression group");
  }

  quiche::QuicheDataReader compression_group_buffer_reader(compressed_data);

  if (auto maybe_brotli_decoder = BrotliDecoder::Create();
      !maybe_brotli_decoder.ok()) {
    return maybe_brotli_decoder.status();
  } else {
    return maybe_brotli_decoder.value()->Decode(
        compression_group_buffer_reader);
  }
}

}  // namespace kv_server
