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

#include "components/query/scanner.h"

#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_join.h"
#include "components/query/driver.h"
#include "gtest/gtest.h"

namespace kv_server {
namespace {

TEST(ScannerTest, Empty) {
  std::istringstream stream("");
  Scanner scanner(stream);
  Driver driver;
  auto t = scanner.yylex(driver);
  ASSERT_EQ(t.token(), Parser::token::YYEOF);
}

TEST(ScannerTest, Var) {
  std::istringstream stream("FOO foo Foo123");
  Scanner scanner(stream);
  Driver driver;
  // first token
  auto t1 = scanner.yylex(driver);
  ASSERT_EQ(t1.token(), Parser::token::VAR);
  ASSERT_EQ(t1.value.as<std::string>(), "FOO");

  // second token
  auto t2 = scanner.yylex(driver);
  ASSERT_EQ(t2.token(), Parser::token::VAR);
  ASSERT_EQ(t2.value.as<std::string>(), "foo");

  // third token
  auto t3 = scanner.yylex(driver);
  ASSERT_EQ(t3.token(), Parser::token::VAR);
  ASSERT_EQ(t3.value.as<std::string>(), "Foo123");

  // done
  auto t4 = scanner.yylex(driver);
  ASSERT_EQ(t4.token(), Parser::token::YYEOF);
}

TEST(ScannerTest, Parens) {
  std::istringstream stream("()");
  Scanner scanner(stream);
  Driver driver;

  auto t1 = scanner.yylex(driver);
  ASSERT_EQ(t1.token(), Parser::token::LPAREN);
  auto t2 = scanner.yylex(driver);
  ASSERT_EQ(t2.token(), Parser::token::RPAREN);
  auto t3 = scanner.yylex(driver);
  ASSERT_EQ(t3.token(), Parser::token::YYEOF);
}

TEST(ScannerTest, WhitespaceVar) {
  std::istringstream stream(" FOO ");
  Scanner scanner(stream);
  Driver driver;
  // first token
  auto t1 = scanner.yylex(driver);
  ASSERT_EQ(t1.token(), Parser::token::VAR);
  ASSERT_EQ(t1.value.as<std::string>(), "FOO");
  auto t2 = scanner.yylex(driver);
  ASSERT_EQ(t2.token(), Parser::token::YYEOF);
}

TEST(ScannerTest, NotAlphaNumVar) {
  std::vector<std::string> expected_vars = {"_", ":", ".", "A_B", "A:B", "A.B"};
  std::string token_list = absl::StrJoin(expected_vars, " ");
  std::istringstream stream(token_list);
  Scanner scanner(stream);
  Driver driver;

  for (const auto& expected_var : expected_vars) {
    auto token = scanner.yylex(driver);
    ASSERT_EQ(token.token(), Parser::token::VAR);
    ASSERT_EQ(token.value.as<std::string>(), expected_var);
  }
  auto last = scanner.yylex(driver);
  ASSERT_EQ(last.token(), Parser::token::YYEOF);
}

TEST(ScannerTest, QuotedVar) {
  std::istringstream stream(
      " \"A1:Stuff\" \"A-B:C&D=E|F\" \"A+B\" \"A/B\" \"A\" ");
  Scanner scanner(stream);
  Driver driver;

  auto t1 = scanner.yylex(driver);
  ASSERT_EQ(t1.token(), Parser::token::VAR);
  ASSERT_EQ(t1.value.as<std::string>(), "A1:Stuff");
  auto t2 = scanner.yylex(driver);
  ASSERT_EQ(t2.token(), Parser::token::VAR);
  ASSERT_EQ(t2.value.as<std::string>(), "A-B:C&D=E|F");
  auto t3 = scanner.yylex(driver);
  ASSERT_EQ(t3.token(), Parser::token::VAR);
  ASSERT_EQ(t3.value.as<std::string>(), "A+B");
  auto t4 = scanner.yylex(driver);
  ASSERT_EQ(t4.token(), Parser::token::VAR);
  ASSERT_EQ(t4.value.as<std::string>(), "A/B");
  auto t5 = scanner.yylex(driver);
  ASSERT_EQ(t5.token(), Parser::token::VAR);
  ASSERT_EQ(t5.value.as<std::string>(), "A");
  auto last = scanner.yylex(driver);
  ASSERT_EQ(last.token(), Parser::token::YYEOF);
}

TEST(ScannerTest, EmptyQuotedInvalid) {
  std::istringstream stream(" \"\" ");
  Scanner scanner(stream);
  Driver driver;

  // Since it there is no valid match, we have 2 errors
  // for each of the double quotes.
  auto t1 = scanner.yylex(driver);
  ASSERT_EQ(t1.token(), Parser::token::ERROR);
  auto t2 = scanner.yylex(driver);
  ASSERT_EQ(t2.token(), Parser::token::ERROR);

  auto t3 = scanner.yylex(driver);
  ASSERT_EQ(t3.token(), Parser::token::YYEOF);
}

TEST(ScannerTest, Operators) {
  std::istringstream stream("| UNION & INTERSECTION - DIFFERENCE");
  Scanner scanner(stream);
  Driver driver;

  auto t1 = scanner.yylex(driver);
  ASSERT_EQ(t1.token(), Parser::token::UNION);
  auto t2 = scanner.yylex(driver);
  ASSERT_EQ(t2.token(), Parser::token::UNION);

  auto t3 = scanner.yylex(driver);
  ASSERT_EQ(t3.token(), Parser::token::INTERSECTION);
  auto t4 = scanner.yylex(driver);
  ASSERT_EQ(t4.token(), Parser::token::INTERSECTION);

  auto t5 = scanner.yylex(driver);
  ASSERT_EQ(t5.token(), Parser::token::DIFFERENCE);
  auto t6 = scanner.yylex(driver);
  ASSERT_EQ(t6.token(), Parser::token::DIFFERENCE);

  auto t7 = scanner.yylex(driver);
  ASSERT_EQ(t7.token(), Parser::token::YYEOF);
}

TEST(ScannerTest, Error) {
  std::istringstream stream("!");
  Scanner scanner(stream);
  Driver driver;

  auto t1 = scanner.yylex(driver);
  ASSERT_EQ(t1.token(), Parser::token::ERROR);
  auto t2 = scanner.yylex(driver);
  ASSERT_EQ(t2.token(), Parser::token::YYEOF);
}

}  // namespace
}  // namespace kv_server
