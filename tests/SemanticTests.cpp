#include <boost/ut.hpp>

import std;
import Lexer;
import Parser;
import Semantic;

using namespace boost::ut;

namespace {

auto parse_decls(std::string_view source) -> std::vector<ProDecl> {
  Lexer lexer{source};
  Parser parser{lexer.lex()};

  std::vector<ProDecl> decls;
  while (!parser.isAtEnd()) {
    decls.push_back(parser.parseDecl());
  }
  return decls;
}

auto opcodes(const std::vector<Quad>& code) -> std::vector<std::string> {
  std::vector<std::string> result;
  for (const auto& quad : code) {
    result.push_back(quad.op);
  }
  return result;
}

} // namespace

suite<"semantic"> semantic_tests = [] {
  "generates symbols and three address code for declarations assignments and if"_test = [] {
    auto decls = parse_decls("int main() { int a = 2; int b = 1; int c; if (a > b) { c = a + b; } return c; }");

    SemanticAnalyzer analyzer;
    auto result = analyzer.analyze(decls);

    expect(result.symbols.size() == 4_i);
    expect(result.symbols.contains("main"));
    expect(result.symbols.contains("a"));
    expect(result.symbols.contains("b"));
    expect(result.symbols.contains("c"));

    const auto ops = opcodes(result.code);
    expect(ops == std::vector<std::string>{
                      "func",
                      "assign",
                      "assign",
                      ">",
                      "if_false",
                      "+",
                      "assign",
                      "label",
                      "return",
                      "end",
                  });

    expect(result.code[0].result == "main");
    expect(result.code[1].arg1 == "2");
    expect(result.code[1].result == "a");
    expect(result.code[3].arg1 == "a");
    expect(result.code[3].arg2 == "b");
    expect(result.code[5].arg1 == "a");
    expect(result.code[5].arg2 == "b");
    expect(result.code[6].result == "c");
    expect(result.code[8].arg1 == "c");
  };
};
