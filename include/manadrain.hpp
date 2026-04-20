#include <bitset>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "atom_prealloc.hpp"

namespace Manadrain {
enum class ESCAPE_ERR { MALFORMED };
enum class NUMBER_ERR { INVALID_LITERAL, INTEGER_OVERFLOW };
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
enum class TOK_OPERATOR { EQ_STRICT, EQ_SLOPPY };
using TOKEN = std::variant<std::monostate, PARSE_ERRMSG, char32_t, TOK_STRING,
                           TOK_IDENTI, double, TOK_OPERATOR>;

struct EXPR_CALL;
struct EXPR_MEMBER;
struct EXPR_BINARY;
struct EXPR_OBJECT;
struct EXPR_ARRACCESS;
struct EXPR_ASSIGN;
constexpr int EXPRV_ERROR = 0;
using EXPRESSION = std::variant<PARSE_ERRMSG, TOK_STRING, TOK_IDENTI, double,
                                EXPR_CALL, EXPR_MEMBER, EXPR_BINARY,
                                EXPR_OBJECT, EXPR_ARRACCESS, EXPR_ASSIGN>;
using EXPR_PTR = std::shared_ptr<EXPRESSION>;
struct EXPR_CALL {
  EXPR_PTR callee;
  std::vector<EXPR_PTR> arguments;
};
struct EXPR_MEMBER {
  EXPR_PTR object;
  TOK_IDENTI property;
};
struct EXPR_BINARY {
  EXPR_PTR left;
  EXPR_PTR right;
  TOK_OPERATOR bin_op;
};
struct EXPR_ASSIGN {
  EXPR_PTR left;
  EXPR_PTR right;
};
struct EXPR_OBJECT {};
struct EXPR_ARRACCESS {
  EXPR_PTR object;
  EXPR_PTR property;
};

struct STMT_VARDECL {
  std::size_t p_kind;
  TOK_IDENTI identifier;
  EXPR_PTR initializer;
};
using STATEMENT = std::variant<STMT_VARDECL, EXPRESSION>;

constexpr std::uint8_t MEMORY_ALIGNMENT = 8;

class Scanner {
public:
  bool reached_eof() { return buffer_idx >= buffer.size(); }
  std::optional<char32_t> next();
  std::optional<char32_t> peek();
  void prev();
  void backtrack(std::size_t N);

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

class SpaceChewer : public Scanner {
public:
  void chewLF();
  std::expected<bool, PARSE_ERRMSG> chewSpace1(char32_t ch);

  bool newlineSeen() { return newline_seen; }
  void unseeNewline() { newline_seen = 0; }

private:
  bool newline_seen;

  std::expected<bool, PARSE_ERRMSG> chew_comment(char32_t ch);
  std::expected<bool, PARSE_ERRMSG> chew_comment_block();
  bool chew_comment_line();
};

class AtomTokenizer : public SpaceChewer {
public:
  std::string str1_temp;
  void str1_encode(char32_t cp);

  std::size_t find_atom();
  std::size_t alloc_atom();

private:
  std::unordered_map<std::string, std::size_t> atom_umap;
  std::vector<char> atom_arena{std::from_range, atom_prealloc_buf};
};

class StringTokenizer : public AtomTokenizer {
public:
  TOKEN tokenize(char32_t separator);

private:
  std::optional<char32_t> decode_octal();
  std::optional<char32_t> decode_hex();
  std::variant<std::monostate, char32_t, PARSE_ERRMSG>
  decode_escape(char32_t leading);
  std::variant<std::monostate, char32_t, PARSE_ERRMSG>
  decode_special(char32_t separator, char32_t ch);
};

class IdentifierTokenizer : public StringTokenizer {
public:
  std::optional<TOKEN> tokenize();

private:
  std::optional<char32_t> decode_uni_braced();
  std::optional<char32_t> decode_uni_fixed(char32_t leading);
  std::optional<char32_t> decode_uni();
  std::expected<int, PARSE_ERRMSG> decode_escape(char32_t leading);
  bool encode_uchar(char32_t ch);
};

class Tokenizer : public IdentifierTokenizer {
public:
  TOKEN tokenize();

private:
  std::optional<TOKEN> tokenize_lookahead(char32_t leading);
};

struct Parser : Tokenizer {
  TOKEN token_curr;
  std::vector<STATEMENT> program;

  EXPR_PTR parse_assign_expr();
  EXPR_PTR parse_binary_expr();
  EXPR_PTR parse_postfix_expr();
  std::pair<bool, EXPR_PTR> parse_postfix_expr(EXPR_PTR expression);
  EXPRESSION parse_primary_expr(char32_t punct);
  EXPRESSION parse_primary_expr();
  std::pair<bool, EXPR_PTR> parse_arg_expr();
  EXPR_PTR parse_call_expr(EXPR_PTR callee);
  EXPR_PTR parse_member_expr(EXPR_PTR object);
  EXPR_PTR parse_array_access(EXPR_PTR object);

  std::variant<std::monostate, STMT_VARDECL, PARSE_ERRMSG>
  parse_variable_decl();
  std::expected<void, PARSE_ERRMSG> parse_statement();
  std::expected<void, PARSE_ERRMSG> expect_statement_end();

  bool parse();
};
} // namespace Manadrain
