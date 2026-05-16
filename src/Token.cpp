export module Token;
import std;
import SourceLocation;

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
  KwInt,
  KwString,
  KwBool,
  KwVoid,

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
  PuncComma,
};

export struct Token {
  std::string_view value;
  TokenType type;
  SourceLocation location;
};
