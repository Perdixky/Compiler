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

auto all_tokens() -> std::array<TokenType, 28> {
  return {
      TokenType::EndOfFile,      TokenType::Identifier,     TokenType::LiteralNumber, TokenType::LiteralString,
      TokenType::LiteralBoolean, TokenType::LiteralNull,    TokenType::KwIf,          TokenType::KwElse,
      TokenType::KwWhile,        TokenType::KwReturn,       TokenType::KwInt,         TokenType::KwString,
      TokenType::KwBool,         TokenType::KwVoid,         TokenType::OpPlus,        TokenType::OpMinus,
      TokenType::OpStar,         TokenType::OpSlash,        TokenType::OpEqual,       TokenType::OpEqualEqual,
      TokenType::OpLess,         TokenType::OpGreater,      TokenType::PuncLeftParen, TokenType::PuncRightParen,
      TokenType::PuncLeftBrace,  TokenType::PuncRightBrace, TokenType::PuncSemicolon, TokenType::PuncComma,
  };
}

auto grammar_nonterminals(const Grammar& grammar) -> std::vector<NonTerminal> {
  std::vector<NonTerminal> result;
  std::unordered_set<NonTerminal> seen;
  for (const auto& rule : grammar.rules()) {
    if (seen.insert(rule.lhs).second) {
      result.push_back(rule.lhs);
    }
  }
  return result;
}

auto has_item(const ItemSet& item_set, std::string_view lhs, std::size_t dot_position, const SymbolList& rhs) -> bool {
  return std::ranges::any_of(item_set, [&](const ItemSet::Item& item) {
    return item.dot_position_ == dot_position && item.rule_.lhs.name == lhs && item.rule_.rhs == rhs;
  });
}

auto has_item(const ItemSet& item_set, std::string_view lhs, std::size_t dot_position, const SymbolList& rhs,
              TokenType expected) -> bool {
  return std::ranges::any_of(item_set, [&](const ItemSet::Item& item) {
    return item.dot_position_ == dot_position && item.rule_.lhs.name == lhs && item.rule_.rhs == rhs &&
           item.expected_ == Terminal{expected};
  });
}

auto item_sets_equivalent(const ItemSet& lhs, const ItemSet& rhs) -> bool {
  return lhs.size() == rhs.size() && std::ranges::all_of(lhs, [&](const ItemSet::Item& item) {
           return std::ranges::find(rhs, item) != rhs.end();
         });
}

auto transition_item_set(const ItemSet& item_set, const Symbol& symbol, const Grammar& grammar) -> ItemSet {
  ItemSet result = item_set | std::views::filter([&](const ItemSet::Item& item) {
                     return item.dot_position_ < item.rule_.rhs.size() && item.rule_.rhs[item.dot_position_] == symbol;
                   }) |
                   std::views::transform([](ItemSet::Item item) {
                     ++item.dot_position_;
                     return item;
                   }) |
                   std::ranges::to<ItemSet>();
  result.closure(grammar);
  return result;
}

auto find_item_set(const std::vector<ItemSet>& item_sets, const ItemSet& target) -> std::size_t {
  const auto it = std::ranges::find_if(item_sets, [&](const ItemSet& item_set) {
    return item_sets_equivalent(item_set, target);
  });
  if (it == item_sets.end()) {
    return std::numeric_limits<std::size_t>::max();
  }
  return static_cast<std::size_t>(it - item_sets.begin());
}

auto find_reduce_item(const ItemSetCollection& collection, std::size_t state, TokenType lookahead)
    -> const ItemSet::Item* {
  const ItemSet::Item* result = nullptr;
  for (const auto& item : collection.itemSets()[state]) {
    if (item.dot_position_ == item.rule_.rhs.size() && item.rule_.lhs.name != "Start" &&
        item.expected_ == Terminal{lookahead}) {
      if (result != nullptr) {
        return nullptr;
      }
      result = &item;
    }
  }
  return result;
}

auto lr_accepts(const Grammar& grammar, std::vector<TokenType> input) -> bool {
  ItemSetCollection collection{grammar};
  auto [action_table, goto_table] = collection.buildTables(grammar);
  constexpr auto missing = std::numeric_limits<std::size_t>::max();

  input.push_back(TokenType::EndOfFile);
  std::vector<std::size_t> stack{0};
  std::size_t cursor = 0;
  for (std::size_t steps = 0; steps < 4096; ++steps) {
    const std::size_t state = stack.back();
    const TokenType lookahead = input[cursor];
    const std::pair<ActionType, std::size_t> entry = action_table[state, Terminal{lookahead}];
    const auto [action, target] = entry;

    if (action == ActionType::Error) {
      return false;
    }
    if (action == ActionType::Accept) {
      return lookahead == TokenType::EndOfFile && cursor == input.size() - 1;
    }
    if (action == ActionType::Shift) {
      if (target == missing || cursor + 1 >= input.size()) {
        return false;
      }
      stack.push_back(target);
      ++cursor;
      continue;
    }

    if (target != state) {
      return false;
    }
    const ItemSet::Item* item = find_reduce_item(collection, state, lookahead);
    if (item == nullptr || item->rule_.rhs.size() >= stack.size()) {
      return false;
    }
    stack.resize(stack.size() - item->rule_.rhs.size());
    const std::size_t next = goto_table[stack.back(), item->rule_.lhs];
    if (next == missing) {
      return false;
    }
    stack.push_back(next);
  }

  return false;
}

} // namespace

suite<"lr grammar"> lr_grammar_tests = [] {
  "keeps the original grammar patterns"_test = [] {
    Grammar grammar;

    expect(grammar.rules().size() == 16_i);
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
                            "FuncCallStmtNt",
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

    const auto* func_call_stmt_rule = find_rule(grammar, "FuncCallStmtNt");
    expect(func_call_stmt_rule != nullptr);
    if (func_call_stmt_rule == nullptr) {
      return;
    }

    const auto func_call_stmt_sequence = sequence_leaves(func_call_stmt_rule->rhs);
    expect(func_call_stmt_sequence.size() == 3_i);
    expect(is_nonterminal(*func_call_stmt_sequence[0], "Id"));
    expect(is_terminal(*func_call_stmt_sequence[1], TokenType::PuncLeftParen));
    expect(is_terminal(*func_call_stmt_sequence[2], TokenType::PuncRightParen));
  };

  "stores the expanded grammar after compile"_test = [] {
    Grammar grammar;

    grammar.compile();

    expect(grammar.compiled_rules().size() == 30_i);

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

    expect(rhs_equals(rhs_for(grammar, "FuncCallStmtNt"), std::vector<SymbolList>{
                                                              {
                                                                  symbol("Id"),
                                                                  symbol(TokenType::PuncLeftParen),
                                                                  symbol(TokenType::PuncRightParen),
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
    expect(grammar.compiled_rules().size() == 30_i);
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

  "computes lr1 lookahead through nullable suffixes"_test = [] {
    Grammar grammar;
    grammar.compile();

    const auto first = grammar.first(SymbolList{symbol("FuncDeclList"), symbol(TokenType::EndOfFile)});

    expect(std::ranges::find(first, Terminal{TokenType::KwInt}) != first.end());
    expect(std::ranges::find(first, Terminal{TokenType::KwString}) != first.end());
    expect(std::ranges::find(first, Terminal{TokenType::KwBool}) != first.end());
    expect(std::ranges::find(first, Terminal{TokenType::KwVoid}) != first.end());
    expect(std::ranges::find(first, Terminal{TokenType::EndOfFile}) != first.end());
    expect(first.size() == 5_i);

    const auto deduplicated = grammar.first(SymbolList{symbol("FuncDeclList"), symbol(TokenType::KwInt)});
    expect(std::ranges::count(deduplicated, Terminal{TokenType::KwInt}) == 1_i);
  };

  "terminals and nonterminals deduplicate in unordered sets"_test = [] {
    std::unordered_set<Terminal> terminals;
    terminals.insert(Terminal{TokenType::KwInt});
    terminals.insert(Terminal{TokenType::KwInt});
    terminals.insert(Terminal{TokenType::KwString});

    expect(terminals.size() == 2_i);
    expect(terminals.contains(Terminal{TokenType::KwInt}));
    expect(terminals.contains(Terminal{TokenType::KwString}));

    std::unordered_set<NonTerminal> nonterminals;
    nonterminals.insert(NonTerminal{"Type"});
    nonterminals.insert(NonTerminal{"Type"});
    nonterminals.insert(NonTerminal{"Expr"});

    expect(nonterminals.size() == 2_i);
    expect(nonterminals.contains(NonTerminal{"Type"}));
    expect(nonterminals.contains(NonTerminal{"Expr"}));
  };

  "item set is constructible and comparable as a range"_test = [] {
    const std::vector<ItemSet::Item> items{
        ItemSet::Item{.dot_position_ = 0,
                      .rule_ =
                          CompiledRule{
                              .lhs = NonTerminal{"Type"},
                              .rhs = {symbol(TokenType::KwInt)},
                          },
                      .expected_ = Terminal{TokenType::Identifier}},
        ItemSet::Item{.dot_position_ = 0,
                      .rule_ =
                          CompiledRule{
                              .lhs = NonTerminal{"Type"},
                              .rhs = {symbol(TokenType::KwString)},
                          },
                      .expected_ = Terminal{TokenType::Identifier}},
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
                      .rule_ =
                          CompiledRule{
                              .lhs = NonTerminal{"Type"},
                              .rhs = {symbol(TokenType::KwInt)},
                          },
                      .expected_ = Terminal{TokenType::Identifier}},
        ItemSet::Item{.dot_position_ = 1,
                      .rule_ =
                          CompiledRule{
                              .lhs = NonTerminal{"Type"},
                              .rhs = {symbol(TokenType::KwString)},
                          },
                      .expected_ = Terminal{TokenType::Identifier}},
    }};

    expect(bool{from_view == same_items});

    ItemSet reordered_items{std::vector<ItemSet::Item>{
        ItemSet::Item{.dot_position_ = 1,
                      .rule_ =
                          CompiledRule{
                              .lhs = NonTerminal{"Type"},
                              .rhs = {symbol(TokenType::KwString)},
                          },
                      .expected_ = Terminal{TokenType::Identifier}},
        ItemSet::Item{.dot_position_ = 1,
                      .rule_ =
                          CompiledRule{
                              .lhs = NonTerminal{"Type"},
                              .rhs = {symbol(TokenType::KwInt)},
                          },
                      .expected_ = Terminal{TokenType::Identifier}},
    }};

    expect(bool{from_view == reordered_items});
  };

  "initial item set closure expands reachable nonterminals"_test = [] {
    Grammar grammar;
    grammar.compile();

    ItemSet item_set = ItemSet::initial();
    expect(item_set.size() == 1_i);

    item_set.closure(grammar);

    expect(item_set.size() == 12_i);
    expect(has_item(item_set, "Start", 0, SymbolList{symbol("FuncDeclList")}, TokenType::EndOfFile));
    expect(has_item(item_set, "FuncDeclList", 0, SymbolList{}, TokenType::EndOfFile));
    expect(has_item(item_set, "FuncDeclList", 0, SymbolList{symbol("FuncDecl"), symbol("FuncDeclList")},
                    TokenType::EndOfFile));
    expect(has_item(item_set, "FuncDecl", 0,
                    SymbolList{
                        symbol("Type"),
                        symbol("Id"),
                        symbol(TokenType::PuncLeftParen),
                        symbol(TokenType::PuncRightParen),
                        symbol("BlockStmt"),
                    },
                    TokenType::KwInt));
    expect(has_item(item_set, "FuncDecl", 0,
                    SymbolList{
                        symbol("Type"),
                        symbol("Id"),
                        symbol(TokenType::PuncLeftParen),
                        symbol(TokenType::PuncRightParen),
                        symbol("BlockStmt"),
                    },
                    TokenType::KwVoid));
    expect(has_item(item_set, "FuncDecl", 0,
                    SymbolList{
                        symbol("Type"),
                        symbol("Id"),
                        symbol(TokenType::PuncLeftParen),
                        symbol(TokenType::PuncRightParen),
                        symbol("BlockStmt"),
                    },
                    TokenType::EndOfFile));
    expect(has_item(item_set, "Type", 0, SymbolList{symbol(TokenType::KwInt)}, TokenType::Identifier));
    expect(has_item(item_set, "Type", 0, SymbolList{symbol(TokenType::KwString)}, TokenType::Identifier));
    expect(has_item(item_set, "Type", 0, SymbolList{symbol(TokenType::KwBool)}, TokenType::Identifier));
    expect(has_item(item_set, "Type", 0, SymbolList{symbol(TokenType::KwVoid)}, TokenType::Identifier));
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

    const std::pair<ActionType, std::size_t> reduce_entry = action_table[0, Terminal{TokenType::EndOfFile}];
    const auto [reduce_action, reduce_target] = reduce_entry;
    expect(reduce_action == ActionType::Reduce);
    expect(reduce_target == 0_i);

    const std::pair<ActionType, std::size_t> invalid_entry = action_table[0, Terminal{TokenType::LiteralNumber}];
    const auto [invalid_action, invalid_target] = invalid_entry;
    expect(invalid_action == ActionType::Error);
    expect(invalid_target == missing);
  };

  "builds action and goto tables from every generated item set"_test = [] {
    Grammar grammar;
    grammar.compile();

    ItemSetCollection collection{grammar};
    auto [action_table, goto_table] = collection.buildTables(grammar);
    const auto& item_sets = collection.itemSets();
    constexpr auto missing = std::numeric_limits<std::size_t>::max();

    expect(!item_sets.empty());

    for (const auto& [state, item_set] : std::views::enumerate(item_sets)) {
      const std::size_t state_index = static_cast<std::size_t>(state);
      std::unordered_map<Terminal, std::pair<ActionType, std::size_t>> expected_actions;
      std::unordered_map<NonTerminal, std::size_t> expected_gotos;

      for (const auto& item : item_set) {
        if (item.dot_position_ == item.rule_.rhs.size()) {
          const auto expected = item.rule_.lhs.name == "Start" ? std::make_pair(ActionType::Accept, missing)
                                                               : std::make_pair(ActionType::Reduce, state_index);
          const auto [it, inserted] = expected_actions.emplace(item.expected_, expected);
          expect(inserted || it->second == expected);
          continue;
        }

        const Symbol& symbol = item.rule_.rhs[item.dot_position_];
        const ItemSet target = transition_item_set(item_set, symbol, grammar);
        const std::size_t target_index = find_item_set(item_sets, target);
        expect(target_index != missing);

        if (const auto* terminal = std::get_if<Terminal>(&symbol)) {
          const auto expected = std::make_pair(ActionType::Shift, target_index);
          const auto [it, inserted] = expected_actions.emplace(*terminal, expected);
          expect(inserted || it->second == expected);
        } else {
          const auto [it, inserted] = expected_gotos.emplace(std::get<NonTerminal>(symbol), target_index);
          expect(inserted || it->second == target_index);
        }
      }

      for (TokenType token : all_tokens()) {
        const std::pair<ActionType, std::size_t> actual = action_table[state_index, Terminal{token}];
        const auto it = expected_actions.find(Terminal{token});
        const auto expected = it == expected_actions.end() ? std::make_pair(ActionType::Error, missing) : it->second;
        expect(actual == expected);
      }

      for (const NonTerminal& nonterminal : grammar_nonterminals(grammar)) {
        const std::size_t actual = goto_table[state_index, nonterminal];
        const auto it = expected_gotos.find(nonterminal);
        const std::size_t expected = it == expected_gotos.end() ? missing : it->second;
        expect(actual == expected);
      }
    }
  };

  "generated lr table accepts valid programs and rejects invalid ones"_test = [] {
    Grammar grammar;
    grammar.compile();

    expect(lr_accepts(grammar, {}));
    expect(lr_accepts(grammar, {
                                   TokenType::KwInt,
                                   TokenType::Identifier,
                                   TokenType::PuncLeftParen,
                                   TokenType::PuncRightParen,
                                   TokenType::PuncLeftBrace,
                                   TokenType::PuncRightBrace,
                               }));
    expect(lr_accepts(grammar, {
                                   TokenType::KwVoid,
                                   TokenType::Identifier,
                                   TokenType::PuncLeftParen,
                                   TokenType::PuncRightParen,
                                   TokenType::PuncLeftBrace,
                                   TokenType::KwInt,
                                   TokenType::Identifier,
                                   TokenType::PuncSemicolon,
                                   TokenType::Identifier,
                                   TokenType::OpEqual,
                                   TokenType::Identifier,
                                   TokenType::OpPlus,
                                   TokenType::Identifier,
                                   TokenType::PuncSemicolon,
                                   TokenType::KwReturn,
                                   TokenType::Identifier,
                                   TokenType::PuncSemicolon,
                                   TokenType::PuncRightBrace,
                               }));
    expect(lr_accepts(grammar, {
                                   TokenType::KwBool,
                                   TokenType::Identifier,
                                   TokenType::PuncLeftParen,
                                   TokenType::PuncRightParen,
                                   TokenType::PuncLeftBrace,
                                   TokenType::KwIf,
                                   TokenType::PuncLeftParen,
                                   TokenType::Identifier,
                                   TokenType::PuncRightParen,
                                   TokenType::PuncLeftBrace,
                                   TokenType::PuncRightBrace,
                                   TokenType::PuncRightBrace,
                               }));
    expect(lr_accepts(grammar, {
                                   TokenType::KwString,
                                   TokenType::Identifier,
                                   TokenType::PuncLeftParen,
                                   TokenType::PuncRightParen,
                                   TokenType::PuncLeftBrace,
                                   TokenType::PuncRightBrace,
                                   TokenType::KwInt,
                                   TokenType::Identifier,
                                   TokenType::PuncLeftParen,
                                   TokenType::PuncRightParen,
                                   TokenType::PuncLeftBrace,
                                   TokenType::PuncRightBrace,
                               }));

    expect(!lr_accepts(grammar, {TokenType::LiteralNumber}));
    expect(!lr_accepts(grammar, {
                                    TokenType::KwInt,
                                    TokenType::Identifier,
                                    TokenType::PuncLeftParen,
                                    TokenType::PuncRightParen,
                                    TokenType::PuncLeftBrace,
                                    TokenType::KwInt,
                                    TokenType::Identifier,
                                    TokenType::PuncRightBrace,
                                }));
    expect(!lr_accepts(grammar, {
                                    TokenType::KwInt,
                                    TokenType::Identifier,
                                    TokenType::PuncLeftParen,
                                    TokenType::PuncRightParen,
                                    TokenType::PuncLeftBrace,
                                    TokenType::Identifier,
                                    TokenType::PuncLeftParen,
                                    TokenType::PuncRightParen,
                                    TokenType::PuncSemicolon,
                                    TokenType::PuncRightBrace,
                                }));
  };
};
