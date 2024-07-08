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

%skeleton "lalr1.cc" // -*- C++ -*-
%require "3.3.2"
%language "c++"

%define api.parser.class {Parser}
%define api.namespace {kv_server}
// Make sure we are thread safe and don't use yylval, yylloc
// https://www.gnu.org/software/bison/manual/html_node/Pure-Calling.html
%define api.pure full

%code requires {
  #include <memory>
  #include <string>
  #include "components/query/ast.h"

  namespace kv_server {
  class Scanner;
  class Driver;
  }  // namespace kv_server
}
// The parsing context.
%param { Driver& driver }
%parse-param {Scanner& scanner}


%code {
  #include "components/query/parser.h"
  #include "components/query/driver.h"
  #include "components/query/scanner.h"

  #undef yylex
  #define yylex(x) scanner.yylex(x)
}

/* declare tokens */
%token UNION INTERSECTION DIFFERENCE LPAREN RPAREN
%token <std::string> VAR ERROR
%token YYEOF 0

/* Allows defining the types returned by `term` and `exp below. */
%define api.token.constructor
%define api.value.type variant

%type <std::unique_ptr<Node>> term
%nterm <std::unique_ptr<Node>> exp

/* Order of operations is left to right */
%left UNION INTERSECTION DIFFERENCE

/* Cause build failures on grammar conflicts */
%expect 0

%initial-action {
  driver.ClearError();
}

%%

query:
  %empty
 | query exp YYEOF { driver.SetAst(std::move($2)); }

exp: term {$$ = std::move($1);}
 | exp UNION exp { $$ = std::make_unique<UnionNode>(std::move($1), std::move($3)); }
 | exp INTERSECTION exp { $$ = std::make_unique<IntersectionNode>(std::move($1), std::move($3)); }
 | exp DIFFERENCE exp { $$ = std::make_unique<DifferenceNode>(std::move($1), std::move($3)); }
 | LPAREN exp RPAREN   { $$ = std::move($2); }
 | ERROR { driver.SetError("Invalid token: " + $1); YYERROR;}
 ;

term: VAR { $$ = std::make_unique<ValueNode>(std::move($1)); }
 ;

%%

void
kv_server::Parser::error (const std::string& m)
{
  driver.SetError(m);
}
