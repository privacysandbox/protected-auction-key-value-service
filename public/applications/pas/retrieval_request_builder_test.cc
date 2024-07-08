/*
 * Copyright 2023 Google LLC
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

#include "public/applications/pas/retrieval_request_builder.h"

#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "public/query/cpp/client_utils.h"
#include "public/test_util/proto_matcher.h"

namespace kv_server::application_pas {
namespace {

using google::protobuf::TextFormat;

TEST(RequestBuilder, Build) {
  privacy_sandbox::server_common::ConsentedDebugConfiguration
      consented_debug_configuration;
  consented_debug_configuration.set_is_consented(true);
  consented_debug_configuration.set_token("test_token");
  privacy_sandbox::server_common::LogContext log_context;
  log_context.set_generation_id("generation_id");
  log_context.set_adtech_debug_id("debug_id");

  v2::GetValuesRequest expected;
  TextFormat::ParseFromString(
      R"pb(
        client_version: "Retrieval.20231018"
        metadata {
          fields {
            key: "is_pas"
            value { string_value: "true" }
          }
        }
        partitions {
          id: 0
          arguments { data { string_value: "protected signals" } }
          arguments {
            data {
              struct_value {
                fields {
                  key: "m1"
                  value { string_value: "v1" }
                }
                fields {
                  key: "m2"
                  value { string_value: "v2" }
                }
                fields {
                  key: "m3"
                  value { string_value: "v3" }
                }
              }
            }
          }
          arguments { data { string_value: "contextual signals" } }
          arguments {
            data {
              list_value {
                values { string_value: "item1" }
                values { string_value: "item2" }
                values { string_value: "item3" }
              }
            }
          }
        }
        log_context {
          generation_id: "generation_id"
          adtech_debug_id: "debug_id"
        }
        consented_debug_config { is_consented: true token: "test_token" })pb",
      &expected);
  EXPECT_THAT(
      BuildRetrievalRequest(log_context, consented_debug_configuration,
                            "protected signals",
                            {{"m1", "v1"}, {"m2", "v2"}, {"m3", "v3"}},
                            "contextual signals", {"item1", "item2", "item3"}),
      EqualsProto(expected));
}

}  // namespace
}  // namespace kv_server::application_pas
