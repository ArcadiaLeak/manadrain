#include <bitset>
#include <expected>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "atom_prealloc.hpp"

namespace Manadrain {
enum class ESCAPE_ERR { MALFORMED };
enum class NUMBER_ERR { INVALID_LITERAL };
enum class UNEXPECTED_ERR { STRING_END, COMMENT_END, THIS_TOKEN };
enum class NEEDED_ERR {
  COMMA,
  SEMICOLON,
  CLOSING_BRACE,
  CLOSING_BRACKET,
  VARIABLE_NAME,
  FIELD_NAME
};
using PARSE_ERRMSG =
    std::variant<ESCAPE_ERR, NUMBER_ERR, UNEXPECTED_ERR, NEEDED_ERR>;

struct TOK_STRING {
  char32_t separator;
  std::size_t p_atom;
  bool operator==(const TOK_STRING &) const = default;
};
struct TOK_IDENTI {
  bool has_escape;
  std::size_t p_atom;
  bool operator==(const TOK_IDENTI &) const = default;
};
enum TOKV_INDEX {
  TOKV_EOF,
  TOKV_ERROR,
  TOKV_PUNCT,
  TOKV_STRING,
  TOKV_IDENTI,
  TOKV_NUMBER,
  TOKV_OP
};
enum class TOK_OPERATOR { EQ_STRICT, EQ_SLOPPY, DIV_ASSIGN };
using TOKEN = std::variant<std::monostate, PARSE_ERRMSG, char32_t, TOK_STRING,
                           TOK_IDENTI, double, TOK_OPERATOR>;

struct EXPR_CALL;
struct EXPR_MEMBER;
struct EXPR_BINARY;
struct EXPR_OBJECT;
struct EXPR_ACCESS;
struct EXPR_ASSIGN;
using EXPRESSION =
    std::variant<std::monostate, TOK_STRING, TOK_IDENTI, double,
                 std::unique_ptr<EXPR_CALL>, std::unique_ptr<EXPR_MEMBER>,
                 std::unique_ptr<EXPR_BINARY>, std::unique_ptr<EXPR_OBJECT>,
                 std::unique_ptr<EXPR_ACCESS>, std::unique_ptr<EXPR_ASSIGN>>;
struct EXPR_CALL {
  EXPRESSION callee;
  std::vector<EXPRESSION> arguments;
};
struct EXPR_MEMBER {
  EXPRESSION object;
  TOK_IDENTI property;
};
struct EXPR_BINARY {
  EXPRESSION left;
  EXPRESSION right;
  TOK_OPERATOR bin_op;
};
struct EXPR_ASSIGN {
  EXPRESSION left;
  EXPRESSION right;
};
struct EXPR_OBJECT {};
struct EXPR_ACCESS {
  EXPRESSION object;
  EXPRESSION property;
};

struct STMT_VARDECL {
  std::size_t p_kind;
  TOK_IDENTI identifier;
  EXPRESSION initializer;
};
using STATEMENT = std::variant<STMT_VARDECL, EXPRESSION>;

constexpr std::uint8_t MEMORY_ALIGNMENT = 8;

class Scanner {
public:
  bool reached_eof() { return buffer_idx >= buffer.size(); }
  char32_t next_u();
  std::optional<char32_t> next();
  std::optional<char32_t> peek();
  void prev();
  void backtrack(std::size_t N);
  void chewLF();

  void setBuffer(const std::basic_string<std::uint8_t> &buffer_ref) {
    buffer = buffer_ref;
  }
  void setBuffer(std::basic_string<std::uint8_t> &&buffer_ref) {
    buffer = std::move(buffer_ref);
  }

private:
  std::basic_string<std::uint8_t> buffer;
  int buffer_idx;
};

class NumberTokenizer : public Scanner {
public:
  TOKEN tokenize(char32_t leading);

private:
  void peek_behind_octal(std::optional<char32_t> &trail_opt);

  enum class PREFIX { ZERO_LEAD_A, HEX, BINARY, OCTAL, ZERO_LEAD_8 };
  PREFIX decode_prefix();
};

class AtomTokenizer : public NumberTokenizer {
public:
  std::string my_atom;

  std::size_t atomFind();
  std::size_t atomAlloc();

private:
  std::unordered_map<std::string, std::size_t> atom_umap;
  std::vector<char> atom_arena{std::from_range, atom_prealloc_buf};
};

class StringTokenizer : public AtomTokenizer {
public:
  TOKEN tokenize(char32_t separator);

private:
  std::optional<char32_t> decode_esc8();
  std::optional<char32_t> decode_xseq();
  std::variant<std::monostate, char32_t, PARSE_ERRMSG>
  decode_escape(char32_t leading);
  std::variant<std::monostate, char32_t, PARSE_ERRMSG>
  decode_special(char32_t separator, char32_t ch);
};

class IdentifierTokenizer : public StringTokenizer {
public:
  std::optional<TOKEN> tokenize(char32_t leading);

private:
  bool encode_uchar(char32_t ch);
  std::optional<char32_t> decode_uni_braced();
  std::optional<char32_t> decode_uni_fixed(char32_t leading);
  std::expected<int, PARSE_ERRMSG> decode_escape(char32_t leading);

protected:
  std::optional<char32_t> decode_uni();
};

class Tokenizer : public IdentifierTokenizer {
public:
  TOKEN tokenize();
  bool newlineSeen() { return newline_seen; }

private:
  bool newline_seen;
};

class Parser : public Tokenizer {
public:
  std::vector<STATEMENT> program;
  bool parse();

private:
  TOKEN my_token;
  EXPRESSION my_expression;

  std::expected<void, PARSE_ERRMSG> parse_assign_expr();
  std::expected<void, PARSE_ERRMSG> parse_binary_expr();
  std::expected<void, PARSE_ERRMSG> parse_postfix_expr();
  std::expected<void, PARSE_ERRMSG> parse_primary_expr();
  std::expected<void, PARSE_ERRMSG> parse_call_expr();
  std::expected<void, PARSE_ERRMSG> parse_member_expr();
  std::expected<void, PARSE_ERRMSG> parse_access_expr();

  std::variant<std::monostate, STMT_VARDECL, PARSE_ERRMSG>
  parse_variable_decl();
  std::expected<void, PARSE_ERRMSG> parse_statement();
  std::expected<void, PARSE_ERRMSG> expect_statement_end();
};
} // namespace Manadrain
