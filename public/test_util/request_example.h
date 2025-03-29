/*
 * Copyright 2024 Google LLC
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

#ifndef PUBLIC_TEST_UTIL_REQUEST_EXAMPLE_H_
#define PUBLIC_TEST_UTIL_REQUEST_EXAMPLE_H_

#include <string>

namespace kv_server {

//  Non-consented V2 request example
constexpr std::string_view kExampleV2RequestInJson = R"(
  {
  "metadata": {
      "hostname": "example.com",
      "is_pas": "true"
  },
  "partitions": [
      {
          "id": 0,
          "compressionGroupId": 0,
          "arguments": [
              {
                  "tags": [
                      "structured",
                      "groupNames"
                  ],
                  "data": [
                      "hello"
                  ]
              },
              {
                  "tags": [
                      "custom",
                      "keys"
                  ],
                  "data": [
                      "key1"
                  ]
              }
          ]
      }
  ]
  }
  )";
// Consented V2 request example without log context
constexpr std::string_view kExampleConsentedV2RequestInJson = R"(
  {
  "metadata": {
      "hostname": "example.com",
      "is_pas": "true"
  },
  "partitions": [
      {
          "id": 0,
          "compressionGroupId": 0,
          "arguments": [
              {
                  "tags": [
                      "structured",
                      "groupNames"
                  ],
                  "data": [
                      "hello"
                  ]
              },
              {
                  "tags": [
                      "custom",
                      "keys"
                  ],
                  "data": [
                      "key1"
                  ]
              }
          ]
      }
  ],
  "consented_debug_config": {
        "is_consented": true,
        "token": "debug_token"
  }
  })";

// Consented V2 request example with log context
constexpr std::string_view kExampleConsentedV2RequestWithLogContextInJson = R"(
  {
  "metadata": {
      "hostname": "example.com",
      "is_pas": "true"
  },
  "partitions": [
      {
          "id": 0,
          "compressionGroupId": 0,
          "arguments": [
              {
                  "tags": [
                      "structured",
                      "groupNames"
                  ],
                  "data": [
                      "hello"
                  ]
              },
              {
                  "tags": [
                      "custom",
                      "keys"
                  ],
                  "data": [
                      "key1"
                  ]
              }
          ]
      }
  ],
  "consented_debug_config": {
        "is_consented": true,
        "token": "debug_token"
  },
  "log_context": {
      "generation_id": "client_UUID",
      "adtech_debug_id": "adtech_debug_test"
  }
  })";

//  Non-consented V2 request example with multiple partitions
constexpr std::string_view kV2RequestMultiplePartitionsInJson = R"(
  {
  "metadata": {
      "hostname": "example.com"
  },
  "partitions": [
      {
          "id": 0,
          "compressionGroupId": 0,
          "arguments": [
              {
                  "tags": [
                      "structured",
                      "groupNames"
                  ],
                  "data": [
                      "hello"
                  ]
              }
           ]
      },
      {
          "id": 0,
          "compressionGroupId": 1,
          "arguments": [
              {
                  "tags": [
                      "custom",
                      "keys"
                  ],
                  "data": [
                      "key1"
                  ]
              }
          ]
      },
      {
          "id": 2,
          "compressionGroupId": 0,
          "arguments": [
              {
                  "tags": [
                      "custom",
                      "keys"
                  ],
                  "data": [
                      "key2"
                  ]
              }
          ]
      }
  ]
  }
  )";

// Consented V2 request example with multiple partitions without log context
constexpr std::string_view kConsentedV2RequestMultiplePartitionsInJson =
    R"(
  {
  "metadata": {
      "hostname": "example.com"
  },
  "partitions": [
      {
          "id": 0,
          "compressionGroupId": 0,
          "arguments": [
              {
                  "tags": [
                      "structured",
                      "groupNames"
                  ],
                  "data": [
                      "hello"
                  ]
              }
           ]
      },
      {
          "id": 0,
          "compressionGroupId": 1,
          "arguments": [
              {
                  "tags": [
                      "custom",
                      "keys"
                  ],
                  "data": [
                      "key1"
                  ]
              }
          ]
      },
      {
          "id": 2,
          "compressionGroupId": 0,
          "arguments": [
              {
                  "tags": [
                      "custom",
                      "keys"
                  ],
                  "data": [
                      "key2"
                  ]
              }
          ]
      }
  ],
  "consented_debug_config": {
        "is_consented": true,
        "token": "debug_token"
  }
  })";

// Consented V2 request example with multiple partitions with log context
constexpr std::string_view
    kConsentedV2RequestMultiplePartitionsWithLogContextInJson = R"(
  {
  "metadata": {
      "hostname": "example.com"
  },
  "partitions": [
      {
          "id": 0,
          "compressionGroupId": 0,
          "arguments": [
              {
                  "tags": [
                      "structured",
                      "groupNames"
                  ],
                  "data": [
                      "hello"
                  ]
              }
           ]
      },
      {
          "id": 0,
          "compressionGroupId": 1,
          "arguments": [
              {
                  "tags": [
                      "custom",
                      "keys"
                  ],
                  "data": [
                      "key1"
                  ]
              }
          ]
      },
      {
          "id": 2,
          "compressionGroupId": 0,
          "arguments": [
              {
                  "tags": [
                      "custom",
                      "keys"
                  ],
                  "data": [
                      "key2"
                  ]
              }
          ]
      }
  ],
  "consented_debug_config": {
        "is_consented": true,
        "token": "debug_token"
  },
  "log_context": {
      "generation_id": "client_UUID",
      "adtech_debug_id": "adtech_debug_test"
  }
  })";

// Consented V2 request example with multiple partitions with log context and
// debug info response flag
constexpr std::string_view
    kConsentedV2RequestMultiPartWithDebugInfoResponseInJson =
        R"(
  {
  "metadata": {
      "hostname": "example.com"
  },
  "partitions": [
      {
          "id": 0,
          "compressionGroupId": 0,
          "arguments": [
              {
                  "tags": [
                      "structured",
                      "groupNames"
                  ],
                  "data": [
                      "hello"
                  ]
              }
           ]
      },
      {
          "id": 0,
          "compressionGroupId": 1,
          "arguments": [
              {
                  "tags": [
                      "custom",
                      "keys"
                  ],
                  "data": [
                      "key1"
                  ]
              }
          ]
      },
      {
          "id": 2,
          "compressionGroupId": 0,
          "arguments": [
              {
                  "tags": [
                      "custom",
                      "keys"
                  ],
                  "data": [
                      "key2"
                  ]
              }
          ]
      }
  ],
  "consented_debug_config": {
        "is_consented": true,
        "token": "debug_token",
        "is_debug_info_in_response": true
  },
  "log_context": {
      "generation_id": "client_UUID",
      "adtech_debug_id": "adtech_debug_test"
  }
  })";

// Example consented debug token used in the unit tests
constexpr std::string_view kExampleConsentedDebugToken = "debug_token";

// The following are UDF inputs corresponding to the above
// request examples.

// UDFExecutionMetadata for kExample*V2RequestInJson
constexpr std::string_view kExampleV2RequestUdfMetadata = R"pb(
  request_metadata {
    fields {
      key: "hostname"
      value { string_value: "example.com" }
    }
    fields {
      key: "is_pas"
      value { string_value: "true" }
    }
  })pb";

// UDFArgument 1 for kExample*V2RequestInJson
constexpr std::string_view kExampleV2RequestUdfArg1 = R"pb(
  tags {
    values { string_value: "structured" }
    values { string_value: "groupNames" }
  }
  data { list_value { values { string_value: "hello" } } })pb";

// UDFArgument 2 for kExample*V2RequestInJson
constexpr std::string_view kExampleV2RequestUdfArg2 = R"pb(
  tags {
    values { string_value: "custom" }
    values { string_value: "keys" }
  }
  data { list_value { values { string_value: "key1" } } })pb";

// UDFExecutionMetadata for k*V2RequestMultiplePartitionsInJson
constexpr std::string_view kExampleV2MultiPartitionUdfMetadata = R"pb(
  request_metadata {
    fields {
      key: "hostname"
      value { string_value: "example.com" }
    }
  }
)pb";

// UDFArgument 1 for k*V2RequestMultiplePartitionsInJson
constexpr std::string_view kExampleV2MultiPartitionUdfArg1 = R"pb(
  tags {
    values { string_value: "structured" }
    values { string_value: "groupNames" }
  }
  data { list_value { values { string_value: "hello" } } }
)pb";

// UDFArgument 2 for k*V2RequestMultiplePartitionsInJson
constexpr std::string_view kExampleV2MultiPartitionUdfArg2 = R"pb(
  tags {
    values { string_value: "custom" }
    values { string_value: "keys" }
  }
  data { list_value { values { string_value: "key1" } } }
)pb";

// UDFArgument 3 for k*V2RequestMultiplePartitionsInJson
constexpr std::string_view kExampleV2MultiPartitionUdfArg3 = R"pb(
  tags {
    values { string_value: "custom" }
    values { string_value: "keys" }
  }
  data { list_value { values { string_value: "key2" } } }
)pb";

}  // namespace kv_server

#endif  // PUBLIC_TEST_UTIL_REQUEST_EXAMPLE_H_
