module;

#include <cstddef>
#include <rfl.hpp>

export module Parser.LR;
import std;
import Token;
import ankerl.unordered_dense;

export struct Terminal {
  TokenType type;

  friend bool operator==(const Terminal&, const Terminal&) = default;

  auto hash(this const auto& self) -> std::size_t {
    return std::hash<int>()(static_cast<int>(self.type));
  }
};

export struct NonTerminal {
  std::string name;

  friend bool operator==(const NonTerminal&, const NonTerminal&) = default;
  auto hash(this const auto& self) -> std::size_t {
    return std::hash<std::string>()(self.name);
  }
};

export using Symbol = std::variant<Terminal, NonTerminal>;

export class Pattern {
public:
  struct Epsilon {};

  struct One {
    Symbol symbol;
  };

  struct Sequence {
    std::vector<Pattern> parts;
  };

  struct Alternative {
    std::vector<Pattern> choices;
  };

  using Node = std::variant<Epsilon, One, Sequence, Alternative>;

  enum class Kind {
    Epsilon,
    One,
    Sequence,
    Alternative,
  };

public:
  Pattern(Symbol symbol) : node_(One{std::move(symbol)}) {}

  Pattern(Terminal terminal) : Pattern(Symbol{terminal}) {}

  Pattern(NonTerminal nonterminal) : Pattern(Symbol{std::move(nonterminal)}) {}

  static Pattern epsilon() {
    return Pattern(Epsilon{});
  }

  static Pattern terminal(TokenType type) {
    return Pattern(Terminal{type});
  }

  static Pattern nonterminal(std::string name) {
    return Pattern(NonTerminal{std::move(name)});
  }

  Kind kind() const {
    if (std::holds_alternative<Epsilon>(node_)) {
      return Kind::Epsilon;
    }

    if (std::holds_alternative<One>(node_)) {
      return Kind::One;
    }

    if (std::holds_alternative<Sequence>(node_)) {
      return Kind::Sequence;
    }

    return Kind::Alternative;
  }

  bool is_epsilon() const {
    return std::holds_alternative<Epsilon>(node_);
  }

  bool is_one() const {
    return std::holds_alternative<One>(node_);
  }

  bool is_sequence() const {
    return std::holds_alternative<Sequence>(node_);
  }

  bool is_alternative() const {
    return std::holds_alternative<Alternative>(node_);
  }

  auto at(this auto& self, std::size_t index) -> const Symbol& {
    if (const auto* sequence = std::get_if<Sequence>(&self.node_)) {
      if (index < sequence->parts.size()) {
        const auto& part = sequence->parts[index];
        if (const auto* one = std::get_if<One>(&part.node_)) {
          return one->symbol;
        }
      }
    }
    if (index == 0 && std::holds_alternative<One>(self.node_)) {
      const auto& one = std::get<One>(self.node_);
      return one.symbol;
    }
  }

  const Node& node() const {
    return node_;
  }

  Node& node() {
    return node_;
  }

  friend Pattern operator>>(Pattern lhs, Pattern rhs) {
    return Pattern::make_sequence(std::move(lhs), std::move(rhs));
  }

  friend Pattern operator|(Pattern lhs, Pattern rhs) {
    return Pattern::make_alternative(std::move(lhs), std::move(rhs));
  }

private:
  explicit Pattern(Epsilon epsilon) : node_(epsilon) {}

  explicit Pattern(One one) : node_(std::move(one)) {}

  explicit Pattern(Sequence sequence) : node_(std::move(sequence)) {}

  explicit Pattern(Alternative alternative) : node_(std::move(alternative)) {}

private:
  static Pattern make_sequence(Pattern lhs, Pattern rhs) {
    std::vector<Pattern> parts;
    parts.push_back(std::move(lhs));
    parts.push_back(std::move(rhs));
    return Pattern(Sequence{std::move(parts)});
  }

  static Pattern make_alternative(Pattern lhs, Pattern rhs) {
    std::vector<Pattern> choices;
    choices.push_back(std::move(lhs));
    choices.push_back(std::move(rhs));
    return Pattern(Alternative{std::move(choices)});
  }

private:
  Node node_;
};

export inline Pattern t(TokenType type) {
  return Pattern::terminal(type);
}

export inline Pattern nt(std::string name) {
  return Pattern::nonterminal(std::move(name));
}

export inline Pattern eps() {
  return Pattern::epsilon();
}

export using SymbolList = std::vector<Symbol>;

export struct Rule {
  NonTerminal lhs;
  Pattern rhs;
};

export struct CompiledRule {
  NonTerminal lhs;
  SymbolList rhs;
};

export class Grammar {
public:
  Grammar() {
    const NonTerminal ExprNt{"Expr"};
    const NonTerminal DeclNt{"Decl"};
    const NonTerminal DeclListNt{"DeclList"};
    const NonTerminal StmtNt{"Stmt"};
    const NonTerminal StmtListNt{"StmtList"};
    const NonTerminal FuncDeclNt{"FuncDecl"};
    const NonTerminal FuncDeclListNt{"FuncDeclList"};
    const NonTerminal IfStmtNt{"IfStmt"};
    const NonTerminal ReturnStmtNt{"ReturnStmt"};
    const NonTerminal AssignStmtNt{"AssignStmt"};
    const NonTerminal DeclStmtNt{"DeclStmt"};
    const NonTerminal BlockStmtNt{"BlockStmt"};
    const NonTerminal FuncCallStmtNt{"FuncCallStmtNt"};
    const NonTerminal TypeNt{"Type"};
    const NonTerminal IdNt{"Id"};
    const NonTerminal OpNt{"Op"};

    const Pattern Expr = Pattern(ExprNt);
    const Pattern Decl = Pattern(DeclNt);
    const Pattern DeclList = Pattern(DeclListNt);
    const Pattern Stmt = Pattern(StmtNt);
    const Pattern StmtList = Pattern(StmtListNt);
    const Pattern FuncDecl = Pattern(FuncDeclNt);
    const Pattern FuncDeclList = Pattern(FuncDeclListNt);
    const Pattern FuncCallStmt = Pattern(FuncCallStmtNt);
    const Pattern IfStmt = Pattern(IfStmtNt);
    const Pattern ReturnStmt = Pattern(ReturnStmtNt);
    const Pattern AssignStmt = Pattern(AssignStmtNt);
    const Pattern DeclStmt = Pattern(DeclStmtNt);
    const Pattern BlockStmt = Pattern(BlockStmtNt);
    const Pattern Type = Pattern(TypeNt);
    const Pattern Id = Pattern(IdNt);
    const Pattern Op = Pattern(OpNt);

    const Pattern identifier = t(TokenType::Identifier);

    const Pattern left_paren = t(TokenType::PuncLeftParen);
    const Pattern right_paren = t(TokenType::PuncRightParen);
    const Pattern left_brace = t(TokenType::PuncLeftBrace);
    const Pattern right_brace = t(TokenType::PuncRightBrace);
    const Pattern semicolon = t(TokenType::PuncSemicolon);

    const Pattern equal = t(TokenType::OpEqual);

    const Pattern kw_return = t(TokenType::KwReturn);
    const Pattern kw_if = t(TokenType::KwIf);

    Pattern type_pattern = t(TokenType::KwInt) | t(TokenType::KwString) | t(TokenType::KwBool) | t(TokenType::KwVoid);

    Pattern id_pattern = identifier;

    Pattern op_pattern = t(TokenType::OpPlus) | t(TokenType::OpMinus) | t(TokenType::OpStar) | t(TokenType::OpSlash);

    Pattern expr_pattern = Id | Id >> Op >> Id;

    Pattern decl_pattern = Type >> Id >> ((equal >> Expr) | eps());

    Pattern decl_list_pattern = Decl >> ((semicolon >> DeclList) | eps());

    Pattern decl_stmt_pattern = Decl >> semicolon;

    Pattern assign_stmt_pattern = Id >> equal >> Expr >> semicolon;

    Pattern return_stmt_pattern = kw_return >> Expr >> semicolon;

    Pattern stmt_pattern = DeclStmt | AssignStmt | ReturnStmt | IfStmt;

    Pattern stmt_list_pattern = eps() | Stmt >> StmtList;

    Pattern block_stmt_pattern = left_brace >> StmtList >> right_brace;

    Pattern func_decl_pattern = Type >> Id >> left_paren >> right_paren >> BlockStmt;

    Pattern func_decl_list_pattern = eps() | FuncDecl >> FuncDeclList;

    Pattern func_call_stmt_pattern = Id >> left_paren >> right_paren;

    Pattern if_stmt_pattern = kw_if >> left_paren >> Expr >> right_paren >> BlockStmt;

    rules_.reserve(15);

    add_rule(ExprNt, std::move(expr_pattern));
    add_rule(DeclNt, std::move(decl_pattern));
    add_rule(DeclListNt, std::move(decl_list_pattern));
    add_rule(StmtNt, std::move(stmt_pattern));
    add_rule(StmtListNt, std::move(stmt_list_pattern));
    add_rule(FuncDeclNt, std::move(func_decl_pattern));
    add_rule(FuncDeclListNt, std::move(func_decl_list_pattern));
    add_rule(FuncCallStmtNt, func_call_stmt_pattern);
    add_rule(IfStmtNt, std::move(if_stmt_pattern));
    add_rule(ReturnStmtNt, std::move(return_stmt_pattern));
    add_rule(AssignStmtNt, std::move(assign_stmt_pattern));
    add_rule(DeclStmtNt, std::move(decl_stmt_pattern));
    add_rule(BlockStmtNt, std::move(block_stmt_pattern));
    add_rule(TypeNt, std::move(type_pattern));
    add_rule(IdNt, std::move(id_pattern));
    add_rule(OpNt, std::move(op_pattern));
  }

  void compile() {
    compiled_rules_.clear();
    compiled_rules_.reserve(rules_.size() * 2);

    for (const auto& rule : rules_) {
      for (auto rhs : compile_pattern(rule.rhs)) {
        compiled_rules_.push_back(CompiledRule{
            .lhs = rule.lhs,
            .rhs = std::move(rhs),
        });
      }
    }
  }

  const std::vector<Rule>& rules() const {
    return rules_;
  }

  const std::vector<CompiledRule>& compiled_rules() const {
    return compiled_rules_;
  }

  auto findCompiledRules(const NonTerminal& name) const -> std::ranges::view auto {
    return compiled_rules_ | std::views::filter([name](const CompiledRule& rule) {
             return rule.lhs.name == name.name;
           });
  }

  auto first(this const auto& self, const Symbol& name) -> std::vector<Terminal> {
    if (std::holds_alternative<Terminal>(name)) [[unlikely]] {
      return {std::get<Terminal>(name)};
    }
    std::vector<NonTerminal> first_nonterminals = {std::get<NonTerminal>(name)};
    bool changed;

    std::vector<Terminal> first_terminals;
    first_terminals.reserve(self.compiled_rules_.size());
    while (!first_nonterminals.empty()) {
      auto first_symbols =
          std::ranges::find_if(self.compiled_rules_,
                               [&](const CompiledRule& rule) {
                                 return std::ranges::any_of(first_nonterminals, [&](const NonTerminal& nonterminal) {
                                   return nonterminal == rule.lhs;
                                 });
                               }) |
          std::views::transform([](const CompiledRule& rule) {
            return rule.rhs.front();
          });

      for (const Symbol& symbol : first_symbols) {
        if (std::holds_alternative<NonTerminal>(symbol)) {
          auto nonterminal = std::get<NonTerminal>(symbol);
          if (std::ranges::find(first_nonterminals, nonterminal) == first_nonterminals.end()) {
            first_nonterminals.push_back(nonterminal);
          }
        } else {
          auto terminal = std::get<Terminal>(symbol);
          if (std::ranges::find(first_terminals, terminal) == first_terminals.end()) {
            first_terminals.push_back(terminal);
          }
        }
      }
    }
    return first_terminals;
  }

private:
  void add_rule(NonTerminal lhs, Pattern rhs) {
    rules_.push_back(Rule{
        .lhs = std::move(lhs),
        .rhs = std::move(rhs),
    });
  }

  static std::vector<SymbolList> compile_pattern(const Pattern& pattern) {
    if (std::holds_alternative<Pattern::Epsilon>(pattern.node())) {
      return {SymbolList{}};
    }

    if (const auto* one = std::get_if<Pattern::One>(&pattern.node())) {
      return {SymbolList{one->symbol}};
    }

    if (const auto* sequence = std::get_if<Pattern::Sequence>(&pattern.node())) {
      return compile_sequence(*sequence);
    }

    const auto& alternative = std::get<Pattern::Alternative>(pattern.node());
    return compile_alternative(alternative);
  }

  static std::vector<SymbolList> compile_sequence(const Pattern::Sequence& sequence) {
    std::vector<SymbolList> result = {SymbolList{}};

    for (const auto& part : sequence.parts) {
      std::vector<SymbolList> part_compiled = compile_pattern(part);
      std::vector<SymbolList> next;

      for (const auto& prefix : result) {
        for (const auto& suffix : part_compiled) {
          SymbolList combined = prefix;
          combined.insert(combined.end(), suffix.begin(), suffix.end());
          next.push_back(std::move(combined));
        }
      }

      result = std::move(next);
    }

    return result;
  }

  static std::vector<SymbolList> compile_alternative(const Pattern::Alternative& alternative) {
    std::vector<SymbolList> result;

    for (const auto& choice : alternative.choices) {
      std::vector<SymbolList> choice_compiled = compile_pattern(choice);

      result.insert(result.end(), choice_compiled.begin(), choice_compiled.end());
    }

    return result;
  }

private:
  std::vector<Rule> rules_;
  std::vector<CompiledRule> compiled_rules_;
};

export class ItemSet {
public:
  struct Item {
    std::size_t dot_position_;
    CompiledRule rule_;
    Terminal expected_;

    bool operator==(const Item& other) const {
      return dot_position_ == other.dot_position_ && rule_.lhs.name == other.rule_.lhs.name &&
             rule_.rhs == other.rule_.rhs && expected_ == other.expected_;
    }
  };

  ItemSet() = default;

  explicit ItemSet(std::ranges::input_range auto&& items) {
    std::ranges::copy(std::forward<decltype(items)>(items), std::back_inserter(items_));
  }

  static auto initial() -> ItemSet {
    return ItemSet{std::views::single(Item{.dot_position_ = 0,
                                           .rule_ = CompiledRule{
                                               .lhs = NonTerminal{"Start"},
                                               .rhs = {NonTerminal{"FuncDeclList"}},
                                           }})};
  }

  auto begin() {
    return items_.begin();
  }

  auto begin() const {
    return items_.begin();
  }

  auto end() {
    return items_.end();
  }

  auto end() const {
    return items_.end();
  }

  auto size() const -> std::size_t {
    return items_.size();
  }

  auto closure(this ItemSet& self, const Grammar& grammar) -> void {
    self.items_.reserve(self.items_.size() * 2);
    bool changed = false;
    do {
      changed = false;
      for (std::size_t i = 0; i < self.items_.size(); ++i) {
        const Item& item = self.items_[i];
        if (item.dot_position_ >= item.rule_.rhs.size()) {
          continue;
        }
        const Symbol symbol = item.rule_.rhs[item.dot_position_];
        if (const auto* nonterminal = std::get_if<NonTerminal>(&symbol)) {
          for (const auto& rule : grammar.findCompiledRules(*nonterminal)) {
            Item new_item{.dot_position_ = 0, .rule_ = rule};
            if (std::ranges::find_if(self, [&](const auto& item) {
                  return item == new_item;
                }) == self.end()) {
              self.items_.push_back(new_item);
              changed = true;
            }
          }
        }
      }
    } while (changed);
  }

  auto operator==(this const auto& self, const auto& other) -> bool {
    return std::ranges::equal(self, other);
  }

private:
  std::vector<Item> items_;
};

export template <class Key>
concept HashableKey = std::equality_comparable<Key> && std::copy_constructible<Key> && requires(const Key& key) {
  { key.hash() } -> std::convertible_to<std::size_t>;
};

export inline std::size_t hash_combine(std::size_t a, std::size_t b) {
  a ^= b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2);
  return a;
}

export template <HashableKey SecondKey, std::equality_comparable Value> class Sparse2D {
public:
  struct Key {
    std::size_t first;
    SecondKey second;

    bool operator==(const Key&) const = default;
  };

  struct KeyHash {
    std::size_t operator()(const Key& key) const {
      std::size_t h1 = std::hash<std::size_t>{}(key.first);
      std::size_t h2 = key.second.hash();
      return hash_combine(h1, h2);
    }
  };

  explicit Sparse2D(Value default_value) : default_value_(default_value) {}

  class CellRef {
  public:
    CellRef(Sparse2D& owner, Key key) : owner_(&owner), key_(std::move(key)) {}

    operator Value() const {
      return owner_->get(key_);
    }

    CellRef& operator=(Value value) {
      owner_->set(key_, value);
      return *this;
    }

  private:
    Sparse2D* owner_;
    Key key_;
  };

  CellRef operator[](std::size_t first, const SecondKey& second) {
    return CellRef(*this, Key{first, second});
  }

  Value operator[](std::size_t first, const SecondKey& second) const {
    return get(Key{first, second});
  }

  Value get(std::size_t first, const SecondKey& second) const {
    return get(Key{first, second});
  }

  void set(std::size_t first, const SecondKey& second, Value value) {
    set(Key{first, second}, value);
  }

  bool contains(std::size_t first, const SecondKey& second) const {
    return data_.find(Key{first, second}) != data_.end();
  }

  void erase(std::size_t first, const SecondKey& second) {
    data_.erase(Key{first, second});
  }

  void reserve(std::size_t n) {
    data_.reserve(n);
  }

  std::size_t size() const {
    return data_.size();
  }

private:
  Value get(const Key& key) const {
    auto it = data_.find(key);
    if (it == data_.end()) {
      return default_value_;
    }
    return it->second;
  }

  void set(const Key& key, Value value) {
    if (value == default_value_) {
      data_.erase(key);
    } else {
      data_.insert_or_assign(key, value);
    }
  }

private:
  ankerl::unordered_dense::map<Key, Value, KeyHash> data_;
  Value default_value_;
};

export enum class ActionType {
  Shift,
  Reduce,
  Accept,
  Error,
};

export class ItemSetCollection {
public:
  ItemSetCollection(const Grammar& grammar) {
    ItemSet initial_set = ItemSet::initial();
    initial_set.closure(grammar);
    item_sets_.push_back(std::move(initial_set));
    bool changed = false;
    do {
      changed = false;
      for (std::size_t i = 0; i < item_sets_.size(); ++i) {
        auto next_symbols = item_sets_[i] | std::views::filter([](const auto& item) {
                              return item.dot_position_ < item.rule_.rhs.size();
                            }) |
                            std::views::transform([](const auto& item) {
                              return item.rule_.rhs[item.dot_position_];
                            }) |
                            std::ranges::to<std::vector>();

        auto sub = std::ranges::unique(next_symbols);
        next_symbols.erase(sub.begin(), sub.end());

        for (const Symbol& symbol : next_symbols) {
          const std::size_t old_size = item_sets_.size();
          std::size_t next_index = go(i, symbol, grammar);
          if (next_index >= old_size) {
            changed = true;
          }
        }
      }
    } while (changed);
  }

  auto buildTables(this ItemSetCollection& self, const Grammar& grammar)
      -> std::pair<Sparse2D<Terminal, std::pair<ActionType, std::size_t>>, Sparse2D<NonTerminal, std::size_t>> {
    constexpr auto max = std::numeric_limits<std::size_t>::max();

    Sparse2D<Terminal, std::pair<ActionType, std::size_t>> action_table(std::make_pair(ActionType::Error, max));
    Sparse2D<NonTerminal, std::size_t> goto_table(max);

    for (const auto& [i, item_set] : std::views::enumerate(self.item_sets_)) {
      for (const auto& item : item_set) {
        if (item.dot_position_ == item.rule_.rhs.size()) [[unlikely]] {
          if (item.rule_.lhs.name == "Start") {
            action_table[i, Terminal{TokenType::EndOfFile}] = std::make_pair(ActionType::Accept, max);
          } else {
            for (const auto& [_, tok] : rfl::get_enumerator_array<TokenType>()) {
              Terminal terminal{tok};
              action_table[i, terminal] = std::make_pair(ActionType::Reduce, i);
            }
          }
          continue;
        }
        const Symbol& symbol = item.rule_.rhs[item.dot_position_];
        std::size_t next_index = self.go(i, symbol, grammar);
        if (std::holds_alternative<Terminal>(symbol)) {
          action_table[i, std::get<Terminal>(symbol)] = std::make_pair(ActionType::Shift, next_index);
        } else {
          goto_table[i, std::get<NonTerminal>(symbol)] = next_index;
        }
      }
    }
    return {std::move(action_table), std::move(goto_table)};
  }

private:
  auto go(this ItemSetCollection& self, std::size_t item_set_idx, const Symbol& symbol, const Grammar& grammar)
      -> std::size_t {
    ItemSet result =
        self.item_sets_[item_set_idx] | std::views::filter([&](const auto& item) {
          return item.dot_position_ < item.rule_.rhs.size() && item.rule_.rhs[item.dot_position_] == symbol;
        }) |
        std::views::transform([](const auto& item) {
          ItemSet::Item new_item = item;
          ++new_item.dot_position_;
          return new_item;
        }) |
        std::ranges::to<ItemSet>();

    result.closure(grammar);
    if (auto it = std::ranges::find(self.item_sets_, result); it != self.item_sets_.end()) {
      return static_cast<std::size_t>(it - self.item_sets_.begin());
    }

    self.item_sets_.push_back(std::move(result));
    return self.item_sets_.size() - 1;
  }

private:
  std::vector<ItemSet> item_sets_;
};
