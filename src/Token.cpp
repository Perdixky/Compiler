export module Token;
import std;

export enum class TokenType {
  EndOfFile,

  Identifier,

  LiteralNumber,
  LiteralString,
  LiteralBoolean,
  LiteralNull,

  KwIf,
  KwElse,
  KwWhile,
  KwReturn,

  OpPlus,
  OpMinus,
  OpStar,
  OpSlash,
  OpEqual,
  OpEqualEqual,

  PuncLeftParen,
  PuncRightParen,
  PuncLeftBrace,
  PuncRightBrace,
  PuncSemicolon,
};

export struct Token {
  std::string_view value;
  TokenType type;
};
