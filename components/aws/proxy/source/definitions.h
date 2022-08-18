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

#ifndef DEFINITIONS_H_
#define DEFINITIONS_H_

#include <sys/types.h>

#include <cstdint>

// Socket connection state
// clang-format off
enum class ConnectionState {
  Unknown,
  Connecting,
  Connected,
  Disconnected
 };
// clang-format on

// Linux socket handle - defined as a specific type for readability on API
typedef int SocketHandle;

// Callback invoked when there is incoming data to the server. Parameters are:
//
//    socket_handle      Socket were data arrived
//    buffer             Buffer received
//    buffer_size        Size of buffer received in bytes
//
// Return value: Return false to indicate that connection must be closed.
typedef bool (*ReceiveCallback)(SocketHandle incoming_socket_handle,
                                char* buffer, size_t buffer_size);

// Callback invoked when a new client connection has been accepted. If
// callback returns false, connection is rejected.Parameters
// are:
//
//    port        TCP port in which this connection was accepted
//    server      Socket handle of the server accepting the connection
//    client      Socket handle of the new client connecting
typedef bool (*NewConnectionCallback)(uint16_t port, SocketHandle server,
                                      SocketHandle client);

// Callback invoked when a client connection has been closed. Parameters
// are:
//
//    socket_handle      Socket handle corresponding to the connection that
//                       has been closed
typedef void (*CloseConnectionCallback)(SocketHandle socket_handle);

#endif  // DEFINITIONS_H_
