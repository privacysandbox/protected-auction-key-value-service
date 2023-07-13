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

#ifndef COMPONENTS_QUERY_SCANNER_H_
#define COMPONENTS_QUERY_SCANNER_H_

// FlexLexer.h has no include guard so that it can be included multiple times,
// each time providing a different definition for  yFlexLexer, to define
// multiple lexer base classes.
#ifndef yyFlexLexerOnce
#define yyFlexLexer KVFlexLexer
#include <FlexLexer.h>
#endif

#include <istream>

#include "components/query/parser.h"

namespace kv_server {
class Driver;

// Lexer responsible for converting input stream into tokens.
class Scanner : public yyFlexLexer {
 public:
  explicit Scanner(std::istream& input) : yyFlexLexer(&input) {}
  ~Scanner() override = default;
  virtual kv_server::Parser::symbol_type yylex(kv_server::Driver& driver);

 private:
  // This function is never called.  It is here to avoid
  // Compilation warning: 'kv_server::Scanner::yylex' hides
  // overloaded virtual function [-Woverloaded-virtual]
  // defined in FlexLexer.h
  // The above yylex function is called by parser.yy
  int yylex() override {
    assert(false);
    return 1;
  }
};

}  // namespace kv_server
#endif  // COMPONENTS_QUERY_SCANNER_H_
