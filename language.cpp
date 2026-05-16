#include <algorithm>
#include <cassert>
#include <unordered_map>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"

namespace Manadrain {
static const std::unordered_map<std::string_view, RESERVED> keyword_pool{
    {"const", RESERVED::W_CONST},       {"let", RESERVED::W_LET},
    {"var", RESERVED::W_VAR},           {"class", RESERVED::W_CLASS},
    {"function", RESERVED::W_FUNCTION}, {"return", RESERVED::W_RETURN},
    {"import", RESERVED::W_IMPORT},     {"export", RESERVED::W_EXPORT},
    {"from", RESERVED::W_FROM},         {"as", RESERVED::W_AS},
    {"default", RESERVED::W_DEFAULT},   {"undefined", RESERVED::W_UNDEFINED},
    {"null", RESERVED::W_NULL},         {"true", RESERVED::W_TRUE},
    {"false", RESERVED::W_FALSE},       {"if", RESERVED::W_IF},
    {"else", RESERVED::W_ELSE},         {"while", RESERVED::W_WHILE},
    {"for", RESERVED::W_FOR},           {"do", RESERVED::W_DO},
    {"break", RESERVED::W_BREAK},       {"continue", RESERVED::W_CONTINUE},
    {"switch", RESERVED::W_SWITCH}};

std::optional<char32_t> Script::forward() {
  if (position >= text_source.size())
    backtrace.emplace();
  else {
    ucs4_t ch;
    int advance{u8_mbtoucr(&ch, text_source.data() + position,
                           text_source.size() - position)};
    assert(advance >= 0);
    position += advance;
    backtrace.push(ch);
  }
  return backtrace.top();
}

std::generator<std::optional<char32_t>> Script::traverse_text() {
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

void Script::backward() {
  std::optional behind{backtrace.top()};
  position -= std::ranges::distance(
      behind | std::views::transform(traverse_ucs4) | std::views::join);
  backtrace.pop();
}

void Script::backward(std::size_t N) {
  for (int i = 0; i < N; ++i)
    backward();
}

bool has_code_point(std::optional<char32_t> opt) { return opt.has_value(); }
bool has_token(std::optional<TOKEN> opt) { return opt.has_value(); }

TOKEN Script::tokenize_word(char32_t leading) {
  std::string identifier_str{std::from_range, traverse_ucs4(leading)};
  auto xid_continue_view =
      traverse_text() | std::views::take_while(has_code_point) |
      std::views::join | std::views::take_while(uc_is_property_xid_continue) |
      std::views::transform(traverse_ucs4) | std::views::join;
  identifier_str.append_range(xid_continue_view);
  backward();
  do {
    auto iter_keyword = keyword_pool.find(identifier_str);
    if (iter_keyword == keyword_pool.end())
      break;
    return iter_keyword->second;
  } while (0);
  auto iter_atlas = atom_atlas.find(identifier_str);
  if (iter_atlas == atom_atlas.end()) {
    auto iter_atom =
        atom_pool.insert(atom_pool.end(), std::move(identifier_str));
    auto insertion_ret = atom_atlas.insert(*iter_atom);
    iter_atlas = insertion_ret.first;
  }
  return IDENTIFIER{*iter_atlas};
}

std::int32_t code_point_to_int(std::optional<char32_t> code_point) {
  if (not code_point)
    return -1;
  return *code_point;
}

TOKEN Script::tokenize_string_literal(char32_t separator) {
  std::string literal_str{};
  auto not_ended = [separator](auto code_point) {
    return code_point != separator;
  };
  auto literal_view = traverse_text() |
                      std::views::transform(code_point_to_int) |
                      std::views::take_while(not_ended);
  for (std::int32_t leading : literal_view) {
    switch (leading) {
    case '\r':
      if (forward() != '\n')
        backward();
      [[fallthrough]];
    case '\n':
      if (separator != '`')
        throw ScriptError{UNEXPECTED_STRING_END{}};
      literal_str.push_back('\n');
      break;
    default:
      literal_str.append_range(traverse_ucs4(leading));
      break;
    }
  }
  auto iter_atlas = atom_atlas.find(literal_str);
  if (iter_atlas == atom_atlas.end()) {
    auto iter_atom = atom_pool.insert(atom_pool.end(), std::move(literal_str));
    auto insertion_ret = atom_atlas.insert(*iter_atom);
    iter_atlas = insertion_ret.first;
  }
  return STRING_LITERAL{*iter_atlas, separator};
}

TOKEN Script::tokenize_numeric_literal(char32_t leading) {
  std::string numeric_str{std::from_range, traverse_ucs4(leading)};
  auto has_digit = [](char32_t code_point) { return std::isdigit(code_point); };
  auto digits_view = traverse_text() | std::views::take_while(has_code_point) |
                     std::views::join | std::views::take_while(has_digit) |
                     std::views::transform(traverse_ucs4) | std::views::join;
  numeric_str.append_range(digits_view);
  backward();
  std::int64_t num_literal{};
  std::from_chars(numeric_str.data(), numeric_str.data() + numeric_str.size(),
                  num_literal);
  return NUMERIC_LITERAL{num_literal};
}

TOKEN Script::tokenize() {
  for (char32_t leading : traverse_text() |
                              std::views::take_while(has_code_point) |
                              std::views::join) {
    TOKEN token_ret{};
    if (uc_is_property_white_space(leading))
      continue;
    else if (std::ranges::any_of(
                 std::to_array({'`', '\'', '"'}),
                 [leading](char quote) { return leading == quote; }))
      token_ret = tokenize_string_literal(leading);
    else if (uc_is_property_xid_start(leading) || leading == '_')
      token_ret = tokenize_word(leading);
    else if (std::ranges::any_of(
                 std::to_array({'(', ')', ':', '{', '}', '=', ';'}),
                 [leading](char punct) { return leading == punct; }))
      token_ret = leading;
    else if (std::isdigit(leading))
      token_ret = tokenize_numeric_literal(leading);
    else
      throw ScriptError{UNEXPECTED_TOKEN{}};
    std::stack<std::optional<char32_t>> local_backtrace{};
    local_backtrace.swap(backtrace);
    return token_ret;
  }
  throw ScriptError{UNEXPECTED_END_OF_FILE{}};
}

std::generator<TOKEN> Script::traverse_tokens() {
  while (1)
    co_yield tokenize();
}

void Script::parse_text() {
  for (TOKEN token : traverse_tokens())
    script_body.push_back(
        token.visit([this](auto t) { return parse_statement(t); }));
}

Statement Script::parse_statement(RESERVED reserved) {
  switch (reserved) {
  case RESERVED::W_FUNCTION:
    return tokenize().visit([this](auto token) {
      FunctionDecl funcdecl{*this};
      funcdecl.parse(0, token);
      return funcdecl;
    });
  default:
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }
}

void FunctionDecl::parse(int stage, IDENTIFIER identifier) {
  switch (stage) {
  case FUNCTION_I:
    function_name = identifier.pool_view;
    script.tokenize().visit(
        [stage, this](auto token) { parse(stage + 1, token); });
    return;
  default:
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }
}

void FunctionDecl::parse(int stage, char32_t punct) {
  if (stage == FUNCTION_II && punct == '(') {
    script.tokenize().visit(
        [stage, this](auto token) { parse(stage + 1, token); });
    return;
  }
  if (stage == FUNCTION_III && punct == ')') {
    script.tokenize().visit(
        [stage, this](auto token) { parse(stage + 1, token); });
    return;
  }
  if (stage == FUNCTION_IV && punct == '{') {
    for (TOKEN token : script.traverse_tokens())
      function_body.push_back(
          token.visit([this](auto t) { return parse_statement(t); }));
    script.tokenize().visit(
        [stage, this](auto token) { parse(stage + 1, token); });
    return;
  }
  throw ScriptError{UNEXPECTED_TOKEN{}};
}

Statement FunctionDecl::parse_statement(RESERVED reserved) {
  switch (reserved) {
  case RESERVED::W_LET:
    return script.tokenize().visit([this](auto token) {
      VariableDecl vardecl{script};
      vardecl.parse(0, token);
      return vardecl;
    });
  default:
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }
}

void VariableDecl::parse(int stage, IDENTIFIER identifier) {
  switch (stage) {
  case VARIABLE_I:
    variable_name = identifier.pool_view;
    script.tokenize().visit(
        [stage, this](auto token) { parse(stage + 1, token); });
    return;
  default:
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }
}

void VariableDecl::parse(int stage, char32_t punct) {
  if (stage == VARIABLE_II && punct == '=') {
    return;
  }
  if (stage == VARIABLE_III && punct == ';')
    return;
  throw ScriptError{UNEXPECTED_TOKEN{}};
}
} // namespace Manadrain
