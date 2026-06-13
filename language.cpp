#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <flat_map>
#include <generator>
#include <inplace_vector>
#include <list>
#include <map>
#include <memory>
#include <memory_resource>
#include <meta>
#include <mutex>
#include <new>
#include <optional>
#include <print>
#include <ranges>
#include <set>
#include <thread>
#include <variant>
#include <vector>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"

namespace Manadrain {
class CompilationError : public std::runtime_error {
public:
  CompilationError(const char *msg) : std::runtime_error{msg} {}
  CompilationError() : std::runtime_error{"compilation error!"} {}
};
class UnexpectedStringEnd final : public CompilationError {
public:
  UnexpectedStringEnd() : CompilationError{"unexpected string end!"} {}
};
class UnexpectedToken final : public CompilationError {
public:
  UnexpectedToken() : CompilationError{"unexpected token!"} {}
};
class MissingPunctuation final : public CompilationError {
public:
  MissingPunctuation(char32_t ch)
      : CompilationError{"missing punctuation!"}, must_be{ch} {}
  char32_t must_be;
};
class MissingFormalParameter final : public CompilationError {
public:
  MissingFormalParameter() : CompilationError{"missing formal parameter!"} {}
};
class MissingFunctionName final : public CompilationError {
public:
  MissingFunctionName() : CompilationError{"missing function_name!"} {}
};
class DuplicateDeclaration final : public CompilationError {
public:
  DuplicateDeclaration() : CompilationError{"duplicate declaration!"} {}
};
class MissingPropertyName final : public CompilationError {
public:
  MissingPropertyName() : CompilationError{"missing property name!"} {}
};
class InvalidPropertyName final : public CompilationError {
public:
  InvalidPropertyName() : CompilationError{"invalid property name!"} {}
};
class InvalidPropertyAccess final : public CompilationError {
public:
  InvalidPropertyAccess() : CompilationError{"invalid property access!"} {}
};
class MissingVariableName final : public CompilationError {
public:
  MissingVariableName() : CompilationError{"missing variable name!"} {}
};
class InvalidMethodAccess final : public CompilationError {
public:
  InvalidMethodAccess() : CompilationError{"invalid method access!"} {}
};
class InvalidVariableAccess final : public CompilationError {
public:
  InvalidVariableAccess() : CompilationError{"invalid variable access!"} {}
};
class InvalidReturnStatement final : public CompilationError {
public:
  InvalidReturnStatement() : CompilationError{"invalid return statement!"} {}
};
class InvalidAssignment final : public CompilationError {
public:
  InvalidAssignment() : CompilationError{"invalid assignment!"} {}
};
class UnresolvableCircularity final : public CompilationError {
public:
  UnresolvableCircularity() : CompilationError{"unresolvable circularity!"} {}
};

enum class Keyword {
  MONOSTATE,
  K_CONST,
  K_LET,
  K_VAR,
  K_CLASS,
  K_FUNCTION,
  K_RETURN,
  K_IMPORT,
  K_EXPORT,
  K_FROM,
  K_AS,
  K_DEFAULT,
  K_UNDEFINED,
  K_NULL,
  K_TRUE,
  K_FALSE,
  K_IF,
  K_ELSE,
  K_WHILE,
  K_FOR,
  K_DO,
  K_BREAK,
  K_CONTINUE,
  K_SWITCH,
  K_TYPEOF
};
enum class Operator {
  DOUBLE_EQUALS,
  TRIPLE_EQUALS,
  BANG_EQUALS,
  BANG_DOUBLE_EQUALS,
  DIVIDE_ASSIGN,
  BITWISE_CONJUNCT_ASSIGN,
  LOGICAL_CONJUNCT_ASSIGN,
  LOGICAL_CONJUNCT,
  BITWISE_DISJUNCT_ASSIGN,
  LOGICAL_DISJUNCT_ASSIGN,
  LOGICAL_DISJUNCT
};

struct Identifier {
  std::size_t offset;
  auto operator<=>(const Identifier &) const = default;
};

using Token =
    std::variant<std::monostate, char32_t, std::int64_t, double, Operator,
                 Keyword, Identifier, std::string_view, std::u16string_view>;

inline constexpr std::size_t OFFSET_console{0};
inline constexpr std::size_t OFFSET_log{1};
inline constexpr std::size_t OFFSET_length{2};

class Machine {};

class Compiler {
public:
  Compiler(Machine &m) : machine{m} {}

  std::unique_ptr<const std::vector<std::uint8_t>> text_buffer;
  void parse_text();

private:
  Machine &machine;

  std::map<std::string, Identifier> atom_atlas;
  std::set<std::string> token_strings;
  std::set<std::u16string> token_u16strings;

  std::size_t position;
  std::vector<std::optional<char32_t>> backtrace;

  std::generator<std::optional<char32_t>> traverse_text();
  std::optional<char32_t> forward();
  void backward();
  void backward(std::size_t N);

  Token tokenize_identifier(char32_t leading);
  Token tokenize_string_literal(char32_t separator);
  Token tokenize_numeric_literal(char32_t leading);

  Token last_token;
  void assert_punct(char32_t must_be);
  void tokenize();
};

static const std::flat_map<std::string_view, Keyword> keyword_atlas{
    {"const", Keyword::K_CONST},       {"let", Keyword::K_LET},
    {"var", Keyword::K_VAR},           {"class", Keyword::K_CLASS},
    {"function", Keyword::K_FUNCTION}, {"return", Keyword::K_RETURN},
    {"import", Keyword::K_IMPORT},     {"export", Keyword::K_EXPORT},
    {"from", Keyword::K_FROM},         {"as", Keyword::K_AS},
    {"default", Keyword::K_DEFAULT},   {"undefined", Keyword::K_UNDEFINED},
    {"null", Keyword::K_NULL},         {"true", Keyword::K_TRUE},
    {"false", Keyword::K_FALSE},       {"if", Keyword::K_IF},
    {"else", Keyword::K_ELSE},         {"while", Keyword::K_WHILE},
    {"for", Keyword::K_FOR},           {"do", Keyword::K_DO},
    {"break", Keyword::K_BREAK},       {"continue", Keyword::K_CONTINUE},
    {"switch", Keyword::K_SWITCH},     {"typeof", Keyword::K_TYPEOF}};
static const std::flat_map<std::string_view, std::size_t> identifier_atlas{
    {"console", OFFSET_console},
    {"log", OFFSET_log},
    {"length", OFFSET_length}};
static const std::array persistent_identifiers{
    std::to_array<std::string_view>({"console", "log", "length"})};

std::optional<char32_t> Compiler::forward() {
  if (position >= text_buffer->size())
    backtrace.push_back(std::nullopt);
  else {
    ucs4_t ch;
    int advance{u8_mbtoucr(&ch, text_buffer->data() + position,
                           text_buffer->size() - position)};
    assert(advance >= 0);
    position += advance;
    backtrace.push_back(ch);
  }
  return backtrace.back();
}

std::generator<std::optional<char32_t>> Compiler::traverse_text() {
  while (1)
    co_yield forward();
}

std::inplace_vector<char, 6> traverse_u8(ucs4_t cp) {
  std::array<std::uint8_t, 6> buffer{};
  int advance{u8_uctomb(buffer.data(), cp, buffer.size())};
  assert(advance >= 0);
  return {std::from_range, buffer | std::views::take(advance)};
}

std::inplace_vector<char16_t, 2> traverse_u16(ucs4_t cp) {
  std::array<std::uint16_t, 2> buffer{};
  int advance{u16_uctomb(buffer.data(), cp, buffer.size())};
  assert(advance >= 0);
  return {std::from_range, buffer | std::views::take(advance)};
}

void Compiler::backward() {
  std::optional<char32_t> behind{backtrace.back()};
  position -= std::ranges::distance(
      behind | std::views::transform(traverse_u8) | std::views::join);
  backtrace.pop_back();
}

void Compiler::backward(std::size_t N) {
  for (int i = 0; i < N; ++i)
    backward();
}

Token Compiler::tokenize_identifier(char32_t leading) {
  std::string identifier_str{std::from_range, traverse_u8(leading)};
  auto does_exist = [](auto an_optional) { return an_optional.has_value(); };
  auto xid_continue_view =
      traverse_text() | std::views::take_while(does_exist) | std::views::join |
      std::views::take_while(uc_is_property_xid_continue) |
      std::views::transform(traverse_u8) | std::views::join;
  identifier_str.append_range(xid_continue_view);
  backward();
  auto iter_reserved = keyword_atlas.find(identifier_str);
  if (iter_reserved != keyword_atlas.end())
    return iter_reserved->second;
  auto iter_persistent = identifier_atlas.find(identifier_str);
  if (iter_persistent != identifier_atlas.end())
    return Identifier{iter_persistent->second};
  Identifier identifier{persistent_identifiers.size() + atom_atlas.size() - 1};
  auto emplace_ret =
      atom_atlas.try_emplace(std::move(identifier_str), identifier);
  return emplace_ret.first->second;
}

Token Compiler::tokenize_string_literal(char32_t separator) {
  std::u16string u16_literal{};
  auto match_literal_end = [separator](auto code_point) {
    return code_point != separator;
  };
  auto literal_view =
      traverse_text() | std::views::take_while(match_literal_end);
  for (std::optional<char32_t> leading : literal_view) {
    if (leading == '\r' && forward() != '\n')
      backward();
    if (not leading || leading == '\r' || leading == '\n')
      throw UnexpectedStringEnd{};
    u16_literal.append_range(traverse_u16(*leading));
  }
  if (std::ranges::all_of(u16_literal, [](char16_t ch) { return ch < 128; })) {
    auto cast_to_ascii = [](char16_t ch) { return static_cast<char>(ch); };
    auto ascii_string = u16_literal | std::views::transform(cast_to_ascii);
    auto [string_iter, _] =
        token_strings.emplace(std::from_range, ascii_string);
    return *string_iter;
  }
  auto [u16string_iter, _] = token_u16strings.emplace(std::move(u16_literal));
  return *u16string_iter;
}

Token Compiler::tokenize_numeric_literal(char32_t leading) {
  std::string numeric_str{std::from_range, traverse_u8(leading)};
  auto match_nullopt = [](auto an_optional) { return an_optional.has_value(); };
  auto match_digit = [](char32_t code_point) {
    return std::isdigit(code_point);
  };
  numeric_str.append_range(
      traverse_text() | std::views::take_while(match_nullopt) |
      std::views::join | std::views::take_while(match_digit) |
      std::views::transform(traverse_u8) | std::views::join);
  backward();
  std::int64_t num_literal{};
  std::from_chars(numeric_str.data(), numeric_str.data() + numeric_str.size(),
                  num_literal);
  return num_literal;
}

void Compiler::tokenize() {
  auto does_exist = [](auto an_optional) { return an_optional.has_value(); };
  auto match_lineterm = [](std::optional<char32_t> uchar) {
    static const std::array lineterm{std::to_array<std::optional<char32_t>>(
        {std::nullopt, '\n', '\r', 0x2028, 0x2029})};
    return !std::ranges::binary_search(lineterm, uchar);
  };
  for (char32_t leading : traverse_text() | std::views::take_while(does_exist) |
                              std::views::join) {
    if (uc_is_property_white_space(leading))
      continue;
    std::inplace_vector<char32_t, 4> headbuf{leading};
    headbuf.append_range(forward());
    if (std::ranges::equal(headbuf, std::to_array({'/', '/'}))) {
      auto comment = traverse_text() | std::views::take_while(match_lineterm) |
                     std::views::join;
      std::ranges::for_each(comment, [](auto) {});
      backward();
      continue;
    } else
      backward();
    headbuf = {leading};
    headbuf.append_range(forward());
    if (std::ranges::equal(headbuf, std::to_array({'|', '|'}))) {
      last_token = Operator::LOGICAL_DISJUNCT;
      return;
    } else
      backward();
    if (std::ranges::binary_search(std::to_array({'"', '\'', '`'}), leading)) {
      last_token = tokenize_string_literal(leading);
      return;
    }
    if (uc_is_property_xid_start(leading) || leading == '_') {
      last_token = tokenize_identifier(leading);
      return;
    }
    static const std::array legal_punct{std::to_array<char32_t>(
        {'(', ')', '*', '+', ',', '-', '.', '/', ':', ';', '=', '{', '}'})};
    if (std::ranges::binary_search(legal_punct, leading)) {
      last_token = leading;
      return;
    }
    if (std::isdigit(leading)) {
      last_token = tokenize_numeric_literal(leading);
      return;
    }
    throw UnexpectedToken{};
  }
  last_token = std::monostate{};
}

void Compiler::assert_punct(char32_t must_be) {
  char32_t *alter_ptr = std::get_if<char32_t>(&last_token);
  if (alter_ptr && *alter_ptr == must_be)
    return;
  throw MissingPunctuation{must_be};
}

void Compiler::parse_text() {
  while (1) {
    tokenize();
    if (std::holds_alternative<std::monostate>(last_token))
      break;
    // entry_module.parse_statement();
  }
}

Language::Language() { machine = std::make_unique<Machine>(); }
Language::~Language() = default;
Language::Language(Language &&other) noexcept = default;
Language &Language::operator=(Language &&other) noexcept = default;

void Language::compile_and_execute() {
  Manadrain::Compiler compiler{*machine};
  compiler.text_buffer = std::move(text_buffer);
}
} // namespace Manadrain
