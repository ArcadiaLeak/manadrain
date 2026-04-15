#include <limits>
#include <stdexcept>

#include <unicode/uchar.h>
#include <unicode/ustring.h>

#include "manadrain.hpp"

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

static bool lineterm(char32_t ch) {
  return ch == '\r' || ch == '\n' || ch == 0x2028 || ch == 0x2029;
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

std::variant<std::monostate, PARSE_ERRCODE, char32_t>
ParseDriver::parse(PARSE_ESCAPE esc, char32_t ch) {
  switch (ch) {
  case 'b':
    return U'\b';
  case 'f':
    return U'\f';
  case 'n':
    return U'\n';
  case 'r':
    return U'\r';
  case 't':
    return U'\t';
  case 'v':
    return U'\v';
  case '0':
    if (reached_eof() || !std::isdigit(next()))
      return U'\0';
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
      return PARSE_ERRCODE::LEGACY_OCTAL_SEQ;

    case ESC_RULE::STRING_IN_TEMPLATE:
    case ESC_RULE::REGEXP_UTF16:
      return PARSE_ERRCODE::MALFORMED_ESCAPE;

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
        esc.rule == ESC_RULE::STRING_IN_TEMPLATE)
      return PARSE_ERRCODE::MALFORMED_ESCAPE;
    [[fallthrough]];
  default:
    return std::monostate{};
  }
}

std::variant<std::monostate, PARSE_ERRCODE, char32_t>
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
    return std::monostate{};
  }
  ESC_RULE esc_rule = ESC_RULE::STRING_IN_SLOPPY_MODE;
  if (strictness == STRICTNESS::STRICT)
    esc_rule = ESC_RULE::STRING_IN_STRICT_MODE;
  else if (separator == '`')
    esc_rule = ESC_RULE::STRING_IN_TEMPLATE;
  return parse(PARSE_ESCAPE{esc_rule}, ch);
}

std::variant<std::monostate, PARSE_ERRCODE, char32_t>
ParseDriver::parse_uchar(PARSE_STRING, char32_t separator, char32_t ch) {
  switch (ch) {
  case '\r':
    skip_lf();
    [[fallthrough]];
  case '\n':
    if (separator == '`')
      return U'\n';
    return PARSE_ERRCODE::UNEXPECTED_STRING_END;
  case '\\':
    if (reached_eof())
      return std::monostate{};
    return parse_escape(PARSE_STRING{}, separator, next());
  default:
    return ch;
  }
}

std::variant<std::monostate, PARSE_ERRCODE>
ParseDriver::parse_atom(PARSE_STRING, char32_t separator) {
  str1_temp.clear();
  while (1) {
    if (reached_eof())
      return PARSE_ERRCODE::UNEXPECTED_STRING_END;
    auto ch_alt = parse_uchar(PARSE_STRING{}, separator, next());

    switch (ch_alt.index()) {
    case 2: {
      char32_t ch = std::get<2>(ch_alt);
      if (ch == separator)
        return std::monostate{};
      str1_encode(ch);
      break;
    }
    case 1:
      return std::get<1>(ch_alt);
    case 0:
      continue;
    }
  }
}

bool ParseDriver::is_allowed_uchar(PARSE_IDENT ident, char32_t ch) {
  UProperty must_be = str1_temp.size() > ident.is_private ? UCHAR_XID_CONTINUE
                                                          : UCHAR_XID_START;
  return u_hasBinaryProperty(ch, must_be);
}

std::variant<bool, PARSE_ERRCODE> ParseDriver::parse_uchar(PARSE_IDENT ident) {
  std::array<char32_t, 2> ch{};
  int i;
  for (i = 0; i < 2; ++i) {
    if (reached_eof())
      break;
    ch[i] = next();
  }
  if (ch[0] == '\\') {
    if (ch[1] == 'u')
      throw std::runtime_error{"unimplemented!"};
    return PARSE_ERRCODE::MALFORMED_ESCAPE;
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

std::variant<bool, PARSE_ERRCODE> ParseDriver::parse_atom(PARSE_IDENT ident) {
  str1_temp.clear();
  if (ident.is_private)
    str1_temp.push_back('#');

  bool further = 1;
  while (further) {
    auto further_alt = parse_uchar(ident);
    switch (further_alt.index()) {
    case 0:
      further = std::get<0>(further_alt);
      break;
    case 1:
      return std::get<1>(further_alt);
    }
  }
  return str1_temp.size() > ident.is_private;
}

bool ParseDriver::skip_comment_line() {
  if (reached_eof())
    return 0;
  if (lineterm(next())) {
    prev();
    return 0;
  }
  return 1;
}

std::variant<bool, PARSE_ERRCODE> ParseDriver::skip_comment_block() {
  std::array<char32_t, 2> ch{};
  int i;
  for (i = 0; i < 2; ++i) {
    if (reached_eof())
      break;
    ch[i] = next();
  }
  if (i == 0)
    return PARSE_ERRCODE::UNEXPECTED_COMMENT_END;
  if (ch[0] == '*' && ch[1] == '/')
    return false;
  backtrack(--i);
  if (lineterm(ch[0]))
    newline_seen = 1;
  return true;
}

std::variant<bool, PARSE_ERRCODE> ParseDriver::skip_comment(char32_t ch) {
  switch (ch) {
  case '/':
    while (1) {
      if (not skip_comment_line())
        return true;
    }
  case '*':
    while (1) {
      auto comment = skip_comment_block();
      switch (comment.index()) {
      case 0:
        if (not std::get<0>(comment))
          return true;
        continue;
      case 1:
        return std::get<1>(comment);
      }
    }
  default:
    prev();
    return false;
  }
}

std::variant<bool, PARSE_ERRCODE> ParseDriver::skip_ws_1(char32_t ch) {
  switch (ch) {
  case '\r':
    skip_lf();
    [[fallthrough]];

  case '\n':
  case 0x2028:
  case 0x2029:
    newline_seen = 1;
    return true;

  case '/': {
    if (reached_eof())
      return true;
    auto comment_alt = skip_comment(next());
    if (comment_alt.index() == 0 && !std::get<0>(comment_alt))
      prev();
    return comment_alt;
  }

  default: {
    bool ws = u_isWhitespace(ch);
    if (not ws)
      prev();
    return ws;
  }
  }
}

TOKEN ParseDriver::tokenize() {
whitespace: {
  if (reached_eof())
    return std::monostate{};
  auto ws_alt = skip_ws_1(next());
  switch (ws_alt.index()) {
  case 0:
    if (std::get<0>(ws_alt))
      goto whitespace;
    break;
  case 1:
    return std::get<1>(ws_alt);
  }
}

  {
    char32_t ch{next()};
    if (ch == '\'' || ch == '"') {
      auto atom_alt = parse_atom(PARSE_STRING{}, ch);
      switch (atom_alt.index()) {
      case 0: {
        P_ATOM pos_atom = find_static_atom()
                              .or_else([this]() { return find_dynamic_atom(); })
                              .value();
        return TOK_STRING{.separator = ch, .p_atom = pos_atom};
      }
      case 1:
        return std::get<1>(atom_alt);
      }
      throw std::runtime_error{"unreachable!"};
    }
    prev();
  }

  {
    auto atom_alt = parse_atom(PARSE_IDENT{});
    bool have_ident{};
    switch (atom_alt.index()) {
    case 0:
      have_ident = std::get<0>(atom_alt);
      break;
    case 1:
      return std::get<1>(atom_alt);
    }
    if (have_ident) {
      P_ATOM pos_atom = find_static_atom()
                            .or_else([this]() { return find_dynamic_atom(); })
                            .value();
      return TOK_IDENTI{.p_atom = pos_atom};
    }
  }

  return next();
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

std::variant<std::monostate, PARSE_ERRCODE>
ParseDriver::parse(PARSE_VARDECL, std::size_t idx) {
  bool may_have_init{1};

  token_curr = tokenize();
  if (token_curr.index() != TOKV_IDENTI)
    return PARSE_ERRCODE::NEEDED_VARIABLE_NAME;
  std::get<STMT_VARDECL>(program[idx]).identifier =
      std::get<TOK_IDENTI>(token_curr);
  token_curr = tokenize();

expect_semi:
  if (may_have_init)
    goto check_initializer;
  else if (token_curr.index() == TOKV_EOF || token_curr == TOKEN{U';'})
    return std::monostate{};
  else
    return PARSE_ERRCODE::NEEDED_SEMICOLON;

check_initializer:
  if (token_curr == TOKEN{U'='}) {
    token_curr = tokenize();
    auto postfix_expr = parse(PARSE_POSTFIX_EXPR{});
    if (postfix_expr.index() == 1)
      return std::get<1>(postfix_expr);
    std::get<STMT_VARDECL>(program[idx]).initializer =
        std::move(std::get<0>(postfix_expr));
  }
  may_have_init = 0;
  goto expect_semi;
}

std::variant<EXPRESSION, PARSE_ERRCODE> ParseDriver::parse(PARSE_POSTFIX_EXPR) {
  EXPRESSION expr{};
  while (1) {
    switch (token_curr.index()) {
    case TOKV_STRING:
      expr = std::get<TOKV_STRING>(token_curr);
      break;
    case TOKV_IDENTI:
      expr = std::get<TOKV_IDENTI>(token_curr);
      break;
    default:
      return PARSE_ERRCODE::UNEXPECTED_TOKEN;
    }
    token_curr = tokenize();
    break;
  }
  return expr;
}

bool ParseDriver::parse() {
  token_curr = tokenize();
  while (1) {
    std::optional<PARSE_ERRCODE> errcode{};

    switch (token_curr.index()) {
    case TOKV_EOF:
      return 1;
    case TOKV_ERROR:
      errcode = std::get<TOKV_ERROR>(token_curr);
      goto fail;
    case TOKV_IDENTI:
      if (std::get<TOKV_IDENTI>(token_curr).match_keyword(STRICTNESS::STRICT))
        goto parse_vardecl;
      [[fallthrough]];
    default:
      goto parse_postfix;
    }

  fail:
    if (not errcode)
      throw std::runtime_error{"parsing failed without an errcode!"};
    return 0;

  parse_vardecl: {
    program.emplace_back(
        STMT_VARDECL{.rule = std::get<TOKV_IDENTI>(token_curr)
                                 .match_keyword(STRICTNESS::STRICT)
                                 .value()});
    auto declaration = parse(PARSE_VARDECL{}, program.size() - 1);
    if (declaration.index() == 0) {
      token_curr = tokenize();
      continue;
    }
    errcode = std::get<1>(declaration);
    goto fail;
  }

  parse_postfix: {
    auto postfix_expr = parse(PARSE_POSTFIX_EXPR{});
    switch (postfix_expr.index()) {
    case 0:
      program.emplace_back(std::move(std::get<0>(postfix_expr)));
      continue;
    case 1:
      errcode = std::get<1>(postfix_expr);
      goto fail;
    }
  }
  }
}
} // namespace Manadrain
