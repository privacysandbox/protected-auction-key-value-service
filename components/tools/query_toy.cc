// Copyright 2023 Google LLC
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

// This program can be run to query the hard coded database below, ex:
// bazel run components/tools:query_toy -- --query="A UNION B"
// results in: [a,b,c,d]
// Alternatively you can run in interactive, allowing to query multiple times.
// bazel run components/tools:query_toy

#include <signal.h>

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/str_join.h"
#include "components/query/driver.h"
#include "components/query/scanner.h"

ABSL_FLAG(std::string, query, "",
          "If provided outputs the result to stdout.  Does not enter "
          "interactive mode. Interactive mode supplies user with repeated "
          "query prompts.");

absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> kDb = {
    {"A", {"a", "b", "c"}},
    {"B", {"b", "c", "d"}},
    {"C", {"c", "d", "e"}},
    {"D", {"d", "e", "f"}},
};

absl::flat_hash_set<std::string_view> kEmptySet;

template <typename T>
std::string ToString(const T& set) {
  std::vector<std::string_view> sorted_set(set.begin(), set.end());
  std::sort(sorted_set.begin(), sorted_set.end());
  return absl::StrCat("[", absl::StrJoin(sorted_set, ","), "]");
}

std::string ToString(
    const absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>&
        db) {
  // Get an alphabetically sorted list of string keys.
  std::vector<std::string> keys;
  keys.reserve(db.size());  // Reserve space to avoid unnecessary reallocations
  for (const auto& pair : db) {
    keys.push_back(pair.first);
  }
  std::sort(keys.begin(), keys.end());
  std::string result = "{\n";
  for (const auto& key : keys) {
    const auto& it = db.find(key);
    if (it != db.end()) {
      absl::StrAppend(&result, "\t{", key, ", ", ToString(it->second), "},\n");
    }
  }
  absl::StrAppend(&result, "}");
  return result;
}

absl::flat_hash_set<std::string_view> ToView(
    const absl::flat_hash_set<std::string>& values) {
  absl::flat_hash_set<std::string_view> result;
  result.reserve(values.size());
  result.insert(values.begin(), values.end());
  return result;
}

absl::StatusOr<absl::flat_hash_set<std::string_view>> Parse(
    kv_server::Driver& driver, std::string query) {
  std::istringstream stream(query);
  kv_server::Scanner scanner(stream);
  kv_server::Parser parse(driver, scanner);
  int parse_result = parse();
  auto result = driver.GetResult();
  if (parse_result && result.ok()) {
    std::cerr << "Unexpected failed parse result with an OK query result.";
  }
  return result;
}

absl::flat_hash_set<std::string_view> Lookup(std::string_view key) {
  const auto& it = kDb.find(key);
  if (it != kDb.end()) {
    return ToView(it->second);
  }
  return kEmptySet;
}

void ProcessQuery(kv_server::Driver& driver, std::string query) {
  const auto result = Parse(driver, query);
  if (!result.ok()) {
    std::cout << result.status() << std::endl;
    return;
  }
  std::cout << ToString(result.value()) << std::endl;
}

void PromptForQuery(kv_server::Driver& driver) {
  while (true) {
    std::cout << ">> ";
    std::string query;
    std::getline(std::cin, query);
    ProcessQuery(driver, query);
  }
}

void SignalHandler(int signal) {
  std::cout << " Quitting." << std::endl;
  exit(0);
}

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);
  kv_server::Driver driver(Lookup);
  const std::string query = absl::GetFlag(FLAGS_query);
  if (!query.empty()) {
    ProcessQuery(driver, query);
    return 0;
  }
  signal(SIGINT, SignalHandler);
  signal(SIGQUIT, SignalHandler);
  std::cout << "/*" << std::endl << "Sets available to query:" << std::endl;
  std::cout << ToString(kDb) << std::endl;
  std::cout << "*/" << std::endl;
  PromptForQuery(driver);
  return 0;
}
