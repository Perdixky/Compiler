#include <boost/ut.hpp>
#include <proxy/proxy.h>

import std;
import Lexer;
import Parser;

using namespace boost::ut;

suite<"parser"> parser_tests = [] {
  "parses variable declarations with optional initializer"_test = [] {
    Lexer lexer{"int answer = 42"};
    Parser parser{lexer.lex()};
    auto decl = parser.parseDecl();
    const auto& var = proxy_cast<const VarDecl&>(*decl);

    expect(var.name == "answer");
    expect(var.type == Type::Int);
    expect(var.initializer.has_value());
    expect(parser.isAtEnd());
  };

  "parses function declarations with parameters and body"_test = [] {
    Lexer lexer{"int add(int lhs, int rhs) { return 1; }"};
    Parser parser{lexer.lex()};
    auto decl = parser.parseDecl();
    const auto& func = proxy_cast<const FuncDecl&>(*decl);

    expect(func.name == "add");
    expect(func.return_type == Type::Int);
    expect(func.parameters.size() == 2_i);

    const auto& first_param = proxy_cast<const VarDecl&>(*func.parameters.front());
    expect(first_param.name == "lhs");
    expect(first_param.type == Type::Int);
    expect(parser.isAtEnd());
  };

  "parses additive and multiplicative expressions with precedence"_test = [] {
    Lexer lexer{"1 + 2 * 3"};
    Parser parser{lexer.lex()};
    auto expr = parser.parseExpr();
    const auto& add = proxy_cast<const BinaryExpr&>(*expr);

    expect(add.op == BinaryExpr::BinaryOp::Add);
    const auto& right = proxy_cast<const BinaryExpr&>(*add.right);
    expect(right.op == BinaryExpr::BinaryOp::Multiply);
    expect(parser.isAtEnd());
  };
};
