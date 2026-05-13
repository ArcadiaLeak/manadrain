#include <cassert>
#include <inplace_vector>
#include <unordered_map>

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

std::optional<char32_t> Language::forward() {
  if (position >= text_input.size())
    backtrace.emplace();
  else {
    ucs4_t ch;
    int advance{u8_mbtoucr(&ch, text_input.data() + position,
                           text_input.size() - position)};
    assert(advance >= 0);
    position += advance;
    backtrace.push(ch);
  }
  return backtrace.top();
}

std::generator<std::optional<char32_t>> Language::traverse() {
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

void Language::backward() {
  std::optional behind{backtrace.top()};
  position -= std::ranges::distance(
      behind | std::views::transform(traverse_ucs4) | std::views::join);
  backtrace.pop();
}

void Language::backward(std::size_t N) {
  for (int i = 0; i < N; ++i)
    backward();
}

bool has_code_point(std::optional<char32_t> point_opt) {
  return point_opt.has_value();
}

TOKEN Language::tokenize_word(char32_t leading) {
  std::string identifier_str{std::from_range, traverse_ucs4(leading)};
  auto xid_continue_view =
      traverse() | std::views::take_while(has_code_point) | std::views::join |
      std::views::take_while(uc_is_property_xid_continue) |
      std::views::transform(traverse_ucs4) | std::views::join;
  identifier_str.append_range(xid_continue_view);
  backward();
  auto reserved_it = reserved_umap.find(identifier_str);
  if (reserved_it != reserved_umap.end())
    return reserved_it->second;
  auto insertion_ret = string_pool.insert(std::move(identifier_str));
  return IDENTIFIER{*insertion_ret.first};
}

TOKEN Language::tokenize() {
  for (char32_t leading :
       traverse() | std::views::take_while(has_code_point) | std::views::join) {
    if (uc_is_property_white_space(leading))
      continue;
    if (uc_is_property_xid_start(leading) || leading == '_')
      return tokenize_word(leading);
    return leading;
  }
  return std::monostate{};
}

void Language::compile_text() { tokenize().visit(ParseDeclaration{this}); }

void ParseDeclaration::operator()(RESERVED reserved) {
  switch (reserved) {
  case RESERVED::W_FUNCTION:
    lang->tokenize().visit(ParseFunctionDecl{lang});
    return;
  default:
    throw LanguageError{UNEXPECTED_TOKEN{}};
  }
}

void ParseFunctionDecl::operator()(IDENTIFIER identifier) {
  switch (stage) {
  case 0:
    funcname = identifier.pool_view;
    ++stage;
    lang->tokenize().visit(*this);
    return;
  default:
    throw LanguageError{UNEXPECTED_TOKEN{}};
  }
}

void ParseFunctionDecl::operator()(char32_t punct) {
  if (stage == 1 && punct == '(') {
    ++stage;
    lang->tokenize().visit(*this);
    return;
  }
  if (stage == 2 && punct == ')') {
    ++stage;
    lang->tokenize().visit(*this);
    return;
  }
  if (stage == 3 && punct == ':') {
    ++stage;
    lang->tokenize().visit(*this);
    return;
  }
  throw LanguageError{UNEXPECTED_TOKEN{}};
}

TYPE_ANNOTATION parse_type_annotation(RESERVED reserved) {
  switch (reserved) {
  case RESERVED::W_INT:
    return TYPE_ANNOTATION::T_I32;
  default:
    throw LanguageError{INVALID_TYPE_ANNOTATION{}};
  }
}

void ParseFunctionDecl::operator()(RESERVED reserved) {
  switch (stage) {
  case 4:
    return_type = parse_type_annotation(reserved);
    ++stage;
    lang->tokenize().visit(*this);
    return;
  default:
    throw LanguageError{UNEXPECTED_TOKEN{}};
  }
}
} // namespace Manadrain
