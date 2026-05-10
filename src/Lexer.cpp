export module Lexer;
import std;
import Token;
import Fatal;

export class Lexer {
public:
  Lexer(std::string_view source) : source(source) {}

  auto lex(this Lexer &self) -> std::vector<Token> {
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
  auto skipWhitespace(this Lexer &self) -> void {
    while (self.pos < self.source.size() &&
           std::isspace(self.source[self.pos])) {
      self.pos++;
    }
  }

  auto parseIdentifier(this Lexer &self) -> Token {
    size_t start = self.pos;
    while (
        self.pos < self.source.size() &&
        (std::isalnum(self.source[self.pos]) || self.source[self.pos] == '_')) {
      self.pos++;
    }
    std::string_view value = self.source.substr(start, self.pos - start);
    return Token{value, TokenType::Identifier};
  }

  auto parseNumber(this Lexer &self) -> Token {
    size_t start = self.pos;
    while (self.pos < self.source.size() &&
           std::isdigit(self.source[self.pos])) {
      self.pos++;
    }
    std::string_view value = self.source.substr(start, self.pos - start);
    return Token{value, TokenType::LiteralNumber};
  }

  auto parseOne(this Lexer &self) -> Token {
    self.skipWhitespace();

    char current = self.source[self.pos];

    switch (current) {
    case '\0':
      return Token{"", TokenType::EndOfFile};
    case '+':
      self.pos++;
      return Token{"+", TokenType::OpPlus};
    case '-':
      self.pos++;
      return Token{"-", TokenType::OpMinus};
    case '*':
      self.pos++;
      return Token{"*", TokenType::OpStar};
    case '/':
      self.pos++;
      return Token{"/", TokenType::OpSlash};
    case '=':
      if (self.source[self.pos + 1] == '=') {
        self.pos += 2;
        return Token{"==", TokenType::OpEqualEqual};
      } else {
        self.pos++;
        return Token{"=", TokenType::OpEqual};
      }
    case '(':
      self.pos++;
      return Token{"(", TokenType::PuncLeftParen};
    case ')':
      self.pos++;
      return Token{")", TokenType::PuncRightParen};
    case '{':
      self.pos++;
      return Token{"{", TokenType::PuncLeftBrace};
    case '}':
      self.pos++;
      return Token{"}", TokenType::PuncRightBrace};
    case ';':
      self.pos++;
      return Token{";", TokenType::PuncSemicolon};

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
  unsigned int pos = 0;
};
