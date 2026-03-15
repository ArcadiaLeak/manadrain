#include <print>
#include <fstream>
#include <filesystem>
#include <string>
#include <ranges>
#include <vector>
#include <unordered_map>
#include <variant>
#include <functional>
#include <stdexcept>
#include <deque>
#include <limits>
#include <cassert>
#include <optional>
#include <cctype>
#include <array>

#include "enum/js_atom.hpp"
#include "enum/js_class.hpp"
#include "enum/js_token.hpp"

enum class JSAtomType {
  STRING = 1,
  GLOBAL_SYMBOL,
  SYMBOL,
  PRIVATE,
};

enum class JS_CFUNC {
  generic,
  generic_magic,
  constructor,
  constructor_magic,
  constructor_or_func,
  constructor_or_func_magic,
  f_f,
  f_f_f,
  getter,
  setter,
  getter_magic,
  setter_magic,
  iterator_next,
};

enum {
  OP_enter_scope
};

constexpr uint8_t JS_EVAL_TYPE_GLOBAL = 0;
constexpr uint8_t JS_EVAL_TYPE_MODULE = 1;
constexpr uint8_t JS_EVAL_TYPE_DIRECT = 2;
constexpr uint8_t JS_EVAL_TYPE_INDIRECT = 3;
constexpr uint8_t JS_EVAL_TYPE_MASK = 3;

constexpr uint8_t JS_EVAL_FLAG_STRICT = 1 << 3;

constexpr uint8_t JS_MODE_STRICT = 1 << 0;
constexpr uint8_t JS_MODE_ASYNC = 1 << 2;

constexpr uint8_t DECL_MASK_FUNC = 1 << 0;
constexpr uint8_t DECL_MASK_FUNC_WITH_LABEL = 1 << 1;
constexpr uint8_t DECL_MASK_OTHER = 1 << 2;
constexpr uint8_t DECL_MASK_ALL = DECL_MASK_FUNC |
  DECL_MASK_FUNC_WITH_LABEL | DECL_MASK_OTHER;

constexpr uint8_t UTF8_CHAR_LEN_MAX = 6;

constexpr int CP_LS = 0x2028;
constexpr int CP_PS = 0x2029;

struct JSHeapVal {
  int ref_count = 1;

  virtual ~JSHeapVal() = default;
  JSHeapVal(const JSHeapVal&) = default;
  JSHeapVal& operator=(const JSHeapVal&) = default;
  JSHeapVal(JSHeapVal&&) noexcept = default;
  JSHeapVal& operator=(JSHeapVal&&) noexcept = default;

  JSHeapVal() = default;
};

struct JSAtom : JSHeapVal {
  std::string str;
  JSAtom(std::string str): str{str} {}
};

struct JSContext : JSHeapVal {};

struct JSHeapHandle {
  size_t offset;
};

using JSValue = std::variant<
  int32_t, int64_t, double, JSHeapHandle
>;

struct JSTokStr {
  JSValue str;
  int sep;
};

struct JSTokNum {
  JSValue num;
};

struct JSTokIdent {
  std::shared_ptr<JSAtom> atom;
  bool has_escape;
  bool is_reserved;
};

struct JSTokRegexp {
  JSValue body;
  JSValue flags;
};

using JSTokenUnion = std::variant<
  JSTokStr, JSTokNum, JSTokIdent, JSTokRegexp
>;

struct JSToken {
  int val;
  size_t offset;
  JSTokenUnion u;
};

struct JSVarDef {
  std::shared_ptr<JSAtom> var_name;
};

struct JSVarScope {
  size_t parent;
  size_t first;
};

struct JSModuleDef;

struct JSFunctionDef {
  size_t ctx_handle;
  std::shared_ptr<JSFunctionDef> parent;

  bool is_eval;
  bool is_global_var;
  bool is_func_expr;

  int eval_type;
  bool new_target_allowed;
  bool super_call_allowed;
  bool super_allowed;
  bool arguments_allowed;
  uint8_t js_mode;

  size_t func_name;

  std::vector<JSVarDef> vars;
  size_t eval_ret_idx;

  size_t scope_level;
  size_t scope_first;
  std::vector<JSVarScope> scopes;
  size_t body_scope;

  std::deque<uint64_t> byte_code;
  size_t last_opcode_pos;

  std::shared_ptr<JSModuleDef> module_def;
};

struct JSClass {
  size_t class_id;
  size_t class_name;
};

struct JSRuntime {
  std::unordered_map<std::string, size_t> atom_hash;
  std::vector<std::shared_ptr<JSAtom>> atom_array;
  std::vector<JSClass> class_array;
  std::deque<std::unique_ptr<JSHeapVal>> gc_obj_list;

  template<size_t N>
  void init_class_range(
    std::array<int, N> tab, int start
  ) {
    for (size_t i = 0; i < N; i++) {
      class_array.emplace_back(
        JSClass{
          .class_id = i + start,
          .class_name = JS_DupAtom(*this, tab[i])
        }
      );
    }
  }

  size_t new_context();
  void init_atom_range();

  JSValue eval_internal(
    size_t ctx_handle,
    std::optional<JSValue> this_obj,
    std::string input,
    std::string filename,
    int flags,
    std::optional<size_t> scope_idx
  );
};

using JSSourceBuf = std::ranges::concat_view<
  std::ranges::owning_view<std::string>,
  std::ranges::repeat_view<char>
>;

bool lre_is_id_start_byte(int c) {
  return std::isalpha(c) || c == '$' || c == '_';
}

bool lre_is_id_continue_byte(int c) {
  return std::isalnum(c) || c == '$' || c == '_';
}

bool match_identifier(
  JSSourceBuf& buf, size_t idx,
  std::string rhs
) {
  std::string lhs = std::ranges::to<std::string>(
    std::ranges::subrange(
      std::next(buf.begin(), idx),
      std::next(buf.begin(), idx + rhs.size())
    )
  );
  
  if (lhs == rhs) return !lre_is_id_continue_byte(
    *std::next(buf.begin(), idx + rhs.size())
  );

  return false;
}

int from_hex(int c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  else if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  else if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  else
    return -1;
}

bool is_hi_surrogate(uint32_t c) {
  return (c >> 10) == (0xD800 >> 10); // 0xD800-0xDBFF
}

bool is_lo_surrogate(uint32_t c) {
  return (c >> 10) == (0xDC00 >> 10); // 0xDC00-0xDFFF
}

uint32_t from_surrogate(uint32_t hi, uint32_t lo) {
  return 0x10000 + 0x400 * (hi - 0xD800) + (lo - 0xDC00);
}

int lre_parse_escape(
  JSSourceBuf& buf, size_t& begin_idx,
  int allow_utf16
) {
  auto p = std::next(buf.begin(), begin_idx);
  uint32_t c = *p++;

  switch(c) {
    case 'b': c = '\b';
    break;

    case 'f': c = '\f';
    break;

    case 'n': c = '\n';
    break;

    case 'r': c = '\r';
    break;

    case 't': c = '\t';
    break;

    case 'v': c = '\v';
    break;

    case 'x': {
      int h0 = from_hex(*p++);
      if (h0 < 0)
        return -1;
      int h1 = from_hex(*p++);
      if (h1 < 0)
        return -1;
      c = (h0 << 4) | h1;
    }
    break;

    case 'u': {
      int h, i;

      if (*p == '{' && allow_utf16) {
        p++; c = 0;
        while (true) {
          h = from_hex(*p++);
          if (h < 0)
            return -1;
          c = (c << 4) | h;
          if (c > 0x10FFFF)
            return -1;
          if (*p == '}')
            break;
        }
        p++;
      } else {
        c = 0;
        for(i = 0; i < 4; i++) {
          h = from_hex(*p++);
          if (h < 0) {
            return -1;
          }
          c = (c << 4) | h;
        }
        if (
          is_hi_surrogate(c) && allow_utf16 == 2 &&
          p[0] == '\\' && p[1] == 'u'
        ) {
          uint32_t c1 = 0;
          for(i = 0; i < 4; i++) {
            h = from_hex(p[2 + i]);
            if (h < 0)
              break;
            c1 = (c1 << 4) | h;
          }
          if (i == 4 && is_lo_surrogate(c1)) {
            p += 6;
            c = from_surrogate(c, c1);
          }
        }
      }
    }
    break;

    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7':
    c -= '0';
    if (allow_utf16 == 2) {
      if (c != 0 || std::isdigit(*p))
        return -1;
    } else {
      uint32_t v = *p - '0';
      if (v > 7)
        break;
      c = (c << 3) | v;
      p++;
      if (c >= 32)
        break;
      v = *p - '0';
      if (v > 7)
        break;
      c = (c << 3) | v;
      p++;
    }
    break;

    default: return -2;
  }

  begin_idx = std::distance(buf.begin(), p);
  return c;
}

constexpr unsigned int utf8_min_code[5] = {
  0x80, 0x800, 0x10000, 0x00200000, 0x04000000,
};

constexpr unsigned char utf8_first_code_mask[5] = {
  0x1f, 0xf, 0x7, 0x3, 0x1,
};

int unicode_from_utf8(
  JSSourceBuf& buf, size_t begin_idx,
  int max_len, size_t& end_idx
) {
  auto p = std::next(buf.begin(), begin_idx);
  int l, c = *p++;
  if (c < 0x80) {
    end_idx = std::distance(buf.begin(), p);
    return c;
  }
  switch(c) {
    case 0xc0: case 0xc1: case 0xc2: case 0xc3:
    case 0xc4: case 0xc5: case 0xc6: case 0xc7:
    case 0xc8: case 0xc9: case 0xca: case 0xcb:
    case 0xcc: case 0xcd: case 0xce: case 0xcf:
    case 0xd0: case 0xd1: case 0xd2: case 0xd3:
    case 0xd4: case 0xd5: case 0xd6: case 0xd7:
    case 0xd8: case 0xd9: case 0xda: case 0xdb:
    case 0xdc: case 0xdd: case 0xde: case 0xdf:
    l = 1; break;

    case 0xe0: case 0xe1: case 0xe2: case 0xe3:
    case 0xe4: case 0xe5: case 0xe6: case 0xe7:
    case 0xe8: case 0xe9: case 0xea: case 0xeb:
    case 0xec: case 0xed: case 0xee: case 0xef:
    l = 2; break;

    case 0xf0: case 0xf1: case 0xf2: case 0xf3:
    case 0xf4: case 0xf5: case 0xf6: case 0xf7:
    l = 3; break;

    case 0xf8: case 0xf9: case 0xfa: case 0xfb:
    l = 4; break;

    case 0xfc: case 0xfd:
    l = 5; break;

    default: return -1;
  }
  if (l > (max_len - 1))
    return -1;
  c &= utf8_first_code_mask[l - 1];
  for (int i = 0; i < l; i++) {
    int b = *p++;
    if (b < 0x80 || b >= 0xc0)
      return -1;
    c = (c << 6) | (b & 0x3f);
  }
  if (c < utf8_min_code[l - 1])
    return -1;
  end_idx = std::distance(buf.begin(), p);
  return c;
}

int simple_next_token(
  JSSourceBuf& buf,
  size_t& begin_idx,
  bool no_line_feed
) {
  size_t idx = begin_idx;
  uint32_t ch = buf[idx];

  while (true) {
    ch = buf[idx++];
    switch (ch) {
      case '\r': case '\n':
      if (no_line_feed)
        return '\n';
      continue;
      
      case ' ': case '\t': case '\v': case '\f':
      continue;

      case '/':
      if (buf[idx] == '/') {
        if (no_line_feed)
          return '\n';
        auto cch = buf[idx];
        while (cch && cch != '\r' && cch != '\n')
          idx++;
        continue;
      }
      if (buf[idx] == '*') {
        while (buf[++idx]) {
          auto cch = buf[idx];
          if ((cch == '\r' || cch == '\n') && no_line_feed)
            return '\n';
          if (cch == '*' && buf[idx + 1] == '/')
            { idx += 2; break; }
        }
        continue;
      }
      break;

      case '=': if (buf[idx] == '>')
        return JS_TOK_ARROW;
      break;

      case 'i': if (match_identifier(buf, idx, "n"))
        return JS_TOK_IN;
      if (match_identifier(buf, idx, "mport")) {
        begin_idx = idx + 5;
        return JS_TOK_IMPORT;
      }
      return JS_TOK_IDENT;

      case 'o': if (match_identifier(buf, idx, "f"))
        return JS_TOK_OF;
      return JS_TOK_IDENT;

      case 'e': if (match_identifier(buf, idx, "xport"))
        return JS_TOK_EXPORT;
      return JS_TOK_IDENT;

      case 'f': if (match_identifier(buf, idx, "unction"))
        return JS_TOK_FUNCTION;
      return JS_TOK_IDENT;
      
      case '\\': if (buf[idx] == 'u') {
        if (lre_is_id_start_byte(lre_parse_escape(buf, idx, true)))
          return JS_TOK_IDENT;
      }
      break;

      default: if (ch >= 128) {
        ch = unicode_from_utf8(buf, idx - 1, UTF8_CHAR_LEN_MAX, idx);
        if (no_line_feed && (ch == CP_PS || ch == CP_LS))
          return '\n';
      }
      if (std::isspace(ch))
        continue;
      if (lre_is_id_start_byte(ch))
        return JS_TOK_IDENT;
      break;
    }
    return ch;
  }
}

struct JSParseState {
  size_t ctx_handle;
  std::string filename;
  JSToken token;
  bool got_line_feed;

  JSSourceBuf buf;
  size_t last_idx;
  size_t curr_idx;
  size_t buf_size;

  std::unique_ptr<JSFunctionDef> cur_func;
  bool is_module;

  size_t push_scope() {
    if (!cur_func)
      return 0;

    JSFunctionDef& fd = *cur_func;
    JSVarScope scope{
      .parent = fd.scope_level,
      .first = fd.scope_first
    };
    uint64_t scope_idx = fd.scopes.size();
    fd.scopes.emplace_back(scope);
    fd.scope_level = scope_idx;
    emit_op(OP_enter_scope);
    emit_u32(scope_idx);
    return scope_idx;
  }

  void emit_op(uint64_t val) {
    cur_func->last_opcode_pos = cur_func->byte_code.size();
    cur_func->byte_code.push_back(val);
  }

  void emit_u32(uint64_t val) {
    cur_func->byte_code.push_back(val);
  }

  int parse_program(JSRuntime& rt) {
    if (next_token())
      return -1;

    while (token.val != JS_TOK_EOF) {
      if (parse_source_element(rt))
        return -1;
    }

    return 0;
  }

  int parse_source_element(JSRuntime& rt) {
    if (token.val == JS_TOK_FUNCTION || token_is_async_func(rt))
      return -1;
    else if (cur_func->module_def && token.val == JS_TOK_EXPORT)
      return -1;
    else if (cur_func->module_def && token_is_static_import())
      return -1;
    else
      return parse_statement_or_decl(DECL_MASK_ALL);
  }

  int parse_statement_or_decl(int decl_mask) {
    return -1;
  }

  bool token_is_static_import() {
    if (token.val != JS_TOK_IMPORT)
      return false;
    int tok = peek_token(false);
    return tok != '(' && tok != '.';
  }

  bool token_is_async_func(JSRuntime& rt) {
    return (
      token_is_pseudo_keyword(rt.atom_array[JS_ATOM_async]) &&
      peek_token(true) == JS_TOK_FUNCTION
    );
  }

  bool token_is_pseudo_keyword(std::shared_ptr<JSAtom> atom) {
    return token.val == JS_TOK_IDENT &&
      std::get<JSTokIdent>(token.u).atom == atom &&
      std::get<JSTokIdent>(token.u).has_escape;
  }

  int peek_token(bool no_line_feed) {
    return simple_next_token(
      buf, curr_idx, no_line_feed
    );
  }

  int parse_string(
    int sep, bool do_throw, size_t idx,
    JSToken& token, size_t& idxref
  ) {
    auto ch = buf[idx];
    auto strbuf = std::string{""};

    while (true) {
      if (idx >= buf_size)
        goto invalid_char;

      ch = buf[idx]; idx++;
      if (ch == sep) break;

      if (ch == '$' && buf[idx] == '{' && sep == '`')
        { idx++; break; }
      
      if (ch == '\\') {
        size_t idx_escape = idx - 1;
        ch = buf[idx];

        switch (ch) {
          case '\0': if (idx >= buf_size)
            goto invalid_char;
          idx++; break;

          case '\'': case '\"': case '\\':
          idx++; break;

          case '\r':
          if (buf[idx + 1] == '\n')
            idx++;
          [[fallthrough]];

          case '\n': idx++;
          continue;

          default: break;
        }
      }

      strbuf.push_back(ch);
    }

    invalid_char: assert(0);
  }

  int parse_error(size_t offset, std::string message) {
    assert(0);
  }

  int next_token() {
    auto idx = curr_idx;
    auto ch = buf[idx];

    last_idx = curr_idx;
    got_line_feed = false;

    redo: token.offset = curr_idx;
    ch = buf[idx];

    switch (ch) {
      case 0:
      if (idx >= buf_size)
        token.val = JS_TOK_EOF;
      else
        goto def_token;
      break;

      case '\r':
      if (buf[idx + 1] == '\n')
        idx++;
      [[fallthrough]];

      case '\n': idx++;
      line_feed: got_line_feed = true;
      goto redo;

      case '\f': case '\v': case ' ': case '\t': idx++;
      goto redo;

      case '\'': case '\"':
      if (parse_string(ch, true, idx + 1, token, idx))
        goto fail;
      break;

      default: def_token: token.val = ch;
      idx++; break;
    }

    curr_idx = idx;
    return 0;

    fail: token.val = JS_TOK_ERROR;
    return -1;
  }
};

void JSRuntime::init_atom_range() {
  using namespace std::views;
  for (auto [i, str_view] : enumerate(js_atom_init)) {
    JSAtomType atom_type;
    if (i == JS_ATOM_Private_brand)
      atom_type = JSAtomType::PRIVATE;
    else if (i >= JS_ATOM_Symbol_toPrimitive)
      atom_type = JSAtomType::SYMBOL;
    else
      atom_type = JSAtomType::STRING;
    
    if (atom_type == JSAtomType::STRING) {
      std::string str{str_view};
      atom_array.emplace_back(std::make_shared<JSAtom>(str));
      atom_hash[str] = i;
    }
  }
}

size_t JSRuntime::new_context() {
  gc_obj_list.emplace_back(
    std::make_unique<JSContext>()
  );
  return gc_obj_list.size() - 1;
}

constexpr bool JS_AtomIsConst(size_t idx) {
  return idx > JS_ATOM_END;
}

size_t JS_DupAtom(JSRuntime& rt, size_t idx) {
  if (!JS_AtomIsConst(idx))
    rt.atom_array[idx]->ref_count++;
  return idx;
}

JSValue JSRuntime::eval_internal(
  size_t ctx_handle,
  std::optional<JSValue> this_obj,
  std::string input,
  std::string filename,
  int flags,
  std::optional<size_t> scope_idx
) {
  uint8_t js_mode = 0;

  JSParseState state{
    .ctx_handle = ctx_handle,
    .filename = filename,
    .buf = std::ranges::concat_view{
      std::ranges::owning_view{std::string{input}},
      std::ranges::repeat_view{'\0'}
    },
    .buf_size = input.size()
  };

  int eval_type = flags & JS_EVAL_TYPE_MASK;
  if (eval_type == JS_EVAL_TYPE_DIRECT) {
    throw std::runtime_error("unimplemented!");
  } else {
    if (flags & JS_EVAL_FLAG_STRICT)
      js_mode |= JS_MODE_STRICT;
  }
  state.cur_func = std::make_unique<JSFunctionDef>(
    JSFunctionDef{
      .ctx_handle = ctx_handle,
      .parent = nullptr,
      .is_eval = true,
      .is_func_expr = false
    }
  );
  JSFunctionDef& func_def = *state.cur_func;
  func_def.eval_type = eval_type;
  if (eval_type == JS_EVAL_TYPE_DIRECT) {
    throw std::runtime_error("unimplemented!");
  } else {
    func_def.new_target_allowed = false;
    func_def.super_call_allowed = false;
    func_def.super_allowed = false;
    func_def.arguments_allowed = true;
  }
  func_def.js_mode = js_mode;
  func_def.func_name = JS_DupAtom(*this, JS_ATOM__eval_);
  state.is_module = false;
  state.push_scope();
  func_def.body_scope = func_def.scope_level;

  state.parse_program(*this);
  assert(0);
}

std::string source_str(std::string filepath) {
  std::ifstream file{filepath};
  if (!file.is_open()) throw std::runtime_error{
    std::format("could not open file: {}", filepath)
  };

  return std::ranges::to<std::string>(
    std::ranges::subrange(
      std::istreambuf_iterator<char>{file},
      std::istreambuf_iterator<char>{}
    )
  );
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::println(stderr, "Usage: {} <filepath>", argv[0]);
    return 1;
  }

  std::string filepath = argv[1];
  if (!std::filesystem::exists(filepath)) {
    std::println(stderr, "Error: file does not exist: {}", filepath);
    return 1;
  }

  std::shared_ptr<JSRuntime> rt = std::make_shared<JSRuntime>();
  rt->init_atom_range();
  rt->init_class_range(js_std_class_def, JS_CLASS_OBJECT);

  size_t ctx_handle = rt->new_context();
  rt->eval_internal(
    ctx_handle, {}, source_str(filepath), "", 0, {}
  );

  return 0;
}
