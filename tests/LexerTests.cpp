#include <boost/ut.hpp>

import std;
import Lexer;
import Token;

using namespace boost::ut;

suite<"lexer"> lexer_tests = [] {
  "lexes keywords, identifiers, literals, and punctuation"_test = [] {
    Lexer lexer{"int add(int lhs, int rhs) { return lhs + rhs; }"};
    auto tokens = lexer.lex();

    std::vector<TokenType> types;
    for (const auto& token : tokens) {
      types.push_back(token.type);
    }

    expect(types == std::vector<TokenType>{
                        TokenType::KwInt,
                        TokenType::Identifier,
                        TokenType::PuncLeftParen,
                        TokenType::KwInt,
                        TokenType::Identifier,
                        TokenType::PuncComma,
                        TokenType::KwInt,
                        TokenType::Identifier,
                        TokenType::PuncRightParen,
                        TokenType::PuncLeftBrace,
                        TokenType::KwReturn,
                        TokenType::Identifier,
                        TokenType::OpPlus,
                        TokenType::Identifier,
                        TokenType::PuncSemicolon,
                        TokenType::PuncRightBrace,
                        TokenType::EndOfFile,
                    });
  };
};
