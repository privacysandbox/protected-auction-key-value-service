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
  },
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
          "id": 1,
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
          "id": 1,
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
          "id": 1,
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

// Example consented debug token used in the unit tests
constexpr std::string_view kExampleConsentedDebugToken = "debug_token";

}  // namespace kv_server

#endif  // PUBLIC_TEST_UTIL_REQUEST_EXAMPLE_H_
