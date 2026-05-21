#include <algorithm>
#include <cassert>
#include <functional>
#include <print>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"

namespace Manadrain {
static const std::unordered_map<std::string_view, ReservedWord> reserved_atlas{
    {"const", ReservedWord::W_CONST},
    {"let", ReservedWord::W_LET},
    {"var", ReservedWord::W_VAR},
    {"class", ReservedWord::W_CLASS},
    {"function", ReservedWord::W_FUNCTION},
    {"return", ReservedWord::W_RETURN},
    {"import", ReservedWord::W_IMPORT},
    {"export", ReservedWord::W_EXPORT},
    {"from", ReservedWord::W_FROM},
    {"as", ReservedWord::W_AS},
    {"default", ReservedWord::W_DEFAULT},
    {"undefined", ReservedWord::W_UNDEFINED},
    {"null", ReservedWord::W_NULL},
    {"true", ReservedWord::W_TRUE},
    {"false", ReservedWord::W_FALSE},
    {"if", ReservedWord::W_IF},
    {"else", ReservedWord::W_ELSE},
    {"while", ReservedWord::W_WHILE},
    {"for", ReservedWord::W_FOR},
    {"do", ReservedWord::W_DO},
    {"break", ReservedWord::W_BREAK},
    {"continue", ReservedWord::W_CONTINUE},
    {"switch", ReservedWord::W_SWITCH}};
static const std::array reserved_pool{std::to_array<std::string_view>(
    {"",          "const",  "let",    "var",   "class",    "function",
     "return",    "import", "export", "from",  "as",       "default",
     "undefined", "null",   "true",   "false", "if",       "else",
     "while",     "for",    "do",     "break", "continue", "switch"})};
static const std::unordered_map<std::string_view, AnchoredWord> anchored_atlas{
    {"console", AnchoredWord::W_CONSOLE}, {"log", AnchoredWord::W_LOG}};
static const std::array anchored_pool{
    std::to_array<std::string_view>({"", "console", "log"})};

std::optional<char32_t> Parser::forward() {
  if (position >= text_size) {
    backtrace.push_back(-1);
    return std::nullopt;
  } else {
    ucs4_t ch;
    int advance{
        u8_mbtoucr(&ch, text_buffer.get() + position, text_size - position)};
    assert(advance >= 0);
    position += advance;
    backtrace.push_back(std::bit_cast<std::int32_t>(ch));
    return ch;
  }
}

std::generator<std::optional<char32_t>> Parser::traverse_text() {
  while (1)
    co_yield forward();
}

std::generator<char> traverse_ucs4(ucs4_t cp) {
  std::array<std::uint8_t, 6> buffer{};
  int advance{u8_uctomb(buffer.data(), cp, buffer.size())};
  assert(advance >= 0);
  for (int i = 0; i < advance; ++i)
    co_yield buffer[i];
}

void Parser::backward() {
  std::optional<char32_t> behind{};
  if (backtrace.back() > -1)
    behind = std::bit_cast<char32_t>(backtrace.back());
  position -= std::ranges::distance(
      behind | std::views::transform(traverse_ucs4) | std::views::join);
  backtrace.pop_back();
}

void Parser::backward(std::size_t N) {
  for (int i = 0; i < N; ++i)
    backward();
}

std::optional<Token> Parser::revoked_pull() {
  if (token_revoked.empty())
    return std::nullopt;
  token_history.splice(token_history.begin(), token_revoked,
                       token_revoked.begin());
  return token_history.front();
}

void Parser::history_push(Token token) { token_history.push_front(token); }
void Parser::history_pull() {
  token_revoked.splice(token_revoked.begin(), token_history,
                       token_history.begin());
}

Token Parser::tokenize_identifier(char32_t leading) {
  std::string identifier_str{std::from_range, traverse_ucs4(leading)};
  auto does_exist = [](auto an_optional) { return an_optional.has_value(); };
  auto xid_continue_view =
      traverse_text() | std::views::take_while(does_exist) | std::views::join |
      std::views::take_while(uc_is_property_xid_continue) |
      std::views::transform(traverse_ucs4) | std::views::join;
  identifier_str.append_range(xid_continue_view);
  backward();
  auto iter_reserved = reserved_atlas.find(identifier_str);
  if (iter_reserved != reserved_atlas.end())
    return iter_reserved->second;
  auto iter_anchored = anchored_atlas.find(identifier_str);
  if (iter_anchored != anchored_atlas.end())
    return Identifier{iter_anchored->second};
  auto [iter_atlas, did_insert] =
      atom_atlas.insert({identifier_str, atom_pool.size()});
  if (did_insert)
    atom_pool.push_back(std::move(identifier_str));
  return Identifier{AtomizedWord{iter_atlas->second}};
}

Token Parser::tokenize_string_literal(char32_t separator) {
  std::string literal_str{};
  auto match_literal_end = [separator](auto code_point) {
    return code_point != separator;
  };
  auto literal_view =
      traverse_text() | std::views::take_while(match_literal_end);
  for (std::optional<char32_t> leading : literal_view) {
    if (leading == '\r' && forward() != '\n')
      backward();
    if (not leading || leading == '\r' || leading == '\n')
      throw ScriptError{UnexpectedStringEnd{}};
    literal_str.append_range(traverse_ucs4(*leading));
  }
  auto iter_reserved = reserved_atlas.find(literal_str);
  if (iter_reserved != reserved_atlas.end())
    return StringHandle{iter_reserved->second};
  auto iter_anchored = anchored_atlas.find(literal_str);
  if (iter_anchored != anchored_atlas.end())
    return StringHandle{iter_anchored->second};
  auto [iter_atlas, did_insert] =
      atom_atlas.insert({literal_str, atom_pool.size()});
  if (did_insert)
    atom_pool.push_back(std::move(literal_str));
  return StringHandle{AtomizedWord{iter_atlas->second}};
}

Token Parser::tokenize_numeric_literal(char32_t leading) {
  std::string numeric_str{std::from_range, traverse_ucs4(leading)};
  auto match_nullopt = [](auto an_optional) { return an_optional.has_value(); };
  auto match_digit = [](char32_t code_point) {
    return std::isdigit(code_point);
  };
  numeric_str.append_range(
      traverse_text() | std::views::take_while(match_nullopt) |
      std::views::join | std::views::take_while(match_digit) |
      std::views::transform(traverse_ucs4) | std::views::join);
  backward();
  std::int64_t num_literal{};
  std::from_chars(numeric_str.data(), numeric_str.data() + numeric_str.size(),
                  num_literal);
  return num_literal;
}

static const std::array legal_punct = std::to_array<char32_t>(
    {'(', ')', '*', '+', '-', '.', '/', ':', ';', '=', '{', '}'});

Token Parser::tokenize() {
  std::optional revoked_opt{revoked_pull()};
  for (Token revoked_token : revoked_opt)
    return revoked_token;
  auto does_exist = [](auto an_optional) { return an_optional.has_value(); };
  for (char32_t leading : traverse_text() | std::views::take_while(does_exist) |
                              std::views::join) {
    Token token_ret{};
    if (uc_is_property_white_space(leading))
      continue;
    else if (std::ranges::any_of(
                 std::to_array({'`', '\'', '"'}),
                 [leading](char quote) { return leading == quote; }))
      token_ret = tokenize_string_literal(leading);
    else if (uc_is_property_xid_start(leading) || leading == '_')
      token_ret = tokenize_identifier(leading);
    else if (std::ranges::binary_search(legal_punct, leading))
      token_ret = leading;
    else if (std::isdigit(leading))
      token_ret = tokenize_numeric_literal(leading);
    else
      throw ScriptError{UnexpectedToken{}};
    history_push(token_ret);
    return token_ret;
  }
  history_push(std::monostate{});
  return std::monostate{};
}

std::generator<Token> Parser::traverse_tokens() {
  while (1)
    co_yield tokenize();
}

void assert_punct(Token token, char32_t must_be) {
  char32_t *alter_ptr = std::get_if<char32_t>(&token);
  if (alter_ptr && *alter_ptr == must_be)
    return;
  throw ScriptError{MissingPunctuation{must_be}};
}

void Parser::parse_text() {
  FunctionBlueprint blueprint{};
  auto match_script_end = [](Token token) {
    return !std::holds_alternative<std::monostate>(token);
  };
  for (Token token :
       traverse_tokens() | std::views::take_while(match_script_end))
    parse_statement(blueprint, token);
  std::size_t blueprint_handle{blueprint_pool.size()};
  blueprint_pool.push_back(std::move(blueprint));
  main_function = bootstrap(blueprint_handle, std::nullopt);
}

void Parser::parse_statement(FunctionBlueprint &blueprint, Token leading) {
  auto match_reserved = [](auto t) {
    if constexpr (std::is_same_v<decltype(t), ReservedWord>)
      return t;
    return ReservedWord::MONOSTATE;
  };
  ReservedWord word{leading.visit(match_reserved)};
  Statement statement;
  if (word == ReservedWord::W_FUNCTION) {
    std::size_t blueprint_handle{parse_function_decl()};
    AbstractWord function_name{blueprint_pool[blueprint_handle].function_name};
    if (std::ranges::binary_search(blueprint.scope_shape, function_name))
      throw ScriptError{InvalidDeclaration{}};
    blueprint.nested_blueprint.push_back({function_name, blueprint_handle});
    auto lower_bound =
        std::ranges::lower_bound(blueprint.scope_shape, function_name);
    blueprint.scope_shape.insert(lower_bound, function_name);
  } else if (word == ReservedWord::W_LET) {
    VariableDeclaration declaration{parse_variable_decl()};
    AbstractWord variable_name{declaration.variable_name};
    if (std::ranges::binary_search(blueprint.scope_shape, variable_name))
      throw ScriptError{InvalidDeclaration{}};
    auto lower_bound =
        std::ranges::lower_bound(blueprint.scope_shape, variable_name);
    blueprint.scope_shape.insert(lower_bound, variable_name);
    blueprint.body.push_back(declaration);
  } else if (word == ReservedWord::W_RETURN) {
    blueprint.body.push_back(parse_expression());
    assert_punct(tokenize(), ';');
  } else {
    history_pull();
    blueprint.body.push_back(parse_expression());
    assert_punct(tokenize(), ';');
  }
}

std::size_t Parser::parse_function_decl() {
  auto extract_name = [](auto token) -> AbstractWord {
    if constexpr (std::is_same_v<decltype(token), Identifier>)
      return token.handle;
    throw ScriptError{MissingFunctionName{}};
  };
  AbstractWord function_name{std::visit(extract_name, tokenize())};
  for (char function_punct : std::to_array({'(', ')', '{'}))
    assert_punct(tokenize(), function_punct);
  auto match_function_end = [&](Token token) {
    char32_t *alter_ptr = std::get_if<char32_t>(&token);
    return !alter_ptr || *alter_ptr != '}';
  };
  FunctionBlueprint blueprint{function_name};
  for (Token token :
       traverse_tokens() | std::views::take_while(match_function_end)) {
    if (std::holds_alternative<std::monostate>(token))
      throw ScriptError{MissingPunctuation{'}'}};
    parse_statement(blueprint, token);
  }
  std::size_t blueprint_handle{blueprint_pool.size()};
  blueprint_pool.push_back(std::move(blueprint));
  return blueprint_handle;
}

VariableDeclaration Parser::parse_variable_decl() {
  auto extract_name = [](auto token) -> AbstractWord {
    if constexpr (std::is_same_v<decltype(token), Identifier>)
      return token.handle;
    throw ScriptError{MissingVariableName{}};
  };
  VariableDeclaration variable_decl{std::visit(extract_name, tokenize())};
  assert_punct(tokenize(), '=');
  variable_decl.initializer = parse_expression();
  assert_punct(tokenize(), ';');
  return variable_decl;
}

Expression Parser::parse_expression() { return parse_additive_expr(); }

Expression Parser::parse_primary_expr() {
  return tokenize().visit([](auto t) -> Expression {
    if constexpr (std::is_same_v<decltype(t), StringHandle> ||
                  std::is_same_v<decltype(t), Identifier> ||
                  std::is_same_v<decltype(t), std::int64_t> ||
                  std::is_same_v<decltype(t), double>)
      return t;
    throw ScriptError{UnexpectedToken{}};
  });
}

Expression Parser::parse_postfix_expr() {
  using POSTFIX_REDUCER =
      std::optional<std::copyable_function<Expression(Expression) const>>;
  auto match_reducer = [this](auto t) -> POSTFIX_REDUCER {
    if constexpr (std::is_same_v<decltype(t), char32_t>) {
      if (t == '.')
        return [this](Expression expr) { return parse_member_expr(expr); };
      if (t == '(')
        return [this](Expression expr) { return parse_call_expr(expr); };
    }
    return std::nullopt;
  };
  auto postfix_fold = [](Expression postfix_expr, auto postfix_reducer) {
    return postfix_reducer(postfix_expr);
  };
  auto does_exist = [](auto an_optional) { return an_optional.has_value(); };
  auto postfix_reducers =
      traverse_tokens() |
      std::views::transform([&](Token t) { return t.visit(match_reducer); }) |
      std::views::take_while(does_exist) | std::views::join;
  Expression postfix_expr{std::ranges::fold_left(
      postfix_reducers, parse_primary_expr(), postfix_fold)};
  history_pull();
  return postfix_expr;
}

Expression Parser::parse_additive_expr() {
  Expression expr_left{parse_postfix_expr()};
  auto match_binary = [](auto t) -> std::optional<char32_t> {
    if constexpr (std::is_same_v<decltype(t), char32_t>)
      if (t == '+' || t == '-')
        return t;
    return std::nullopt;
  };
  for (char32_t binary_op : tokenize().visit(match_binary)) {
    expr_pool.push_back(
        BinaryExpression{expr_left, parse_postfix_expr(), binary_op});
    return ExpressionHandle{expr_pool.size() - 1};
  }
  history_pull();
  return expr_left;
}

Expression Parser::parse_member_expr(Expression obj_expr) {
  auto property = tokenize().visit([](auto t) -> Identifier {
    if constexpr (std::is_same_v<decltype(t), Identifier>)
      return t;
    throw ScriptError{MissingFieldName{}};
  });
  expr_pool.push_back(MemberExpression{obj_expr, property.handle});
  return ExpressionHandle{expr_pool.size() - 1};
}

Expression Parser::parse_call_expr(Expression callee_expr) {
  auto match_rparen = [](auto t) -> bool {
    if constexpr (std::is_same_v<decltype(t), char32_t>)
      if (t == ')')
        return 1;
    return 0;
  };
  std::vector<Expression> arguments{};
  while (1) {
    if (tokenize().visit(match_rparen))
      break;
    history_pull();
    arguments.push_back(parse_expression());
    if (tokenize().visit(match_rparen))
      break;
    history_pull();
    assert_punct(tokenize(), ',');
  }
  auto bail_function_call = [&]() {
    ExpressionHandle expr_handle{expr_pool.size()};
    expr_pool.push_back(
        FunctionCallExpression{callee_expr, std::move(arguments)});
    return expr_handle;
  };
  ExpressionHandle *callee_handle{std::get_if<ExpressionHandle>(&callee_expr)};
  if (not callee_handle)
    return bail_function_call();
  MemberExpression *expr_member{
      std::get_if<MemberExpression>(&expr_pool[callee_handle->pool_idx])};
  if (not expr_member)
    return bail_function_call();
  expr_pool[callee_handle->pool_idx] = MethodCallExpression{
      expr_member->object, expr_member->property, std::move(arguments)};
  return *callee_handle;
}

Dynamic Script::exec_reduce(std::size_t function_handle,
                            MethodCallExpression &expr_call) {
  auto reduce_argument = [&](Expression arg_expr) {
    return exec_reduce(function_handle, arg_expr);
  };
  auto reduced_args =
      expr_call.arguments | std::views::transform(reduce_argument);
  std::vector<Dynamic> arguments{std::from_range, reduced_args};
  auto reduce_intrinsic_call = [&](auto intrinsic_tag) {
    return instrinsic_call(intrinsic_tag, expr_call.property,
                           std::move(arguments));
  };
  auto reduce_method_call = [&](auto dynamic_alt) -> Dynamic {
    if constexpr (std::is_same_v<decltype(dynamic_alt), IntrinsicHandle>)
      return dynamic_alt.visit(reduce_intrinsic_call);
    return {};
  };
  return exec_reduce(function_handle, expr_call.object)
      .visit(reduce_method_call);
}

Dynamic Script::exec_reduce(std::size_t function_handle,
                            FunctionCallExpression &expr_call) {
  Dynamic dynamic_callee{exec_reduce(function_handle, expr_call.callee)};
  auto match_function_handle = [&](auto dynamic_alt) -> std::size_t {
    if constexpr (std::is_same_v<decltype(dynamic_alt), FunctionHandle>)
      return dynamic_alt.pool_idx;
    throw ScriptError{InvalidFunctionCall{}};
  };
  std::size_t callee_handle{std::visit(match_function_handle, dynamic_callee)};
  std::size_t blueprint_handle{function_pool[callee_handle].blueprint_handle};
  for (Statement statement : blueprint_pool[blueprint_handle].body) {
    exec_reduce(callee_handle, statement);
    if (std::holds_alternative<ReturnStatement>(statement))
      break;
  }
  return function_pool[function_handle].return_val;
}

Dynamic Script::exec_reduce(std::size_t function_handle,
                            BinaryExpression &expression) {
  return {};
}

Dynamic Script::exec_reduce(std::size_t function_handle,
                            MemberExpression &expression) {
  return {};
}

Dynamic Script::exec_reduce(std::size_t function_handle,
                            ExpressionHandle expr_handle) {
  return expr_pool[expr_handle.pool_idx].visit([&](auto &exprnode_alt) {
    return exec_reduce(function_handle, exprnode_alt);
  });
}

Dynamic Script::exec_reduce(std::size_t function_handle,
                            StringHandle string_handle) {
  return {};
}

Dynamic Script::exec_reduce(std::size_t function_handle, std::int64_t number) {
  return {};
}

Dynamic Script::exec_reduce(std::size_t function_handle, double number) {
  return {};
}

Dynamic Script::exec_reduce(std::size_t function_handle,
                            Expression expression) {
  return expression.visit([&](auto &expression_alt) {
    return exec_reduce(function_handle, expression_alt);
  });
}

Dynamic Script::global_get(AbstractWord word) {
  if (word == AbstractWord{AnchoredWord::W_CONSOLE})
    return ConsoleHandle{};
  return {};
}

std::generator<std::size_t>
Script::traverse_function_closure(std::size_t function_handle) {
  while (1) {
    co_yield function_handle;
    VanillaFunction &function{function_pool[function_handle]};
    std::optional parent_handle{function.parent_handle};
    if (not parent_handle)
      break;
    function_handle = *parent_handle;
  }
}

Dynamic Script::exec_reduce(std::size_t function_handle,
                            Identifier identifier) {
  for (std::size_t current_handle :
       traverse_function_closure(function_handle)) {
    std::size_t blueprint_handle{
        function_pool[current_handle].blueprint_handle};
    ObjectShape &scope_shape{blueprint_pool[blueprint_handle].scope_shape};
    auto lower_bound = std::ranges::lower_bound(scope_shape, identifier.handle);
    if (lower_bound == scope_shape.end() || *lower_bound != identifier.handle)
      continue;
    std::size_t scope_idx = std::distance(scope_shape.begin(), lower_bound);
    FunctionScope &own_scope{function_pool[function_handle].own_scope};
    for (Dynamic dynamic_local : own_scope[scope_idx])
      return dynamic_local;
    throw ScriptError{InvalidVariableAccess{}};
  }
  return global_get(identifier.handle);
}

void Script::exec_reduce(std::size_t function_handle,
                         VariableDeclaration declaration) {
  exec_reduce(function_handle, declaration.initializer);
}

void Script::exec_reduce(std::size_t function_handle,
                         ReturnStatement statement) {
  exec_reduce(function_handle, statement.argument);
}

void Script::exec_reduce(std::size_t function_handle, Statement statement) {
  std::visit(
      [&](auto alternative) { exec_reduce(function_handle, alternative); },
      statement);
}

FunctionHandle Script::bootstrap(std::size_t blueprint_handle,
                                 std::optional<std::size_t> parent_handle) {
  ObjectShape &scope_shape = blueprint_pool[blueprint_handle].scope_shape;
  std::size_t function_handle{function_pool.size()};
  function_pool.push_back(VanillaFunction{blueprint_handle, parent_handle});
  function_pool[function_handle].own_scope.resize(scope_shape.size());
  for (auto [nested_name, nested_blueprint] :
       blueprint_pool[blueprint_handle].nested_blueprint) {
    auto lower_bound = std::ranges::lower_bound(scope_shape, nested_name);
    assert(lower_bound != scope_shape.end() && *lower_bound == nested_name);
    std::size_t scope_idx = std::distance(scope_shape.begin(), lower_bound);
    FunctionHandle nested_function{
        bootstrap(nested_blueprint, function_handle)};
    function_pool[function_handle].own_scope[scope_idx] = nested_function;
  }
  return FunctionHandle{function_handle};
}

void Script::execute() {
  std::size_t blueprint_handle{
      function_pool[main_function.pool_idx].blueprint_handle};
  for (Statement statement : blueprint_pool[blueprint_handle].body)
    exec_reduce(main_function.pool_idx, statement);
}

Dynamic Script::instrinsic_call(ConsoleHandle, AbstractWord property,
                                std::vector<Dynamic> arguments) {
  if (property == AbstractWord{AnchoredWord::W_LOG})
    std::println("default message!");
  return {};
}
} // namespace Manadrain
