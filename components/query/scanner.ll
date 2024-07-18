/*
 Copyright 2023 Google LLC

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

/* recognize tokens to support set operations*/
%{
#include "components/query/parser.h"
#include "components/query/scanner.h"

#undef  YY_DECL
#define YY_DECL kv_server::Parser::symbol_type \
  kv_server::Scanner::yylex(kv_server::Driver& driver)

%}

%option c++
%option yyclass="Scanner"
%option prefix="KV"
%option noyywrap nounput noinput debug batch

/* States */
%x IN_FUNC

/* Valid key name characters, this list can be expanded as needed */
VAR_CHARS         [a-zA-Z0-9_:\.]
/*
   Characters used for set operations or those that could be confused.
   Allowing for +, =, / makes the quoted key name characters a superset of
   base64 encoding.
*/
OP_CHARS          [|&\-+=/]

%%
[ \t\r\n]+         {}
(?i:SET)           { yy_push_state(IN_FUNC); return kv_server::Parser::make_SET(); }
<IN_FUNC>{
  "("              { return kv_server::Parser::make_LPAREN(); }
  ")"              { yy_pop_state(); return kv_server::Parser::make_RPAREN();}
  ","              { return kv_server::Parser::make_COMMA(); }
  [0-9]+           { return kv_server::Parser::make_NUMBER(yytext); }
  \"[^\"]*\"       { yytext[strlen(yytext)-1]='\0'; return kv_server::Parser::make_STRING(yytext + 1); }
}
"("                { return kv_server::Parser::make_LPAREN(); }
")"                { return kv_server::Parser::make_RPAREN();}
(?i:UNION)         { return kv_server::Parser::make_UNION(); }
"|"                { return kv_server::Parser::make_UNION(); }
(?i:INTERSECTION)  { return kv_server::Parser::make_INTERSECTION(); }
"&"                { return kv_server::Parser::make_INTERSECTION(); }
(?i:DIFFERENCE)    { return kv_server::Parser::make_DIFFERENCE(); }
"-"                { return kv_server::Parser::make_DIFFERENCE(); }
{VAR_CHARS}+       { return kv_server::Parser::make_VAR(yytext); }
"\""({VAR_CHARS}+|{OP_CHARS}+)+"\"" {
                     // Exclude the double quotes from the var name.
                     yytext[strlen(yytext)-1]='\0';
                     return kv_server::Parser::make_VAR(yytext+1);}
.                  { return kv_server::Parser::make_ERROR(yytext); }
<<EOF>>            { return kv_server::Parser::make_YYEOF(); }
%%
