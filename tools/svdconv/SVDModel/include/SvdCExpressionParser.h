/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdCExpressionParser_H
#define SvdCExpressionParser_H

#include <string>
#include <map>
#include <list>

class SvdCExpressionParser {
public:
  SvdCExpressionParser(const std::string& exprStr);
  ~SvdCExpressionParser();

  bool Parse();

  /*
  * Expression - Node types
  */
  typedef enum xeType  {
    xme_const            = 256,    // some constant
    xme_strcon,                    // string "xxxx"
    xme_namcon,                    // identifier
    xme_labcon,                    // some label, not used in XM

    xme_ref,                       // some reference
    xme_reff,                      // bit-field-reference - not used in XM
    xme_cast,                      // some type cast
    xme_postinc,                   // expr++ postfix
    xme_postdec,                   // expr-- postfix
    xme_preinc,                    // ++expr prefix
    xme_predec,                    // --expr prefix

    xme_addr,                      // & (address_of) - not used in XM
    xme_cont,                      // * (content_of) - not used in XM
    xme_plus,                      // unary +
    xme_minus,                     // unary -
    xme_not,                       // ! (not)
    xme_compl,                     // ~ (complement)
    xme_asn,                       // = (assignment
    xme_asnor,                     // |=
    xme_asnxor,                    // ^=
    xme_asnand,                    // &=
    xme_asnlsh,                    // <<=
    xme_asnrsh,                    // >>=
    xme_asnadd,                    // +=
    xme_asnsub,                    // -=
    xme_asnmul,                    // *=
    xme_asndiv,                    // /=
    xme_asnmod,                    // %=
    xme_hook,                      // e0 ? e1 : e2
    xme_land,                      // &&
    xme_lor,                       // ||
    xme_and,                       // & (bit-and)
    xme_or,                        // | (bit-or)
    xme_xor,                       // ^ (bit-xor)
    xme_equ,                       // ==
    xme_nequ,                      // !=
    xme_lequ,                      // <=
    xme_gequ,                      // >=
    xme_gt,                        // >
    xme_lt,                        // <
    xme_lsh,                       // <<
    xme_rsh,                       // >>
    xme_add,                       // +
    xme_sub,                       // -
    xme_mod,                       // %
    xme_div,                       // /
    xme_mul,                       // *

    xme_param,                     // actual parameter
    xme_void,                      // void-node
    xme_fcall,                     // direct function call

    xme_what,                      // unknown token
    xme_eoi,                       // end of input

    xme_identi,                    // some identifer
    xme_quest,                     // ?
    xme_colon,                     // :
    xme_pp,                        // ++ token
    xme_mm,                        // -- token
    xme_star,                      // * token
    xme_openbr,                    // ( token
    xme_closebr,                   // ) token
    xme_opendim,                   // [ token
    xme_closedim,                  // ] token
    xme_openbl,                    // { token

    xme_closebl,                   // } token
    xme_comma,                     // expr, expr
    xme_semik,                     // ;

    xme_comment,                   // // Comment
    xme_dot,                       // .
    xme_qual,                      // identifier . identifer
    xme_index,                     // expr [index_expr]

    xme_typmemb,                   // 'typedef-id : type_member_id'
                                   // xme_vardecl,                   // Variable declaration via '__var'
  } XETYPE;

  typedef struct token_s {
    XETYPE        type;
    std::string   text;

    token_s() : type(xme_what) {}
  } token_t;
  using TokenList = std::list<token_t>;

  const TokenList& GetTokenList() { return m_tokenList; }

protected:

  typedef struct expr_s {
    expr_s       *L;
    expr_s       *R;

    XETYPE       tp;
    std::string   text;

    expr_s() : L(0), R(0), tp(xme_what) {}
    ~expr_s() {
      delete L;
      delete R;
    }
  } expr_t;

  XETYPE    GetChar         ();
  XETYPE    PutChar         ();
  XETYPE    PeekChar        ();
  XETYPE    GetNextToken    (token_t& token);
  XETYPE    SkipWhite       ();
  XETYPE    GetToken        (std::string& tokenTxt, const std::string& allowedSymbols);

private:
  static const std::string symbol;
  static const std::string number;

  std::string       m_exprStr;
  std::size_t       m_strPos;
  TokenList         m_tokenList;
};

#endif // SvdCExpressionParser_H
