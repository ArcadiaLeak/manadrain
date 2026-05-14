#include <algorithm>
#include <cassert>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"

namespace Manadrain {
static const std::unordered_map<std::string_view, RESERVED> reserved_umap{
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
    {"switch", RESERVED::W_SWITCH},     {"int", RESERVED::W_INT},
    {"long", RESERVED::W_LONG},         {"uint", RESERVED::W_UINT},
    {"ulong", RESERVED::W_ULONG},       {"float", RESERVED::W_FLOAT},
    {"double", RESERVED::W_DOUBLE},     {"string", RESERVED::W_STRING}};

std::optional<char32_t> Compilation::forward() {
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

std::generator<std::optional<char32_t>> Compilation::traverse_text() {
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

void Compilation::backward() {
  std::optional behind{backtrace.top()};
  position -= std::ranges::distance(
      behind | std::views::transform(traverse_ucs4) | std::views::join);
  backtrace.pop();
}

void Compilation::backward(std::size_t N) {
  for (int i = 0; i < N; ++i)
    backward();
}

bool has_code_point(std::optional<char32_t> opt) { return opt.has_value(); }
bool has_token(std::optional<TOKEN> opt) { return opt.has_value(); }

TOKEN Compilation::tokenize_word(char32_t leading) {
  std::string identifier_str{std::from_range, traverse_ucs4(leading)};
  auto xid_continue_view =
      traverse_text() | std::views::take_while(has_code_point) |
      std::views::join | std::views::take_while(uc_is_property_xid_continue) |
      std::views::transform(traverse_ucs4) | std::views::join;
  identifier_str.append_range(xid_continue_view);
  backward();
  auto reserved_it = reserved_umap.find(identifier_str);
  if (reserved_it != reserved_umap.end())
    return reserved_it->second;
  auto insertion_ret = string_pool.insert(std::move(identifier_str));
  return IDENTIFIER{*insertion_ret.first};
}

std::int32_t code_point_to_int(std::optional<char32_t> code_point) {
  if (not code_point)
    return -1;
  return *code_point;
}

STRING_LITERAL Compilation::tokenize_string_literal(char32_t separator) {
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
  auto insertion_ret = string_pool.insert(std::move(literal_str));
  return STRING_LITERAL{*insertion_ret.first, separator};
}

TOKEN Compilation::tokenize_numeric_literal(char32_t leading) {
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

TOKEN Compilation::tokenize() {
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

std::generator<TOKEN> Compilation::traverse_tokens() {
  while (1)
    co_yield tokenize();
}

void Compilation::compile_text() { tokenize().visit(ParseDeclaration{this}); }

void ParseDeclaration::operator()(RESERVED reserved) {
  switch (reserved) {
  case RESERVED::W_FUNCTION: {
    ParseFunctionDecl func_visitor{comp};
    comp->tokenize().visit(func_visitor);
    return;
  }
  default:
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }
}

void ParseFunctionDecl::operator()(IDENTIFIER identifier) {
  switch (stage) {
  case FUNCTION_I:
    funcname = identifier.pool_view;
    ++stage;
    comp->tokenize().visit(*this);
    return;
  default:
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }
}

void ParseFunctionDecl::operator()(char32_t punct) {
  if (stage == FUNCTION_II && punct == '(') {
    ++stage;
    comp->tokenize().visit(*this);
    return;
  }
  if (stage == FUNCTION_III && punct == ')') {
    ++stage;
    comp->tokenize().visit(*this);
    return;
  }
  if (stage == FUNCTION_IV && punct == ':') {
    ++stage;
    comp->tokenize().visit(*this);
    return;
  }
  if (stage == FUNCTION_VI && punct == '{') {
    for (TOKEN token : comp->traverse_tokens()) {
      ParseStatement visitor{this, comp};
      token.visit(visitor);
    }
    ++stage;
    comp->tokenize().visit(*this);
    return;
  }
  throw ScriptError{UNEXPECTED_TOKEN{}};
}

DATATYPE parse_type_annotation(RESERVED reserved) {
  switch (reserved) {
  case RESERVED::W_INT:
    return DATATYPE::T_I32;
  case RESERVED::W_STRING:
    return DATATYPE::T_STRING;
  default:
    throw ScriptError{INVALID_TYPE_ANNOTATION{}};
  }
}

void ParseFunctionDecl::operator()(RESERVED reserved) {
  switch (stage) {
  case FUNCTION_V:
    return_type = parse_type_annotation(reserved);
    ++stage;
    comp->tokenize().visit(*this);
    return;
  default:
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }
}

void ParseStatement::operator()(RESERVED reserved) {
  switch (reserved) {
  case RESERVED::W_LET:
    comp->tokenize().visit(ParseVariableDecl{this, func, comp});
    return;
  default:
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }
}

void ParseVariableDecl::operator()(IDENTIFIER identifier) {
  switch (stage) {
  case VARIABLE_I:
    variable_name = identifier.pool_view;
    ++stage;
    comp->tokenize().visit(*this);
    return;
  default:
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }
}

void ParseVariableDecl::operator()(char32_t punct) {
  if (stage == VARIABLE_II && punct == ':') {
    ++stage;
    comp->tokenize().visit(*this);
    return;
  }
  if (stage == VARIABLE_IV && punct == '=') {
    ++stage;
    comp->tokenize().visit(ParseExpression{stmt, func, comp});
    comp->tokenize().visit(*this);
    return;
  }
  if (stage == VARIABLE_V && punct == ';')
    return;
  throw ScriptError{UNEXPECTED_TOKEN{}};
}

void ParseVariableDecl::operator()(RESERVED reserved) {
  switch (stage) {
  case VARIABLE_III:
    variable_type = parse_type_annotation(reserved);
    ++stage;
    comp->tokenize().visit(*this);
    return;
  default:
    throw ScriptError{UNEXPECTED_TOKEN{}};
  }
}

void ParseExpression::operator()(STRING_LITERAL string_literal) {
  auto encoded_it = comp->str_to_const.find(string_literal.pool_view);
  if (encoded_it == comp->str_to_const.end()) {
    auto encode_slice = [](auto str_slice) {
      std::inplace_vector<char, 8> mem_slot{std::from_range, str_slice};
      std::uint64_t dest{};
      std::memcpy(&dest, mem_slot.data(), mem_slot.size());
      return dest;
    };
    auto encoded_view = string_literal.pool_view | std::views::chunk(8) |
                        std::views::transform(encode_slice);
    auto newly_created_it = comp->const_pool.insert(
        comp->const_pool.end(), {std::from_range, encoded_view});
    std::size_t newly_created_idx =
        std::distance(comp->const_pool.begin(), newly_created_it);
    auto insertion_ret = comp->str_to_const.insert(
        {string_literal.pool_view, newly_created_idx});
    encoded_it = insertion_ret.first;
  }
  std::uint8_t regfile_idx{static_cast<std::uint8_t>(stmt->reflection.size())};
  func->instruct_vec.push_back(
      HEAP_ALLOC{.dest = regfile_idx, .const_idx = encoded_it->second});
  stmt->reflection.push_back(DATATYPE::T_STRING);
}

void ParseExpression::operator()(NUMERIC_LITERAL num_literal) {
  throw ScriptError{INVALID_NUMERIC_LITERAL{}};
}
} // namespace Manadrain
