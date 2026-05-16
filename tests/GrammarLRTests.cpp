#include <boost/ut.hpp>

import std;
import Parser.LR;
import Token;

using namespace boost::ut;

namespace {

auto find_rule(const Grammar& grammar, std::string_view name) -> const Rule* {
  const auto& rules = grammar.rules();
  const auto it = std::ranges::find_if(rules, [name](const Rule& rule) {
    return rule.lhs.name == name;
  });

  if (it == rules.end()) {
    return nullptr;
  }

  return &*it;
}

auto is_nonterminal(const Pattern& pattern, std::string_view name) -> bool {
  const auto* one = std::get_if<Pattern::One>(&pattern.node());
  if (one == nullptr) {
    return false;
  }

  const auto* nonterminal = std::get_if<NonTerminal>(&one->symbol);
  return nonterminal != nullptr && nonterminal->name == name;
}

auto is_terminal(const Pattern& pattern, TokenType type) -> bool {
  const auto* one = std::get_if<Pattern::One>(&pattern.node());
  if (one == nullptr) {
    return false;
  }

  const auto* terminal = std::get_if<Terminal>(&one->symbol);
  return terminal != nullptr && terminal->type == type;
}

void collect_sequence_leaves(const Pattern& pattern, std::vector<const Pattern*>& result) {
  if (const auto* sequence = std::get_if<Pattern::Sequence>(&pattern.node())) {
    for (const auto& part : sequence->parts) {
      collect_sequence_leaves(part, result);
    }
    return;
  }

  result.push_back(&pattern);
}

auto sequence_leaves(const Pattern& pattern) -> std::vector<const Pattern*> {
  std::vector<const Pattern*> result;
  collect_sequence_leaves(pattern, result);
  return result;
}

void collect_alternative_leaves(const Pattern& pattern, std::vector<const Pattern*>& result) {
  if (const auto* alternative = std::get_if<Pattern::Alternative>(&pattern.node())) {
    for (const auto& choice : alternative->choices) {
      collect_alternative_leaves(choice, result);
    }
    return;
  }

  result.push_back(&pattern);
}

auto alternative_leaves(const Pattern& pattern) -> std::vector<const Pattern*> {
  std::vector<const Pattern*> result;
  collect_alternative_leaves(pattern, result);
  return result;
}

auto symbol(TokenType type) -> Symbol {
  return Terminal{type};
}

auto symbol(std::string name) -> Symbol {
  return NonTerminal{std::move(name)};
}

auto rhs_for(const Grammar& grammar, std::string_view name) -> std::vector<SymbolList> {
  std::vector<SymbolList> result;

  for (const auto& rule : grammar.compiled_rules()) {
    if (rule.lhs.name == name) {
      result.push_back(rule.rhs);
    }
  }

  return result;
}

auto rhs_equals(const std::vector<SymbolList>& actual, const std::vector<SymbolList>& expected) -> bool {
  return actual == expected;
}

auto has_item(const ItemSet& item_set, std::string_view lhs, std::size_t dot_position, const SymbolList& rhs) -> bool {
  return std::ranges::any_of(item_set, [&](const ItemSet::Item& item) {
    return item.dot_position_ == dot_position && item.rule_.lhs.name == lhs && item.rule_.rhs == rhs;
  });
}

} // namespace

suite<"lr grammar"> lr_grammar_tests = [] {
  "keeps the original grammar patterns"_test = [] {
    Grammar grammar;

    expect(grammar.rules().size() == 15_i);
    expect(grammar.compiled_rules().empty());

    std::vector<std::string> lhs_names;
    for (const auto& rule : grammar.rules()) {
      lhs_names.push_back(rule.lhs.name);
    }

    expect(lhs_names == std::vector<std::string>{
                            "Expr",
                            "Decl",
                            "DeclList",
                            "Stmt",
                            "StmtList",
                            "FuncDecl",
                            "FuncDeclList",
                            "IfStmt",
                            "ReturnStmt",
                            "AssignStmt",
                            "DeclStmt",
                            "BlockStmt",
                            "Type",
                            "Id",
                            "Op",
                        });

    const auto* expr_rule = find_rule(grammar, "Expr");
    expect(expr_rule != nullptr);
    if (expr_rule == nullptr) {
      return;
    }

    const auto* expr = std::get_if<Pattern::Alternative>(&expr_rule->rhs.node());
    expect(expr != nullptr);
    if (expr == nullptr) {
      return;
    }

    expect(expr->choices.size() == 2_i);
    expect(is_nonterminal(expr->choices[0], "Id"));

    const auto* binary_expr_root = std::get_if<Pattern::Sequence>(&expr->choices[1].node());
    expect(binary_expr_root != nullptr);
    if (binary_expr_root == nullptr) {
      return;
    }

    expect(binary_expr_root->parts.size() == 2_i);

    const auto binary_expr = sequence_leaves(expr->choices[1]);
    expect(binary_expr.size() == 3_i);
    expect(is_nonterminal(*binary_expr[0], "Id"));
    expect(is_nonterminal(*binary_expr[1], "Op"));
    expect(is_nonterminal(*binary_expr[2], "Id"));

    const auto* decl_rule = find_rule(grammar, "Decl");
    expect(decl_rule != nullptr);
    if (decl_rule == nullptr) {
      return;
    }

    const auto* decl = std::get_if<Pattern::Sequence>(&decl_rule->rhs.node());
    expect(decl != nullptr);
    if (decl == nullptr) {
      return;
    }

    expect(decl->parts.size() == 2_i);

    const auto decl_parts = sequence_leaves(decl_rule->rhs);
    expect(decl_parts.size() == 3_i);
    expect(is_nonterminal(*decl_parts[0], "Type"));
    expect(is_nonterminal(*decl_parts[1], "Id"));

    const auto* initializer = std::get_if<Pattern::Alternative>(&decl_parts[2]->node());
    expect(initializer != nullptr);
    if (initializer == nullptr) {
      return;
    }

    expect(initializer->choices.size() == 2_i);
    const auto* initializer_expr = std::get_if<Pattern::Sequence>(&initializer->choices[0].node());
    expect(initializer_expr != nullptr);
    if (initializer_expr == nullptr) {
      return;
    }

    expect(initializer_expr->parts.size() == 2_i);
    expect(is_terminal(initializer_expr->parts[0], TokenType::OpEqual));
    expect(is_nonterminal(initializer_expr->parts[1], "Expr"));
    expect(initializer->choices[1].is_epsilon());

    const auto* type_rule = find_rule(grammar, "Type");
    expect(type_rule != nullptr);
    if (type_rule == nullptr) {
      return;
    }

    const auto* type = std::get_if<Pattern::Alternative>(&type_rule->rhs.node());
    expect(type != nullptr);
    if (type == nullptr) {
      return;
    }

    expect(type->choices.size() == 2_i);

    const auto type_choices = alternative_leaves(type_rule->rhs);
    expect(type_choices.size() == 4_i);
    expect(is_terminal(*type_choices[0], TokenType::KwInt));
    expect(is_terminal(*type_choices[1], TokenType::KwString));
    expect(is_terminal(*type_choices[2], TokenType::KwBool));
    expect(is_terminal(*type_choices[3], TokenType::KwVoid));

    const auto* func_decl_list_rule = find_rule(grammar, "FuncDeclList");
    expect(func_decl_list_rule != nullptr);
    if (func_decl_list_rule == nullptr) {
      return;
    }

    const auto func_decl_list_choices = alternative_leaves(func_decl_list_rule->rhs);
    expect(func_decl_list_choices.size() == 2_i);
    expect(func_decl_list_choices[0]->is_epsilon());

    const auto func_decl_list_sequence = sequence_leaves(*func_decl_list_choices[1]);
    expect(func_decl_list_sequence.size() == 2_i);
    expect(is_nonterminal(*func_decl_list_sequence[0], "FuncDecl"));
    expect(is_nonterminal(*func_decl_list_sequence[1], "FuncDeclList"));
  };

  "stores the expanded grammar after compile"_test = [] {
    Grammar grammar;

    grammar.compile();

    expect(grammar.compiled_rules().size() == 29_i);

    expect(rhs_equals(rhs_for(grammar, "Decl"), std::vector<SymbolList>{
                                                    {
                                                        symbol("Type"),
                                                        symbol("Id"),
                                                        symbol(TokenType::OpEqual),
                                                        symbol("Expr"),
                                                    },
                                                    {
                                                        symbol("Type"),
                                                        symbol("Id"),
                                                    },
                                                }));

    expect(rhs_equals(rhs_for(grammar, "StmtList"), std::vector<SymbolList>{
                                                        {},
                                                        {
                                                            symbol("Stmt"),
                                                            symbol("StmtList"),
                                                        },
                                                    }));

    expect(rhs_equals(rhs_for(grammar, "FuncDeclList"), std::vector<SymbolList>{
                                                            {},
                                                            {
                                                                symbol("FuncDecl"),
                                                                symbol("FuncDeclList"),
                                                            },
                                                        }));

    expect(rhs_equals(rhs_for(grammar, "Type"), std::vector<SymbolList>{
                                                    {symbol(TokenType::KwInt)},
                                                    {symbol(TokenType::KwString)},
                                                    {symbol(TokenType::KwBool)},
                                                    {symbol(TokenType::KwVoid)},
                                                }));

    expect(rhs_equals(rhs_for(grammar, "Op"), std::vector<SymbolList>{
                                                  {symbol(TokenType::OpPlus)},
                                                  {symbol(TokenType::OpMinus)},
                                                  {symbol(TokenType::OpStar)},
                                                  {symbol(TokenType::OpSlash)},
                                              }));

    grammar.compile();
    expect(grammar.compiled_rules().size() == 29_i);
  };

  "finds compiled rules through a lazy range"_test = [] {
    Grammar grammar;
    grammar.compile();

    std::vector<SymbolList> type_rules;
    for (const auto& rule : grammar.findCompiledRules(NonTerminal{"Type"})) {
      type_rules.push_back(rule.rhs);
    }

    expect(type_rules == std::vector<SymbolList>{
                             {symbol(TokenType::KwInt)},
                             {symbol(TokenType::KwString)},
                             {symbol(TokenType::KwBool)},
                             {symbol(TokenType::KwVoid)},
                         });

    std::size_t missing_count = 0;
    for ([[maybe_unused]] const auto& rule : grammar.findCompiledRules(NonTerminal{"Missing"})) {
      ++missing_count;
    }

    expect(missing_count == 0_i);
  };

  "item set is constructible and comparable as a range"_test = [] {
    const std::vector<ItemSet::Item> items{
        ItemSet::Item{.dot_position_ = 0,
                      .rule_ = CompiledRule{
                          .lhs = NonTerminal{"Type"},
                          .rhs = {symbol(TokenType::KwInt)},
                      }},
        ItemSet::Item{.dot_position_ = 0,
                      .rule_ = CompiledRule{
                          .lhs = NonTerminal{"Type"},
                          .rhs = {symbol(TokenType::KwString)},
                      }},
    };

    ItemSet from_view{items | std::views::filter([](const ItemSet::Item& item) {
                        return item.rule_.lhs.name == "Type";
                      }) |
                      std::views::transform([](ItemSet::Item item) {
                        ++item.dot_position_;
                        return item;
                      })};

    expect(from_view.size() == 2_i);
    expect(std::ranges::distance(from_view) == 2_i);
    expect(has_item(from_view, "Type", 1, SymbolList{symbol(TokenType::KwInt)}));
    expect(has_item(from_view, "Type", 1, SymbolList{symbol(TokenType::KwString)}));

    ItemSet same_items{std::vector<ItemSet::Item>{
        ItemSet::Item{.dot_position_ = 1,
                      .rule_ = CompiledRule{
                          .lhs = NonTerminal{"Type"},
                          .rhs = {symbol(TokenType::KwInt)},
                      }},
        ItemSet::Item{.dot_position_ = 1,
                      .rule_ = CompiledRule{
                          .lhs = NonTerminal{"Type"},
                          .rhs = {symbol(TokenType::KwString)},
                      }},
    }};

    expect(bool{from_view == same_items});
  };

  "initial item set closure expands reachable nonterminals"_test = [] {
    Grammar grammar;
    grammar.compile();

    ItemSet item_set = ItemSet::initial();
    expect(item_set.size() == 1_i);

    item_set.closure(grammar);

    expect(item_set.size() == 8_i);
    expect(has_item(item_set, "Start", 0, SymbolList{symbol("FuncDeclList")}));
    expect(has_item(item_set, "FuncDeclList", 0, SymbolList{}));
    expect(has_item(item_set, "FuncDeclList", 0, SymbolList{symbol("FuncDecl"), symbol("FuncDeclList")}));
    expect(has_item(item_set,
                    "FuncDecl",
                    0,
                    SymbolList{
                        symbol("Type"),
                        symbol("Id"),
                        symbol(TokenType::PuncLeftParen),
                        symbol(TokenType::PuncRightParen),
                        symbol("BlockStmt"),
                    }));
    expect(has_item(item_set, "Type", 0, SymbolList{symbol(TokenType::KwInt)}));
    expect(has_item(item_set, "Type", 0, SymbolList{symbol(TokenType::KwString)}));
    expect(has_item(item_set, "Type", 0, SymbolList{symbol(TokenType::KwBool)}));
    expect(has_item(item_set, "Type", 0, SymbolList{symbol(TokenType::KwVoid)}));
  };

  "builds lr action and goto entries from item set collection"_test = [] {
    Grammar grammar;
    grammar.compile();

    ItemSetCollection collection{grammar};
    auto [action_table, goto_table] = collection.buildTables(grammar);
    constexpr auto missing = std::numeric_limits<std::size_t>::max();

    const std::pair<ActionType, std::size_t> kw_int_entry = action_table[0, Terminal{TokenType::KwInt}];
    const auto [kw_int_action, kw_int_target] = kw_int_entry;
    expect(kw_int_action == ActionType::Shift);
    expect(kw_int_target != missing);

    const std::size_t func_decl_list_state = goto_table[0, NonTerminal{"FuncDeclList"}];
    expect(func_decl_list_state != missing);
    expect(goto_table[0, NonTerminal{"FuncDecl"}] != missing);
    expect(goto_table[0, NonTerminal{"Type"}] != missing);

    const std::pair<ActionType, std::size_t> eof_entry =
        action_table[func_decl_list_state, Terminal{TokenType::EndOfFile}];
    const auto [eof_action, eof_target] = eof_entry;
    expect(eof_action == ActionType::Accept);
    expect(eof_target == missing);

    const std::pair<ActionType, std::size_t> reduce_entry = action_table[0, Terminal{TokenType::LiteralNumber}];
    const auto [reduce_action, reduce_target] = reduce_entry;
    expect(reduce_action == ActionType::Reduce);
    expect(reduce_target == 0_i);
  };
};
