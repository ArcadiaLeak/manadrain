#include <print>
#include <fstream>
#include <filesystem>
#include <string>
#include <ranges>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <functional>
#include <span>
#include <stdexcept>
#include <deque>
#include <limits>
#include <cassert>
#include <optional>

#include "js_atom_enum.hpp"
#include "js_class_enum.hpp"
#include "js_token_enum.hpp"

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

struct JSHeapAny {
  virtual ~JSHeapAny() = default;
  JSHeapAny(const JSHeapAny&) = default;
  JSHeapAny& operator=(const JSHeapAny&) = default;
  JSHeapAny(JSHeapAny&&) noexcept = default;
  JSHeapAny& operator=(JSHeapAny&&) noexcept = default;

  JSHeapAny() = default;
};

struct JSAtom {
  std::shared_ptr<char[]> str;
  size_t len;
};

using JSValue = std::variant<int32_t, int64_t, double, std::shared_ptr<JSHeapAny>>;

struct JSTokStr {
  JSValue str;
  int sep;
};

struct JSTokNum {
  JSValue num;
};

struct JSTokIdent {
  std::shared_ptr<char[]> str;
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
using JSCFunction = std::function<JSValue(
  std::shared_ptr<struct JSContext> ctx,
  JSValue this_val,
  int argc,
  std::shared_ptr<JSValue[]> argv
)>;

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

struct JSFunctionDef {
  std::shared_ptr<JSContext> ctx;
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

  std::shared_ptr<JSAtom> func_name;

  std::vector<JSVarDef> vars;
  size_t eval_ret_idx;

  size_t scope_level;
  size_t scope_first;
  std::vector<JSVarScope> scopes;
  size_t body_scope;

  std::deque<uint64_t> byte_code;
  size_t last_opcode_pos;
};

struct JSRuntime {
  std::unordered_map<std::string, size_t> atom_hash;
  std::vector<std::shared_ptr<JSAtom>> atom_vec;

  void insert_wellknown() {
    for (int i = 0; i < JS_ATOM_END; i++) {
      JSAtomType atom_type;
      if (i == JS_ATOM_Private_brand)
        atom_type = JSAtomType::PRIVATE;
      else if (i >= JS_ATOM_Symbol_toPrimitive)
        atom_type = JSAtomType::SYMBOL;
      else
        atom_type = JSAtomType::STRING;
      
      if (atom_type == JSAtomType::STRING) {
        std::string str{js_atom_init[i]};
        std::shared_ptr<JSAtom> js_str = std::make_shared<JSAtom>();
        std::shared_ptr<char[]> shared_str = std::make_shared<char[]>(str.size());
        std::copy(str.begin(), str.end(), shared_str.get());
        js_str->str = shared_str;
        js_str->len = str.size();
        atom_vec.push_back(js_str);
        atom_hash[str] = i;
      }
    }
  }
};

auto inf_range_from(
  std::shared_ptr<char[]> buffer,
  size_t buffer_len,
  char pad
) {
  return std::ranges::concat_view{
    std::span<char>{buffer.get(), buffer_len},
    std::ranges::repeat_view{pad}
  };
}

int simple_next_token(
  size_t& begin_idx,
  std::shared_ptr<char[]> buf,
  size_t buf_size,
  bool no_line_feed
) {
  auto infbuf = inf_range_from(buf, buf_size, '\0');
  size_t idx = begin_idx;
  uint32_t ch = infbuf[idx];

  while (true) {
    ch = infbuf[idx++];
    switch (ch) {
      case '\r': case '\n':
      if (no_line_feed)
        return '\n';
      continue;
      
      case ' ': case '\t': case '\v': case '\f':
      continue;

      case '/':
      if (infbuf[idx] == '/') {
        if (no_line_feed)
          return '\n';
        auto cch = infbuf[idx];
        while (cch && cch != '\r' && cch != '\n')
          idx++;
        continue;
      }
      if (infbuf[idx] == '*') {
        while (infbuf[++idx]) {
          auto cch = infbuf[idx];
          if ((cch == '\r' || cch == '\n') && no_line_feed)
            return '\n';
          if (cch == '*' && infbuf[idx + 1] == '/')
            { idx += 2; break; }
        }
        continue;
      }
      break;

      case '=': if (infbuf[idx] == '>')
        return JS_TOK_ARROW;
      break;

      
    }
  }
}

struct JSParseState {
  std::shared_ptr<struct JSContext> ctx;
  std::string filename;
  JSToken token;
  bool got_line_feed;

  std::shared_ptr<char[]> buf;
  size_t last_idx;
  size_t curr_idx;
  size_t buf_size;

  std::shared_ptr<JSFunctionDef> cur_func;
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

  void parse_program() {
    next_token();

    while (token.val != JS_TOK_EOF)
      parse_source_element();
  }

  void parse_source_element() {

  }

  int peek_token(bool no_line_feed) {
    return simple_next_token(
      curr_idx, buf, buf_size, no_line_feed
    );
  }

  void parse_string(
    int sep, bool do_throw, size_t idx,
    JSToken& token, size_t& idxref
  ) {
    auto infbuf = inf_range_from(buf, buf_size, '\0');
    auto ch = infbuf[idx];
    auto strbuf = std::string{""};

    while (true) {
      if (idx >= buf_size)
        goto invalid_char;

      ch = infbuf[idx]; idx++;
      if (ch == sep) break;

      if (ch == '$' && infbuf[idx] == '{' && sep == '`')
        { idx++; break; }
      
      if (ch == '\\') {
        size_t idx_escape = idx - 1;
        ch = infbuf[idx];

        switch (ch) {
          case '\0': if (idx >= buf_size)
            goto invalid_char;
          idx++; break;

          case '\'': case '\"': case '\\':
          idx++; break;

          case '\r':
          if (infbuf[idx + 1] == '\n')
            idx++;
          [[fallthrough]];

          case '\n': idx++;
          continue;

          default: break;
        }
      }

      strbuf.push_back(ch);
    }

    invalid_char: if (do_throw)
      throw std::runtime_error{"unexpected end of string"};
  }

  void next_token() {
    auto infbuf = inf_range_from(buf, buf_size, '\0');
    auto idx = curr_idx;
    auto ch = infbuf[idx];

    last_idx = curr_idx;
    got_line_feed = false;

    redo: token.offset = curr_idx;
    ch = infbuf[idx];

    switch (ch) {
      case 0:
      if (idx >= buf_size)
        token.val = JS_TOK_EOF;
      else
        goto def_token;
      break;

      case '\r':
      if (infbuf[idx + 1] == '\n')
        idx++;
      [[fallthrough]];

      case '\n': idx++;
      line_feed: got_line_feed = true;
      goto redo;

      case '\f': case '\v': case ' ': case '\t': idx++;
      goto redo;

      case '\'': case '\"':
      parse_string(ch, true, idx + 1, token, idx);
      break;

      default: def_token: token.val = ch;
      idx++; break;
    }

    curr_idx = idx;
  }
};

struct JSStackFrame {};
struct JSFunctionBytecode {};
struct JSVarRef {};

struct JSContext {
  std::shared_ptr<JSRuntime> rt;
};

JSFunctionDef js_new_function_def(
  std::shared_ptr<JSContext> ctx,
  std::shared_ptr<JSFunctionDef> parent,
  bool is_eval,
  bool is_func_expr,
  std::string filename,
  size_t source_pos
) {
  return JSFunctionDef{
    .ctx = ctx,
    .parent = parent,
    .is_eval = is_eval,
    .is_func_expr = is_func_expr
  };
}

JSValue JS_EvalInternal(
  std::shared_ptr<JSContext> ctx,
  JSValue this_obj,
  std::shared_ptr<char[]> input,
  size_t input_len,
  std::string filename,
  int flags,
  std::optional<size_t> scope_idx
) {
  uint8_t js_mode = 0;

  JSParseState state{
    .ctx = ctx,
    .filename = filename,
    .buf = input,
    .buf_size = input_len
  };

  int eval_type = flags & JS_EVAL_TYPE_MASK;
  if (eval_type == JS_EVAL_TYPE_DIRECT) {
    throw std::runtime_error("unimplemented!");
  } else {
    if (flags & JS_EVAL_FLAG_STRICT)
      js_mode |= JS_MODE_STRICT;
  }
  state.cur_func = std::make_shared<JSFunctionDef>(
    js_new_function_def(ctx, nullptr, true, false, filename, 0)
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
  func_def.func_name = ctx->rt->atom_vec[JS_ATOM__eval_];
  state.is_module = false;
  state.push_scope();
  func_def.body_scope = func_def.scope_level;

  state.parse_program();

  assert(0);
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::println(stderr, "Usage: {} <filepath>", argv[0]);
    return 1;
  }

  std::string filePath = argv[1];

  if (!std::filesystem::exists(filePath)) {
    std::println(stderr, "Error: file does not exist: {}", filePath);
    return 1;
  }

  std::ifstream file(filePath);
  if (!file.is_open()) {
    std::println(stderr, "Error: could not open file: {}", filePath);
    return 1;
  }

  std::string source_str = std::ranges::to<std::string>(
    std::views::istream<char>(file >> std::noskipws)
  );
  std::shared_ptr<char[]> source_buf = std::make_shared<char[]>(source_str.size());
  std::copy(source_str.begin(), source_str.end(), source_buf.get());

  std::shared_ptr<JSRuntime> rt = std::make_shared<JSRuntime>();
  rt->insert_wellknown();

  std::shared_ptr<JSContext> ctx = std::make_shared<JSContext>();
  ctx->rt = rt;

  JS_EvalInternal(ctx, nullptr, source_buf, source_str.size(), "", 0, {});

  return 0;
}
