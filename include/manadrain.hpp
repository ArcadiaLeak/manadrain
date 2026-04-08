#include <deque>
#include <string>
#include <unordered_map>
#include <variant>

namespace Manadrain {
enum class STRICTNESS { SLOPPY, STRICT };

struct K_TOKEN_LET {};
struct K_TOKEN_CONST {};
struct K_TOKEN_VAR {};
struct K_TOKEN_EOF {};
struct K_TOKEN_IDENT {};
struct K_TOKEN_STRING {};
struct K_TOKEN_ERROR {};
struct K_TOKEN_UCHAR {};
using TOKEN_KIND = std::variant<K_TOKEN_LET,
                                K_TOKEN_CONST,
                                K_TOKEN_VAR,
                                K_TOKEN_EOF,
                                K_TOKEN_IDENT,
                                K_TOKEN_STRING,
                                K_TOKEN_ERROR,
                                K_TOKEN_UCHAR>;

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

  TOKEN_KIND kind;
  bool newline_seen;
  char32_t uchar;
  PAYLOAD_STR str;
  PAYLOAD_IDENT ident;

  bool is_pseudo_keyword(TOKEN_KIND tok_kind);

  template <typename T>
  bool is_kind() {
    return std::holds_alternative<T>(kind);
  }
};

struct EXPR_IDENT {
  std::size_t atom_idx;
};
using EXPRESSION = std::variant<EXPR_IDENT>;

using VAR_INTRO = std::variant<K_TOKEN_LET, K_TOKEN_CONST, K_TOKEN_VAR>;
struct STMT_VARDECL {
  VAR_INTRO intro;
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
using PARSE_ERR =
    std::variant<std::monostate, BAD_ESCAPE, BAD_STRING, BAD_COMMENT>;

enum class PARSE_OK { COMMIT, REVERT };
using CMD_EXIT = std::variant<PARSE_OK, PARSE_ERR>;

struct PARSE_IDENT;
struct PARSE_STRING;
struct PARSE_TOKEN;
struct PARSE_VARDECL;

struct ParseDriver {
  std::basic_string<std::uint8_t> buffer;
  std::uint32_t buffer_idx;

  STRICTNESS strictness;
  PARSE_ERR known_err;
  TOKEN token;

  std::unordered_map<std::string_view, std::size_t> atom_umap;
  std::deque<std::string> atom_deq;
  std::deque<STATEMENT> program;

  std::string ch_temp;
  std::size_t get_atom();

  std::optional<char32_t> peek();
  std::optional<char32_t> shift();
  void drop(std::uint32_t count);

  template <typename T>
  bool command(T& cmd_functor) {
    std::uint32_t idx_before{buffer_idx};
    CMD_EXIT cmd_exit = cmd_functor(*this);
    switch (cmd_exit.index()) {
      case 0:
        if (std::get<0>(cmd_exit) == PARSE_OK::COMMIT)
          break;
        [[fallthrough]];
      case 1:
        buffer_idx = idx_before;
        break;
    }
    if (cmd_exit.index() == 1) {
      known_err = std::get<1>(cmd_exit);
      return 1;
    }
    return 0;
  }

  bool find_static_atom(PARSE_IDENT&);
  bool find_static_atom(PARSE_STRING&);

  bool parse_comment_line(PARSE_TOKEN&);
  bool parse_comment_block(PARSE_TOKEN&);
  void parse_comment(PARSE_TOKEN&);
  bool parse(PARSE_TOKEN&);

  CMD_EXIT parse(PARSE_VARDECL&);

  bool parse();
};
}  // namespace Manadrain
