#include <unicode/uchar.h>
#include <unicode/ustring.h>

#include <format>
#include <optional>

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
static std::string_view codepoint_cv(char32_t ch,
                                     std::array<char, 4>& cp_storage) {
  std::uint32_t len{}, err{};
  cp_storage = {};
  U8_APPEND(cp_storage.data(), len, cp_storage.size(), ch, err);
  if (err)
    return {};
  return std::string_view{cp_storage.data(), len};
}
static bool is_lineterm(char32_t ch) {
  return ch == '\r' || ch == '\n' || ch == 0x2028 || ch == 0x2029;
}

namespace Manadrain {
static const std::array reserved_arr =
    std::to_array<std::tuple<std::string_view, TOKEN_TYPE, STRICTNESS>>(
        {{"var", TOKEN_TYPE::T_VAR, STRICTNESS::SLOPPY},
         {"const", TOKEN_TYPE::T_CONST, STRICTNESS::SLOPPY},
         {"let", TOKEN_TYPE::T_LET, STRICTNESS::STRICT}});

bool TOKEN::is_pseudo_keyword(TOKEN_TYPE tok_type) {
  if (ident.has_escape)
    return 0;
  if (ident.atom_idx >= reserved_arr.size())
    return 0;
  if (std::get<1>(reserved_arr[ident.atom_idx]) == tok_type)
    return 1;
  return 0;
}

bool TOKEN::is_vardecl_intro() {
  switch (type) {
    case TOKEN_TYPE::T_IDENT:
      if (is_pseudo_keyword(TOKEN_TYPE::T_LET)) {
        type = TOKEN_TYPE::T_LET;
        return 1;
      }
      return 0;
    case TOKEN_TYPE::T_CONST:
    case TOKEN_TYPE::T_LET:
    case TOKEN_TYPE::T_VAR:
      return 1;
    default:
      return 0;
  }
}

std::optional<char32_t> ParseDriver::peek() {
  if (buffer_idx >= buffer.size())
    return std::nullopt;
  UChar32 ch;
  U8_GET(buffer.data(), 0, buffer_idx, buffer.size(), ch);
  if (ch < 0)
    return std::nullopt;
  return ch;
}

std::optional<char32_t> ParseDriver::shift() {
  if (buffer_idx >= buffer.size())
    return std::nullopt;
  UChar32 ch;
  U8_NEXT(buffer.data(), buffer_idx, buffer.size(), ch);
  if (ch < 0)
    return std::nullopt;
  return ch;
}

void ParseDriver::drop(std::uint32_t count) {
  U8_FWD_N(buffer.data(), buffer_idx, buffer.size(), count);
}

std::size_t ParseDriver::obtain_atom() {
  if (not atom_umap.contains(ch_temp)) {
    atom_deq.push_back(std::move(ch_temp));
    atom_umap[atom_deq.back()] = reserved_arr.size() + atom_deq.size() - 1;
  }
  return atom_umap[atom_deq.back()];
}

bool ParseDriver::parse() {
  return 1;
}
}  // namespace Manadrain
