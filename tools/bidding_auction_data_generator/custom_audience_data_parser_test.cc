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

#include "tools/bidding_auction_data_generator/custom_audience_data_parser.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

TEST(CustomAudienceDataLoadingTest, ParseJsonDataTest) {
  const std::string json_input = R"({
   "interestGroups":[
      {
         "name":"group1",
         "ads":[
            {
               "renderUrl":"url1",
               "metadata":[
               ]
            },
            {
               "renderUrl":"url2",
               "metadata":[
               ]
            }
          ]
    },
    {
         "name":"group2",
         "ads":[
            {
               "renderUrl":"url2",
               "metadata":[
               ]
            },
            {
               "renderUrl":"url3",
               "metadata":[
               ]
            }
          ]
    }
  ]})";

  absl::flat_hash_set<std::string> custom_audience_names;
  absl::flat_hash_set<std::string> render_urls;

  absl::Status status = kv_server::ParseAudienceData(
      json_input, custom_audience_names, render_urls);
  EXPECT_TRUE(status.ok());
  EXPECT_THAT(custom_audience_names,
              testing::UnorderedElementsAre("group1", "group2"));
  EXPECT_THAT(render_urls,
              testing::UnorderedElementsAre("url1", "url2", "url3"));
}
}  // namespace
}  // namespace kv_server
