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
  return std::monostate{};
}

std::generator<TOKEN> Script::traverse_tokens() {
  while (1)
    co_yield tokenize();
}

RESERVED extract_reserved(TOKEN token) {
  RESERVED *alter_ptr = std::get_if<RESERVED>(&token);
  return alter_ptr ? *alter_ptr : RESERVED{};
}

std::optional<IDENTIFIER> extract_identifier(TOKEN token) {
  IDENTIFIER *alter_ptr = std::get_if<IDENTIFIER>(&token);
  return alter_ptr ? std::make_optional(*alter_ptr) : std::nullopt;
}

void assert_punct(TOKEN token, char32_t must_be) {
  char32_t *alter_ptr = std::get_if<char32_t>(&token);
  if (alter_ptr && *alter_ptr == must_be)
    return;
  throw ScriptError{MISSING_PUNCT{must_be}};
}

std::copyable_function<bool(TOKEN) const> match_punct(char32_t should_be) {
  return [should_be](TOKEN token) {
    char32_t *alter_ptr = std::get_if<char32_t>(&token);
    return alter_ptr && *alter_ptr == should_be;
  };
}

void Script::parse_text() {
  script_body.append_range(
      traverse_tokens() | std::views::take_while([](TOKEN token) {
        return !std::holds_alternative<std::monostate>(token);
      }) |
      std::views::transform(parse_statement()));
}

std::copyable_function<STATEMENT(TOKEN) const> Script::parse_statement() {
  return [this](TOKEN leading) {
    switch (extract_reserved(leading)) {
    case RESERVED::W_FUNCTION:
      return parse_function_decl();
    case RESERVED::W_LET:
      return parse_variable_decl();
    default:
      throw ScriptError{UNEXPECTED_TOKEN{}};
    }
  };
}

STATEMENT Script::parse_function_decl() {
  FUNCTION_DECL function_decl{};
  std::optional funcname_opt{extract_identifier(tokenize())};
  if (not funcname_opt)
    throw ScriptError{MISSING_FUNCTION_NAME{}};
  function_decl.function_name = funcname_opt->pool_view;
  assert_punct(tokenize(), '(');
  assert_punct(tokenize(), ')');
  assert_punct(tokenize(), '{');
  function_decl.function_body.append_range(
      traverse_tokens() |
      std::views::take_while(std::not_fn(match_punct('}'))) |
      std::views::transform(parse_statement()));
  throw ScriptError{UNSUPPORTED{}};
}

STATEMENT Script::parse_variable_decl() {
  VARIABLE_DECL variable_decl{};
  std::optional varname_opt{extract_identifier(tokenize())};
  if (not varname_opt)
    throw ScriptError{MISSING_VARIABLE_NAME{}};
  variable_decl.variable_name = varname_opt->pool_view;
  throw ScriptError{UNSUPPORTED{}};
}
} // namespace Manadrain
