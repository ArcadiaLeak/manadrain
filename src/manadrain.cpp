#include <limits>
#include <stdexcept>

#include <unicode/uchar.h>
#include <unicode/ustring.h>

#include "manadrain.hpp"

static bool is_hi_surrogate(char32_t c) {
  return (c >> 10) == (0xD800 >> 10); /* 0xD800-0xDBFF */
}
static bool is_lo_surrogate(char32_t c) {
  return (c >> 10) == (0xDC00 >> 10); /* 0xDC00-0xDFFF */
}
static char32_t from_surrogate(char32_t hi, char32_t lo) {
  return 0x10000 + 0x400 * (hi - 0xD800) + (lo - 0xDC00);
}
static bool is_lineterm(char32_t ch) {
  return ch == '\r' || ch == '\n' || ch == 0x2028 || ch == 0x2029;
}

namespace Manadrain {
static const std::array reserved_arr =
    std::to_array<std::tuple<P_ATOM, KEYWORD_KIND, STRICTNESS>>(
        {{S_ATOM_var, KEYWORD_KIND::K_VAR, STRICTNESS::SLOPPY},
         {S_ATOM_const, KEYWORD_KIND::K_CONST, STRICTNESS::SLOPPY},
         {S_ATOM_let, KEYWORD_KIND::K_LET, STRICTNESS::STRICT}});

std::optional<KEYWORD_KIND> TOK_IDENTI::match_keyword(STRICTNESS strictness) {
  if (p_atom.pageid >= 0)
    return std::nullopt;
  for (auto rsv_word : reserved_arr) {
    auto [tok_atom, tok_kind, tok_strict] = rsv_word;
    if (p_atom == tok_atom && strictness >= tok_strict && !has_escape)
      return tok_kind;
  }
  return std::nullopt;
}

bool AtomPage::check_for_count(int N) { return ch_bitset.count() >= N; }

bool AtomPage::check_for_window(int N) {
  std::bitset inv_bitset = ~ch_bitset;
  for (int i = 0; i < N - 1; ++i) {
    if (inv_bitset.none())
      return 0;
    inv_bitset &= inv_bitset << 1;
  }
  return inv_bitset.any();
}

int AtomPage::scan_for_window(int N) {
  int n_zeros = 0;
  for (int i = 0; i < ch_bitset.size(); ++i) {
    if (ch_bitset[i])
      n_zeros = 0;
    else {
      n_zeros++;
      if (n_zeros == N) {
        int ret = i - N + 1;
        return ret;
      }
    }
  }
  throw std::runtime_error{"window seeking failed!"};
}

std::optional<std::uint16_t> AtomPage::try_allocate(int N) {
  if (not check_for_count(N))
    return std::nullopt;
  if (not check_for_window(N))
    return std::nullopt;
  int offset = scan_for_window(N);
  allocate(offset, N);
  return static_cast<std::uint16_t>(offset);
}

void AtomPage::allocate(int offset, int N) {
  for (int i = offset; i < offset + N; ++i)
    ch_bitset[i] = 1;
}

void ParseDriver::str1_encode(char32_t cp) {
  std::array<char, 4> buff{};
  UBool is_error{};
  std::uint32_t length{};
  U8_APPEND(buff.data(), length, buff.size(), cp, is_error);
  if (is_error)
    throw std::runtime_error{"codepoint conversion failed!"};
  str1_temp.append(std::string_view{buff.data(), length});
}

char32_t ParseDriver::next() {
  if (reached_eof())
    throw std::runtime_error{"out of bounds in script buffer!"};
  UChar32 ch;
  U8_NEXT_OR_FFFD(buffer.data(), buffer_idx, buffer.size(), ch);
  return ch;
}

void ParseDriver::prev() { U8_BACK_1(buffer.data(), 0, buffer_idx); }

void ParseDriver::backtrack(std::size_t N) {
  for (int i = 0; i < N; ++i)
    prev();
}

void ParseDriver::skip_lf() {
  if (reached_eof() || next() == '\n')
    return;
  prev();
}

char32_t ParseDriver::parse_octo(PARSE_ESCAPE, char32_t octo) {
  if (reached_eof())
    return octo;
  char32_t ahead = next() - '0';
  if (ahead > 7) {
    prev();
    return octo;
  }
  return (octo << 3) | ahead;
}

std::expected<char32_t, int> ParseDriver::parse(PARSE_ESCAPE esc, char32_t ch) {
  switch (ch) {
  case 'b':
    return '\b';
  case 'f':
    return '\f';
  case 'n':
    return '\n';
  case 'r':
    return '\r';
  case 't':
    return '\t';
  case 'v':
    return '\v';
  case '0':
    if (reached_eof() || !std::isdigit(next()))
      return 0;
    [[fallthrough]];
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
    switch (esc.rule) {
    case ESC_RULE::STRING_IN_STRICT_MODE:
      errcode = PARSE_ERRCODE::LEGACY_OCTAL_SEQ;
      return std::unexpected{2};

    case ESC_RULE::STRING_IN_TEMPLATE:
    case ESC_RULE::REGEXP_UTF16:
      errcode = PARSE_ERRCODE::MALFORMED_ESCAPE;
      return std::unexpected{2};

    default: {
      char32_t oct = ch - '0';
      oct = parse_octo(PARSE_ESCAPE{}, oct);
      if (oct >= 32)
        return oct;
      oct = parse_octo(PARSE_ESCAPE{}, oct);
      return oct;
    }
    }
  case '8':
  case '9':
    if (esc.rule == ESC_RULE::STRING_IN_STRICT_MODE ||
        esc.rule == ESC_RULE::STRING_IN_TEMPLATE) {
      errcode = PARSE_ERRCODE::MALFORMED_ESCAPE;
      return std::unexpected{2};
    }
    [[fallthrough]];
  default:
    return std::unexpected{1};
  }
}

std::expected<char32_t, int>
ParseDriver::parse_escape(PARSE_STRING, char32_t separator, char32_t ch) {
  switch (ch) {
  case '\'':
  case '\"':
  case '\0':
  case '\\':
    return ch;
  case '\r':
    skip_lf();
    [[fallthrough]];
  case '\n':
  case 0x2028:
  case 0x2029:
    /* ignore escaped newline sequence */
    return std::unexpected{1};
  }
  ESC_RULE esc_rule = ESC_RULE::STRING_IN_SLOPPY_MODE;
  if (strictness == STRICTNESS::STRICT)
    esc_rule = ESC_RULE::STRING_IN_STRICT_MODE;
  else if (separator == '`')
    esc_rule = ESC_RULE::STRING_IN_TEMPLATE;
  return parse(PARSE_ESCAPE{esc_rule}, ch);
}

std::expected<char32_t, int>
ParseDriver::parse_uchar(PARSE_STRING, char32_t separator, char32_t ch) {
  switch (ch) {
  case '\r':
    skip_lf();
    [[fallthrough]];
  case '\n':
    if (separator == '`')
      return '\n';
    errcode = PARSE_ERRCODE::UNEXPECTED_STRING_END;
    return std::unexpected{2};
  case '\\':
    return reached_eof() ? std::unexpected{1}
                         : parse_escape(PARSE_STRING{}, separator, next());
  default:
    return ch;
  }
}

std::optional<std::monostate> ParseDriver::parse_atom(PARSE_STRING,
                                                      char32_t separator) {
  str1_temp.clear();
  while (1) {
    if (reached_eof()) {
      errcode = PARSE_ERRCODE::UNEXPECTED_STRING_END;
      return std::nullopt;
    }
    std::expected<char32_t, int> ch_exp =
        parse_uchar(PARSE_STRING{}, separator, next());
    if (ch_exp.error_or(0)) {
      if (ch_exp.error() == 1)
        continue;
      if (ch_exp.error() == 2)
        return std::nullopt;
    }
    if (ch_exp.value() == separator)
      return std::monostate{};
    str1_encode(ch_exp.value());
  }
}

bool ParseDriver::is_allowed_uchar(PARSE_IDENT ident, char32_t ch) {
  UProperty must_be = str1_temp.size() > ident.is_private ? UCHAR_XID_CONTINUE
                                                          : UCHAR_XID_START;
  return u_hasBinaryProperty(ch, must_be);
}

std::optional<bool> ParseDriver::parse_uchar(PARSE_IDENT ident) {
  std::array<char32_t, 2> ch{};
  int i;
  for (i = 0; i < 2; ++i) {
    if (reached_eof())
      break;
    ch[i] = next();
  }
  if (ch[0] == '\\') {
    if (ch[1] == 'u') {
      // unimplemented
      return std::nullopt;
    } else {
      errcode = PARSE_ERRCODE::MALFORMED_ESCAPE;
      return std::nullopt;
    }
  } else {
    int j;
    for (j = 0; j < i; j++) {
      if (not is_allowed_uchar(ident, ch[j]))
        break;
      str1_encode(ch[j]);
    }
    backtrack(i - j);
    return j > 0;
  }
}

std::optional<bool> ParseDriver::parse_atom(PARSE_IDENT ident) {
  str1_temp.clear();
  if (ident.is_private)
    str1_temp.push_back('#');

  bool further = 1;
  while (further) {
    std::optional<bool> further_opt = parse_uchar(ident);
    if (not further_opt)
      return std::nullopt;
    further = further_opt.value();
  }
  return str1_temp.size() > ident.is_private;
}

bool ParseDriver::skip_comment_line() {
  if (reached_eof())
    return 0;
  if (is_lineterm(next())) {
    prev();
    return 0;
  }
  return 1;
}

std::optional<bool> ParseDriver::skip_comment_block() {
  std::array<char32_t, 2> ch{};
  int i;
  for (i = 0; i < 2; ++i) {
    if (reached_eof())
      break;
    ch[i] = next();
  }
  if (i == 0) {
    errcode = PARSE_ERRCODE::UNEXPECTED_COMMENT_END;
    return std::nullopt;
  }
  if (ch[0] == '*' && ch[1] == '/')
    return 0;
  backtrack(--i);
  if (is_lineterm(ch[0]))
    newline_seen = 1;
  return 1;
}

std::optional<bool> ParseDriver::skip_comment(char32_t ch) {
  switch (ch) {
  case '/':
    while (1) {
      if (not skip_comment_line())
        return 1;
    }
  case '*':
    while (1) {
      std::optional<bool> comment = skip_comment_block();
      if (not comment)
        return std::nullopt;
      if (not comment.value())
        return 1;
    }
  default:
    prev();
    return 0;
  }
}

std::optional<bool> ParseDriver::skip_ws_1(char32_t ch) {
  switch (ch) {
  case '\r':
    skip_lf();
    [[fallthrough]];

  case '\n':
  case 0x2028:
  case 0x2029:
    newline_seen = 1;
    return 1;

  case '/':
    if (reached_eof())
      return 1;
    if (skip_comment(next()))
      return 1;
    prev();
    return 0;

  default:
    if (u_isWhitespace(ch))
      return 1;
    prev();
    return 0;
  }
}

TOKEN ParseDriver::tokenize(int flags, PARSE_ERRCODE on_mismatch) {
  while (1) {
    if (reached_eof())
      goto return_eof;

    std::optional<bool> no_ws_ahead = skip_ws_1(next());

    if (not no_ws_ahead)
      return std::nullopt;

    if (not no_ws_ahead.value())
      break;
  }

  if (flags & PARSE_STRING::flag) {
    char32_t ch{next()};
    switch (ch) {
    case '\'':
    case '"':
      if (not parse_atom(PARSE_STRING{}, ch))
        return std::nullopt;
      return TOK_STRING{
          .separator = ch,
          .p_atom = find_static_atom()
                        .or_else([this]() { return find_dynamic_atom(); })
                        .value()};
    default:
      prev();
      break;
    }
  }

  if (flags & PARSE_IDENT::flag) {
    std::optional<bool> ident_opt = parse_atom(PARSE_IDENT{});
    if (not ident_opt)
      return std::nullopt;
    if (ident_opt.value())
      return TOK_IDENTI{
          .p_atom = find_static_atom()
                        .or_else([this]() { return find_dynamic_atom(); })
                        .value()};
  }

  if (flags & PARSE_PUNCT::flag)
    return next();

mismatch:
  errcode = on_mismatch;
  return std::nullopt;

return_eof:
  if (flags & PARSE_EOF::flag)
    return TOK_EOF{};
  else
    goto mismatch;
}

std::optional<P_ATOM> ParseDriver::find_static_atom() {
  for (std::uint16_t i = 0; i < reserved_arr.size(); i++) {
    auto [p_atom, _, _] = reserved_arr[i];
    if (str1_temp ==
        std::string_view{neg1_page_buf.data() + p_atom.offset * ATOM_BLOCK,
                         p_atom.length})
      return p_atom;
  }
  return std::nullopt;
}

std::uint16_t block_N(std::size_t len) {
  std::size_t N = len / ATOM_BLOCK + (len % ATOM_BLOCK != 0);
  return static_cast<std::uint16_t>(N % ATOM_PAGE);
}

std::optional<P_ATOM> ParseDriver::find_dynamic_atom() {
  if (str1_temp.size() > ATOM_BLOCK * ATOM_PAGE)
    throw std::runtime_error{"a string literal exceeds limit!"};
  if (not atom_umap.contains(str1_temp)) {
    P_ATOM p_atom = alloc_dynamic_atom();
    int ch_offset = p_atom.offset * ATOM_BLOCK;
    std::string_view atom_strview{
        atom_deq[p_atom.pageid].ch_arr.data() + ch_offset, p_atom.length};
    atom_umap[atom_strview] = p_atom;
  }
  return atom_umap[str1_temp];
}

P_ATOM ParseDriver::alloc_dynamic_atom() {
  std::uint16_t offset = 0,
                length = static_cast<std::uint16_t>(str1_temp.size());
  std::int16_t i;
  for (i = 0; i < atom_deq.size(); ++i) {
    std::optional offset_opt = atom_deq[i].try_allocate(block_N(length));
    if (not offset_opt)
      continue;
    offset = *offset_opt;
    goto copy_and_return;
  }
  if (atom_deq.size() == std::numeric_limits<std::int16_t>::max())
    throw std::runtime_error{"out of space for an atom!"};
  atom_deq.emplace_back();
  atom_deq[i].allocate(0, block_N(length));

copy_and_return:
  std::ranges::copy(str1_temp,
                    atom_deq[i].ch_arr.begin() + offset * ATOM_BLOCK);
  return P_ATOM{i, offset, length};
}

std::optional<KEYWORD_KIND> ParseDriver::parse_beginning(PARSE_STATEMENT) {
  TOKEN token{};
  std::optional<KEYWORD_KIND> keyword{KEYWORD_KIND{}};

  token = tokenize(PARSE_IDENT::flag & PARSE_EOF::flag);
  if (not token)
    return std::nullopt;

  switch (token->index()) {
  case 0:
    return std::nullopt;
  case 3:
    keyword = std::get<3>(token.value()).match_keyword(STRICTNESS::STRICT);
    if (keyword)
      return keyword.value();
    [[fallthrough]];
  default:
    errcode = PARSE_ERRCODE::UNEXPECTED_TOKEN;
    return std::nullopt;
  }
}

std::optional<std::monostate> ParseDriver::parse(PARSE_VARDECL,
                                                 std::size_t idx) {
  TOKEN token{};

  token = tokenize(PARSE_IDENT::flag, PARSE_ERRCODE::NEEDED_VARIABLE_NAME);
  if (not token)
    return std::nullopt;

  std::get<STMT_VARDECL>(program[idx]).identifier =
      std::get<TOK_IDENTI>(token.value());

  token = tokenize(PARSE_PUNCT::flag & PARSE_EOF::flag);
  if (not token)
    return std::nullopt;

  if (std::get<char32_t>(token.value()) == '=') {
    std::get<STMT_VARDECL>(program[idx]).initializer =
        parse(PARSE_POSTFIX_EXPR{});

    token = tokenize(PARSE_PUNCT::flag & PARSE_EOF::flag);
    if (not token)
      return std::nullopt;
  }

  if (std::get<char32_t>(token.value()) != ';') {
    errcode = PARSE_ERRCODE::NEEDED_SEMICOLON;
    return std::nullopt;
  }

  return std::monostate{};
}

std::optional<EXPRESSION> ParseDriver::parse(PARSE_POSTFIX_EXPR) {
  TOKEN token{};

  token = tokenize(PARSE_STRING::flag);
  if (not token)
    return std::nullopt;
  return std::get<TOK_STRING>(token.value());
}

bool ParseDriver::parse() {
  std::optional<KEYWORD_KIND> beginning{};
  std::optional<std::monostate> declaration{};

  while (1) {
    beginning = parse_beginning(PARSE_STATEMENT{});
    if (not beginning)
      goto fail;

    program.emplace_back(STMT_VARDECL{.category = beginning.value()});
    declaration = parse(PARSE_VARDECL{}, program.size() - 1);
    if (not declaration)
      goto fail;
  }

  return 1;

fail:
  if (not errcode)
    throw std::runtime_error{"parsing failed without an errcode!"};
  return 0;
}
} // namespace Manadrain
