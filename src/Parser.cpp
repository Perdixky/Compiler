module;
#include <proxy/proxy.h>
#include <ranges>
#include <rfl.hpp>
export module Parser;
import std;
import SourceLocation;
import Lexer;
import Token;
import Fatal;

export struct ExprVisitor {
  auto visit(this ExprVisitor& self, auto& expr) -> void {
    // Default implementation does nothing
  }
};
export struct DeclVisitor {
  auto visit(this DeclVisitor& self, auto& decl) -> void {
    // Default implementation does nothing
  }
};
export struct StmtVisitor {
  auto visit(this StmtVisitor& self, auto& stmt) -> void {
    // Default implementation does nothing
  }
};

PRO_DEF_MEM_DISPATCH(accept, accept);

export struct Expr : pro::facade_builder::add_convention<accept, void(ExprVisitor&)>::add_skill<
                         pro::skills::rtti>::add_skill<pro::skills::direct_rtti>::build {};
export struct Decl : pro::facade_builder::add_convention<accept, void(DeclVisitor&)>::add_skill<
                         pro::skills::rtti>::add_skill<pro::skills::direct_rtti>::build {};
export struct Stmt : pro::facade_builder::add_convention<accept, void(StmtVisitor&)>::add_skill<
                         pro::skills::rtti>::add_skill<pro::skills::direct_rtti>::build {};

#define PRO_DECLARE_ACCEPT(Type, Name)                                                                                 \
  auto accept(this Name##Type& self, Type##Visitor& visitor) -> void {                                                 \
    visitor.visit(self);                                                                                               \
  }

export using ProExpr = pro::proxy<Expr>;
export using ProDecl = pro::proxy<Decl>;
export using ProStmt = pro::proxy<Stmt>;

struct MoveOnly {
  MoveOnly() = default;
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly(MoveOnly&&) = default;
  auto operator=(const MoveOnly&) -> MoveOnly& = delete;
  auto operator=(MoveOnly&&) -> MoveOnly& = default;
};

export enum class Type {
  Void,
  Int,
  String,
  Bool
};

export struct VarDecl {
  std::string_view name;
  std::optional<ProExpr> initializer;
  Type type;

  PRO_DECLARE_ACCEPT(Decl, Var);
};

export struct BlockStmt : MoveOnly {
  std::vector<ProStmt> statements;

  PRO_DECLARE_ACCEPT(Stmt, Block);
};

export struct FuncDecl : MoveOnly {
  std::string_view name;
  std::vector<ProDecl> parameters;
  ProStmt body;
  Type return_type;

  PRO_DECLARE_ACCEPT(Decl, Func);
};

export struct IfStmt : MoveOnly {
  ProExpr condition;
  ProStmt then_branch;
  std::optional<ProStmt> else_branch;

  PRO_DECLARE_ACCEPT(Stmt, If);
};

export struct ReturnStmt : MoveOnly {
  std::optional<ProExpr> value;

  PRO_DECLARE_ACCEPT(Stmt, Return);
};

export struct AssignStmt : MoveOnly {
  std::string_view variable_name;
  ProExpr value;

  PRO_DECLARE_ACCEPT(Stmt, Assign);
};

export struct DeclStmt : MoveOnly {
  ProDecl decl;

  PRO_DECLARE_ACCEPT(Stmt, Decl);
};

export struct FuncCallStmt : MoveOnly {
  std::string_view function_name;
  std::vector<ProExpr> arguments;

  PRO_DECLARE_ACCEPT(Stmt, FuncCall);
};

export struct LiteralNumExpr {
  int value;

  PRO_DECLARE_ACCEPT(Expr, LiteralNum);
};

export struct BinaryExpr : MoveOnly {
  enum class BinaryOp {
    Add,
    Subtract,
    Multiply,
    Divide,
    Less,
    Greater,
    Equal
  } op;

  ProExpr left;
  ProExpr right;

  PRO_DECLARE_ACCEPT(Expr, Binary);
};

export class Parser {
public:
  Parser(std::vector<Token>&& tokens) : tokens(tokens), pos(0) {}

  auto isAtEnd(this Parser& self) -> bool {
    return self.check(TokenType::EndOfFile);
  }

  auto parseDecl() -> ProDecl {
    Type type = parseType(/*allow_void=*/true);
    Token name = expect(TokenType::Identifier);

    if (match(TokenType::PuncLeftParen)) {
      FuncDecl* decl = new FuncDecl;
      decl->return_type = type;
      decl->name = name.value;

      if (!check(TokenType::PuncRightParen)) {
        do {
          Type param_type = parseType(/*allow_void=*/false);
          Token param_name = expect(TokenType::Identifier);
          VarDecl* param = new VarDecl;
          param->type = param_type;
          param->name = param_name.value;
          decl->parameters.push_back(param);
        } while (match(TokenType::PuncComma));
      }
      expect(TokenType::PuncRightParen);
      expect(TokenType::PuncLeftBrace);
      decl->body = parseBlockStmt();
      return decl;
    }

    if (type == Type::Void) [[unlikely]] {
      fatal("Variable declaration cannot have void type at: {}", name.location.to_string());
    }

    VarDecl* decl = new VarDecl;
    decl->type = type;
    decl->name = name.value;
    if (match(TokenType::OpEqual)) {
      decl->initializer = parseExpr();
    }
    return decl;
  }

  auto parseType(bool allow_void) -> Type {
    std::vector<TokenType> types{TokenType::KwInt, TokenType::KwString, TokenType::KwBool, TokenType::KwVoid};
    Token cur = expect(types);

    switch (cur.type) {
    case TokenType::KwInt:
      return Type::Int;
    case TokenType::KwString:
      return Type::String;
    case TokenType::KwBool:
      return Type::Bool;
    case TokenType::KwVoid:
      if (allow_void) {
        return Type::Void;
      }
      fatal("Parameter declaration cannot have void type at: {}", cur.location.to_string());
    default:
      fatal("Unexpected type: {} at: {}", cur.value, cur.location.to_string());
    }
  }

  auto parseStmt() -> ProStmt {
    std::vector<TokenType> decl_start_tokens{TokenType::KwInt, TokenType::KwString, TokenType::KwBool,
                                             TokenType::KwVoid};
    if (check(decl_start_tokens)) {
      return parseDeclStmt();
    }

    if (check(TokenType::KwIf)) {
      return parseIfStmt();
    }

    if (check(TokenType::KwReturn)) {
      return parseReturnStmt();
    }

    if (auto id = match(TokenType::Identifier)) {
      if (match(TokenType::OpEqual)) {
        AssignStmt* stmt = new AssignStmt;

        stmt->variable_name = id.value().value;
        stmt->value = parseExpr();
        expect(TokenType::PuncSemicolon);
        return stmt;

      } else if (match(TokenType::PuncLeftParen)) {
        FuncCallStmt* stmt = new FuncCallStmt;

        stmt->function_name = id.value().value;
        do {
          stmt->arguments.push_back(parseExpr());
        } while (match(TokenType::PuncComma));
        expect(TokenType::PuncRightParen);
        expect(TokenType::PuncSemicolon);

        return stmt;
      } else {
        fatal("Unexpected token after identifier: {} at: {}", peek().value, peek().location.to_string());
      }
    }

    fatal("Unexpected token: {} at: {}", peek().value, peek().location.to_string());
  }

  auto parseIfStmt() -> ProStmt {
    IfStmt* stmt = new IfStmt;

    expect(TokenType::KwIf);
    expect(TokenType::PuncLeftParen);
    stmt->condition = parseExpr();
    expect(TokenType::PuncRightParen);

    stmt->then_branch = parseBlockStmt();

    if (match(TokenType::KwElse)) {
      stmt->else_branch = parseBlockStmt();
    }

    return stmt;
  }

  auto parseReturnStmt() -> ProStmt {
    ReturnStmt* stmt = new ReturnStmt;

    expect(TokenType::KwReturn);

    if (!check(TokenType::PuncSemicolon)) {
      stmt->value = parseExpr();
    }

    expect(TokenType::PuncSemicolon);
    return stmt;
  }

  auto parseDeclStmt() -> ProStmt {
    DeclStmt* stmt = new DeclStmt;
    stmt->decl = parseDecl();
    expect(TokenType::PuncSemicolon);
    return stmt;
  }

  auto parseBlockStmt() -> ProStmt {
    BlockStmt* stmt = new BlockStmt;

    while (true) {
      if (match(TokenType::PuncRightBrace)) [[unlikely]] {
        break;
      }
      stmt->statements.push_back(parseStmt());
    }

    return stmt;
  }

  auto parsePrimary() -> ProExpr {
    Token cur = expect(TokenType::LiteralNumber);
    int value;
    std::from_chars(cur.value.cbegin(), cur.value.cend(), value);
    return pro::make_proxy<Expr, LiteralNumExpr>(value);
  }

  auto parseTerm() -> ProExpr {
    ProExpr expr = parsePrimary();
    std::vector<TokenType> op_types{TokenType::OpStar, TokenType::OpSlash};

    while (auto op = match(op_types)) {
      BinaryExpr* binary = new BinaryExpr;
      binary->left = std::move(expr);
      binary->op = op.value().type == TokenType::OpStar ? BinaryExpr::BinaryOp::Multiply : BinaryExpr::BinaryOp::Divide;
      binary->right = parsePrimary();
      expr = binary;
    }

    return expr;
  }

  auto parseExpr() -> ProExpr {
    ProExpr expr = parseTerm();
    std::vector<TokenType> op_types{TokenType::OpPlus, TokenType::OpMinus};

    while (auto op = match(op_types)) {
      BinaryExpr* binary = new BinaryExpr;
      binary->left = std::move(expr);
      binary->op = op.value().type == TokenType::OpPlus ? BinaryExpr::BinaryOp::Add : BinaryExpr::BinaryOp::Subtract;
      binary->right = parseTerm();
      expr = binary;
    }

    return expr;
  }

  auto peek(this Parser& self) -> Token {
    if (self.pos >= self.tokens.size()) [[unlikely]] {
      fatal("Unexpected end of input at: {}", self.tokens.back().location.to_string());
    }
    return self.tokens[self.pos];
  }

  auto check(this Parser& self, TokenType type) -> bool {
    return self.pos < self.tokens.size() && self.peek().type == type;
  }

  auto check(this Parser& self, std::span<TokenType> types) -> bool {
    return self.pos < self.tokens.size() && std::ranges::find(types, self.peek().type) != types.end();
  }

  auto match(this Parser& self, TokenType type) -> std::optional<Token> {
    if (self.check(type)) {
      return self.tokens[self.pos++];
    }
    return std::nullopt;
  }

  auto match(this Parser& self, std::span<TokenType> types) -> std::optional<Token> {
    if (self.check(types)) {
      return self.tokens[self.pos++];
    }
    return std::nullopt;
  }

  auto expect(this Parser& self, TokenType type) -> Token {
    Token cur = self.peek();
    if (cur.type == type) {
      self.pos++;
      return cur;
    }
    fatal("Expected token of type {}, but got {} at: {}", rfl::enum_to_string(type), cur.value,
          cur.location.to_string());
  }

  auto expect(this Parser& self, std::span<TokenType> types) -> Token {
    Token cur = self.peek();

    if (std::ranges::find(types, cur.type) != types.end()) {
      self.pos++;
      return cur;
    }

    auto type_names = std::views::transform(types,
                                            [](const auto& type) {
                                              return rfl::enum_to_string(type);
                                            }) |
                      std::views::join_with(std::string_view{", "});
    fatal("Expected one of: {}, but got {} at: {}", type_names, cur.value, cur.location.to_string());
  }

private:
  std::vector<Token> tokens;
  std::size_t pos;
};
