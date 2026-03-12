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

constexpr int JS_EVAL_TYPE_GLOBAL = 0;
constexpr int JS_EVAL_TYPE_MODULE = 1;
constexpr int JS_EVAL_TYPE_DIRECT = 2;
constexpr int JS_EVAL_TYPE_INDIRECT = 3;
constexpr int JS_EVAL_TYPE_MASK = 3;

constexpr int JS_EVAL_FLAG_STRICT = 1 << 3;

constexpr int JS_MODE_STRICT = 1 << 0;
constexpr int JS_MODE_ASYNC = 1 << 2;

struct JSHeapAny {
  virtual ~JSHeapAny() = default;
  JSHeapAny(const JSHeapAny&) = default;
  JSHeapAny& operator=(const JSHeapAny&) = default;
  JSHeapAny(JSHeapAny&&) noexcept = default;
  JSHeapAny& operator=(JSHeapAny&&) noexcept = default;

  JSHeapAny() = default;
};

struct JSAtom : JSHeapAny {
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

using JSTokenVal = std::variant<
  JSTokStr, JSTokNum, JSTokIdent, JSTokRegexp, uint32_t
>;
using JSCFunction = std::function<JSValue(
  std::shared_ptr<struct JSContext> ctx,
  JSValue this_val,
  int argc,
  std::shared_ptr<JSValue[]> argv
)>;

struct JSToken {
  size_t offset;
  JSTokenVal val;
};

enum class JSEvalType {
  GLOBAL, MODULE, DIRECT, INDIRECT
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

  JSEvalType eval_type;
  uint8_t js_mode;

  std::shared_ptr<JSAtom> func_name;

  std::vector<JSVarDef> vars;
  size_t eval_ret_idx;

  size_t scope_level;
  size_t scope_first;
  std::vector<JSVarScope> scopes;

  std::deque<uint8_t> byte_code;
};

struct JSRuntime {
  std::unordered_map<std::string, std::shared_ptr<JSAtom>> str_hash;
  std::unordered_set<std::shared_ptr<JSHeapAny>> atom_hash;

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
        str_hash[str] = js_str;
        atom_hash.insert(js_str);
      }
    }
  }
};

struct JSParseState {
  std::shared_ptr<struct JSContext> ctx;
  std::shared_ptr<char[]> filename;
  JSToken token;

  std::shared_ptr<char[]> buf;
  size_t buf_prev;
  size_t buf_curr;
  size_t buf_size;
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
  std::shared_ptr<char[]> filename,
  std::shared_ptr<char[]> source_ptr
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
  std::shared_ptr<char[]> filename,
  int flags,
  int scope_idx
) {
  std::shared_ptr<JSParseState> state =
    std::make_shared<JSParseState>(
      JSParseState{
        .ctx = ctx,
        .filename = filename,
        .buf = input,
        .buf_size = input_len
      }
    );
  std::shared_ptr<JSStackFrame> stack_frame{};
  std::shared_ptr<JSFunctionBytecode> bytecode{};
  std::shared_ptr<std::shared_ptr<JSVarRef>[]> var_ref_array{};
  int js_mode = 0;

  int eval_type = flags & JS_EVAL_TYPE_MASK;
  if (eval_type == JS_EVAL_TYPE_DIRECT) {
    throw std::runtime_error("unimplemented!");
  } else {
    if (flags & JS_EVAL_FLAG_STRICT)
      js_mode |= JS_MODE_STRICT;
  }

  throw std::runtime_error("unimplemented!");
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

  std::string source = std::ranges::to<std::string>(
    std::views::istream<char>(file >> std::noskipws)
  );

  std::shared_ptr<JSRuntime> rt = std::make_shared<JSRuntime>();
  rt->insert_wellknown();

  std::shared_ptr<JSContext> ctx = std::make_shared<JSContext>();
  ctx->rt = rt;

  return 0;
}
