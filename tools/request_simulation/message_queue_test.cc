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

#include "tools/request_simulation/message_queue.h"

#include "gtest/gtest.h"

namespace kv_server {
namespace {

TEST(TestMessageQueue, TestQueueOperation) {
  MessageQueue queue(100);
  // Push first element
  queue.Push("first");
  // Push second element
  queue.Push("second");
  EXPECT_EQ(queue.Size(), 2);
  auto pop = queue.Pop();
  EXPECT_TRUE(pop.ok());
  EXPECT_EQ(pop.value(), "first");
  EXPECT_EQ(queue.Size(), 1);
  EXPECT_FALSE(queue.Empty());
  pop = queue.Pop();
  EXPECT_TRUE(pop.ok());
  EXPECT_EQ(pop.value(), "second");
  EXPECT_TRUE(queue.Empty());
  pop = queue.Pop();
  EXPECT_FALSE(pop.ok());
}

TEST(TestMessageQueue, TestCapacityConstraint) {
  MessageQueue queue(1);
  queue.Push("first");
  queue.Push("second");
  EXPECT_EQ(queue.Size(), 1);
  auto pop = queue.Pop();
  EXPECT_TRUE(pop.ok());
  EXPECT_EQ(pop.value(), "first");
}

}  // namespace
}  // namespace kv_server
