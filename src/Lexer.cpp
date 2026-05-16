export module Lexer;
import std;
import Token;
import Fatal;
import SourceLocation;

export class Lexer {
public:
  Lexer(std::string_view source) : source(source) {}

  auto lex(this Lexer& self) -> std::vector<Token> {
    std::vector<Token> tokens;
    tokens.reserve(self.source.size() / 3);

    while (true) {
      tokens.emplace_back(self.parseOne());
      if (tokens.back().type == TokenType::EndOfFile) [[unlikely]] {
        return tokens;
      }
    }
  }

private:
  auto skipWhitespace(this Lexer& self) -> void {
    while (self.pos < self.source.size() && std::isspace(self.source[self.pos])) {
      self.pos++;
    }
  }

  auto parseIdentifier(this Lexer& self) -> Token {
    std::size_t start = self.pos;
    while (self.pos < self.source.size() && (std::isalnum(self.source[self.pos]) || self.source[self.pos] == '_')) {
      self.pos++;
    }
    std::string_view value = self.source.substr(start, self.pos - start);
    if (value == "if") {
      return Token{value, TokenType::KwIf};
    }
    if (value == "else") {
      return Token{value, TokenType::KwElse};
    }
    if (value == "while") {
      return Token{value, TokenType::KwWhile};
    }
    if (value == "return") {
      return Token{value, TokenType::KwReturn};
    }
    if (value == "int") {
      return Token{value, TokenType::KwInt};
    }
    if (value == "string") {
      return Token{value, TokenType::KwString};
    }
    if (value == "bool") {
      return Token{value, TokenType::KwBool};
    }
    if (value == "void") {
      return Token{value, TokenType::KwVoid};
    }
    return Token{value, TokenType::Identifier};
  }

  auto parseNumber(this Lexer& self) -> Token {
    std::size_t start = self.pos;
    while (self.pos < self.source.size() && std::isdigit(self.source[self.pos])) {
      self.pos++;
    }
    std::string_view value = self.source.substr(start, self.pos - start);
    return Token{value, TokenType::LiteralNumber};
  }

  auto parseOne(this Lexer& self) -> Token {
    self.skipWhitespace();

    SourceLocation location{.file = "", .line = 1, .column = self.pos + 1};
    if (self.pos >= self.source.size()) {
      return Token{"", TokenType::EndOfFile, location};
    }

    char current = self.source[self.pos];

    switch (current) {
    case '\0':
      return Token{"", TokenType::EndOfFile, location};
    case '+':
      self.pos++;
      return Token{"+", TokenType::OpPlus, location};
    case '-':
      self.pos++;
      return Token{"-", TokenType::OpMinus, location};
    case '*':
      self.pos++;
      return Token{"*", TokenType::OpStar, location};
    case '/':
      self.pos++;
      return Token{"/", TokenType::OpSlash, location};
    case '=':
      if (self.pos + 1 < self.source.size() && self.source[self.pos + 1] == '=') {
        self.pos += 2;
        return Token{"==", TokenType::OpEqualEqual, location};
      } else {
        self.pos++;
        return Token{"=", TokenType::OpEqual, location};
      }
    case '(':
      self.pos++;
      return Token{"(", TokenType::PuncLeftParen, location};
    case ')':
      self.pos++;
      return Token{")", TokenType::PuncRightParen, location};
    case '{':
      self.pos++;
      return Token{"{", TokenType::PuncLeftBrace, location};
    case '}':
      self.pos++;
      return Token{"}", TokenType::PuncRightBrace, location};
    case ';':
      self.pos++;
      return Token{";", TokenType::PuncSemicolon, location};
    case ',':
      self.pos++;
      return Token{",", TokenType::PuncComma, location};

    default:
      if (std::isalpha(current) || current == '_') {
        return self.parseIdentifier();
      } else if (std::isdigit(current)) {
        return self.parseNumber();
      } else [[unlikely]] {
        fatal("Unexpected character: '{}'", current);
      }
    }
  }

private:
  std::string_view source;
  std::size_t pos = 0;
};
