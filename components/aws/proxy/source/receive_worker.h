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

#ifndef COMPONENTS_AWS_PROXY_SOURCE_RECEIVE_WORKER
#define COMPONENTS_AWS_PROXY_SOURCE_RECEIVE_WORKER

#include <memory>

#include "components/aws/proxy/source/definitions.h"
#include "components/aws/proxy/source/socks5_state.h"

// The thread worker for reading from client, handling handshake, and forwarding
// traffic to dest hosts.
void Socks5Worker(SocketHandle client_sock, size_t buffer_size);

// The thread worker for forwarding traffic from dest to client.
void DestToClientForwarder(const std::shared_ptr<Socks5State>& socks5state,
                           size_t buffer_size);
#endif  // COMPONENTS_AWS_PROXY_SOURCE_RECEIVE_WORKER
