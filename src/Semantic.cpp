module;
#include <proxy/proxy.h>

export module Semantic;
import std;
import Parser;
import Fatal;

export struct SymbolInfo {
  std::string name;
  Type type;
  std::string scope;
};

export struct Quad {
  std::string op;
  std::string arg1;
  std::string arg2;
  std::string result;
};

export struct SemanticResult {
  std::unordered_map<std::string, SymbolInfo> symbols;
  std::vector<Quad> code;
};

export class SemanticAnalyzer {
public:
  auto analyze(this SemanticAnalyzer& self, const std::vector<ProDecl>& decls) -> SemanticResult {
    self.result_ = SemanticResult{};
    self.temp_index_ = 0;
    self.label_index_ = 0;
    self.current_scope_ = "global";

    for (const auto& decl : decls) {
      self.emit_decl(decl);
    }

    return std::move(self.result_);
  }

private:
  auto emit_decl(this SemanticAnalyzer& self, const ProDecl& decl) -> void {
    if (const auto* var = proxy_cast<const VarDecl>(&*decl)) {
      self.declare_symbol(std::string{var->name}, var->type);
      if (var->initializer.has_value()) {
        const std::string value = self.emit_expr(var->initializer.value());
        self.emit("assign", value, "", std::string{var->name});
      }
      return;
    }

    if (const auto* func = proxy_cast<const FuncDecl>(&*decl)) {
      const std::string previous_scope = self.current_scope_;
      self.declare_symbol(std::string{func->name}, func->return_type, "global");
      self.current_scope_ = std::string{func->name};
      self.emit("func", "", "", std::string{func->name});

      for (const auto& parameter : func->parameters) {
        const auto& var = proxy_cast<const VarDecl&>(*parameter);
        self.declare_symbol(std::string{var.name}, var.type);
        self.emit("param", "", "", std::string{var.name});
      }

      self.emit_stmt(func->body);
      self.emit("end", "", "", std::string{func->name});
      self.current_scope_ = previous_scope;
      return;
    }

    fatal("Unsupported declaration in semantic analyzer");
  }

  auto emit_stmt(this SemanticAnalyzer& self, const ProStmt& stmt) -> void {
    if (const auto* block = proxy_cast<const BlockStmt>(&*stmt)) {
      for (const auto& child : block->statements) {
        self.emit_stmt(child);
      }
      return;
    }

    if (const auto* decl = proxy_cast<const DeclStmt>(&*stmt)) {
      self.emit_decl(decl->decl);
      return;
    }

    if (const auto* assign = proxy_cast<const AssignStmt>(&*stmt)) {
      self.require_symbol(std::string{assign->variable_name});
      const std::string value = self.emit_expr(assign->value);
      self.emit("assign", value, "", std::string{assign->variable_name});
      return;
    }

    if (const auto* ret = proxy_cast<const ReturnStmt>(&*stmt)) {
      if (ret->value.has_value()) {
        self.emit("return", self.emit_expr(ret->value.value()), "", "");
      } else {
        self.emit("return", "", "", "");
      }
      return;
    }

    if (const auto* if_stmt = proxy_cast<const IfStmt>(&*stmt)) {
      const std::string done = self.new_label();
      const std::string condition = self.emit_expr(if_stmt->condition);
      self.emit("if_false", condition, "", done);
      self.emit_stmt(if_stmt->then_branch);

      if (if_stmt->else_branch.has_value()) {
        const std::string after_else = self.new_label();
        self.emit("goto", "", "", after_else);
        self.emit("label", "", "", done);
        self.emit_stmt(if_stmt->else_branch.value());
        self.emit("label", "", "", after_else);
      } else {
        self.emit("label", "", "", done);
      }
      return;
    }

    if (const auto* call = proxy_cast<const FuncCallStmt>(&*stmt)) {
      for (const auto& argument : call->arguments) {
        self.emit("arg", self.emit_expr(argument), "", "");
      }
      self.emit("call", std::to_string(call->arguments.size()), "", std::string{call->function_name});
      return;
    }

    fatal("Unsupported statement in semantic analyzer");
  }

  auto emit_expr(this SemanticAnalyzer& self, const ProExpr& expr) -> std::string {
    if (const auto* literal = proxy_cast<const LiteralNumExpr>(&*expr)) {
      return std::to_string(literal->value);
    }

    if (const auto* var = proxy_cast<const VarExpr>(&*expr)) {
      self.require_symbol(std::string{var->name});
      return std::string{var->name};
    }

    if (const auto* binary = proxy_cast<const BinaryExpr>(&*expr)) {
      const std::string left = self.emit_expr(binary->left);
      const std::string right = self.emit_expr(binary->right);
      const std::string target = self.new_temp();
      self.emit(self.binary_op(binary->op), left, right, target);
      return target;
    }

    fatal("Unsupported expression in semantic analyzer");
  }

  auto binary_op(this SemanticAnalyzer&, BinaryExpr::BinaryOp op) -> std::string {
    switch (op) {
    case BinaryExpr::BinaryOp::Add:
      return "+";
    case BinaryExpr::BinaryOp::Subtract:
      return "-";
    case BinaryExpr::BinaryOp::Multiply:
      return "*";
    case BinaryExpr::BinaryOp::Divide:
      return "/";
    case BinaryExpr::BinaryOp::Less:
      return "<";
    case BinaryExpr::BinaryOp::Greater:
      return ">";
    case BinaryExpr::BinaryOp::Equal:
      return "==";
    }
    std::unreachable();
  }

  auto declare_symbol(this SemanticAnalyzer& self, std::string name, Type type) -> void {
    self.declare_symbol(std::move(name), type, self.current_scope_);
  }

  auto declare_symbol(this SemanticAnalyzer& self, std::string name, Type type, std::string scope) -> void {
    if (self.result_.symbols.contains(name)) {
      fatal("Duplicate symbol: {}", name);
    }
    self.result_.symbols.emplace(name, SymbolInfo{.name = name, .type = type, .scope = std::move(scope)});
  }

  auto require_symbol(this SemanticAnalyzer& self, const std::string& name) -> void {
    if (!self.result_.symbols.contains(name)) {
      fatal("Undefined symbol: {}", name);
    }
  }

  auto emit(this SemanticAnalyzer& self, std::string op, std::string arg1, std::string arg2, std::string result) -> void {
    self.result_.code.push_back(Quad{
        .op = std::move(op),
        .arg1 = std::move(arg1),
        .arg2 = std::move(arg2),
        .result = std::move(result),
    });
  }

  auto new_temp(this SemanticAnalyzer& self) -> std::string {
    return std::format("t{}", self.temp_index_++);
  }

  auto new_label(this SemanticAnalyzer& self) -> std::string {
    return std::format("L{}", self.label_index_++);
  }

private:
  SemanticResult result_;
  std::size_t temp_index_ = 0;
  std::size_t label_index_ = 0;
  std::string current_scope_ = "global";
};
