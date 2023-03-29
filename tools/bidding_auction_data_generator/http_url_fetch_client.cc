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

#include "tools/bidding_auction_data_generator/http_url_fetch_client.h"

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "curl/multi.h"
#include "glog/logging.h"

namespace kv_server {
namespace {
// callback function to write the received data to the output
// https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
static size_t WriteCallback(void* data, size_t size, size_t number_of_elements,
                            void* output) {
  reinterpret_cast<std::string*>(output)->append(reinterpret_cast<char*>(data),
                                                 size * number_of_elements);
  return size * number_of_elements;
}
}  // namespace

absl::StatusOr<std::vector<std::string>> HttpUrlFetchClient::FetchUrls(
    const std::vector<std::string>& urls, int64_t timeout_ms) {
  std::vector<std::string> responses(urls.size());
  CURLM* multi_handle = curl_multi_init();
  for (int i = 0; i < urls.size(); i++) {
    VLOG(5) << "Request url: " << urls[i];
    CURL* eh = curl_easy_init();
    curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(eh, CURLOPT_WRITEDATA, &responses[i]);
    curl_easy_setopt(eh, CURLOPT_URL, urls[i].c_str());
    curl_easy_setopt(eh, CURLOPT_PRIVATE, urls[i].c_str());
    curl_easy_setopt(eh, CURLOPT_TIMEOUT_MS, timeout_ms);
    curl_multi_add_handle(multi_handle, eh);
  }
  // keep track of number of running handles
  int still_running = 1;
  while (still_running) {
    CURLMsg* msg;
    int msg_in_queue;
    curl_multi_perform(multi_handle, &still_running);
    while ((msg = curl_multi_info_read(multi_handle, &msg_in_queue))) {
      if (msg->msg == CURLMSG_DONE) {
        if (msg->data.result != CURLE_OK) {
          const auto error_msg = curl_easy_strerror(msg->data.result);
          LOG(ERROR) << "Error in the curl handle: " << error_msg;
          return absl::InternalError(error_msg);
        }
        CURL* eh = msg->easy_handle;
        curl_multi_remove_handle(multi_handle, eh);
        curl_easy_cleanup(eh);
      } else {
        return absl::InternalError(
            "Unable to read message from curl multi handle.");
      }
    }
  }
  curl_multi_cleanup(multi_handle);
  return responses;
}

}  // namespace kv_server
