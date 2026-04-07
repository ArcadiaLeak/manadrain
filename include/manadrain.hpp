#include <deque>
#include <string>
#include <unordered_map>
#include <variant>

namespace Manadrain {
enum class STRICTNESS { SLOPPY, STRICT };
enum class TOKEN_TYPE {
  T_LET,
  T_CONST,
  T_VAR,
  T_EOF,
  T_IDENT,
  T_STRING,
  T_ERROR,
  T_UCHAR
};

struct TOKEN {
  struct PAYLOAD_STR {
    char32_t sep;
    std::size_t atom_idx;
  };
  struct PAYLOAD_IDENT {
    bool has_escape;
    bool is_reserved;
    std::size_t atom_idx;
  };

  TOKEN_TYPE type;
  bool newline_seen;
  char32_t uchar;
  PAYLOAD_STR str;
  PAYLOAD_IDENT ident;

  bool is_pseudo_keyword(TOKEN_TYPE tok_type);
  bool is_vardecl_intro();
};

struct EXPR_IDENT {
  std::size_t atom_idx;
};
using EXPRESSION = std::variant<EXPR_IDENT>;

enum class VARDECL_KIND { K_LET, K_CONST, K_VAR };
struct STMT_VARDECL {
  VARDECL_KIND kind;
  TOKEN::PAYLOAD_IDENT ident;
  TOKEN::PAYLOAD_STR init;
};
using STATEMENT = std::variant<STMT_VARDECL, EXPRESSION>;

enum class BAD_ESCAPE { MALFORMED, PER_SE_BACKSLASH, OCTAL_SEQ };
enum class BAD_STRING {
  UNEXPECTED_END,
  OCTAL_SEQ_IN_ESCAPE,
  MALFORMED_SEQ_IN_ESCAPE
};
enum class BAD_COMMENT { UNEXPECTED_END };
using PARSE_ERROR =
    std::variant<std::monostate, BAD_ESCAPE, BAD_STRING, BAD_COMMENT>;

struct ParseDriver {
  std::basic_string<std::uint8_t> buffer;
  std::uint32_t buffer_idx;

  STRICTNESS strictness;
  PARSE_ERROR known_err;
  TOKEN token;

  std::unordered_map<std::string_view, std::size_t> atom_umap;
  std::deque<std::string> atom_deq;
  std::deque<STATEMENT> program;

  std::string ch_temp;
  std::size_t obtain_atom();

  std::optional<char32_t> peek();
  std::optional<char32_t> shift();
  void drop(std::uint32_t count);

  template <typename T>
  bool exec_command(T& command) {
    std::uint32_t idx_before{buffer_idx};
    bool backtrack = command.exec(*this);
    if (backtrack)
      buffer_idx = idx_before;
    return backtrack;
  }

  bool parse();
};
}  // namespace Manadrain
