#include <print>
#include <fstream>
#include <filesystem>
#include <string>
#include <cstdio>
#include <ranges>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <functional>
#include <span>
#include <stdexcept>

#include "js_atom_enum.hpp"
#include "js_class_enum.hpp"
#include "js_token_enum.hpp"

enum class JS_ATOM_TYPE {
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

struct JSHeapAny {
  virtual ~JSHeapAny() = default;
  JSHeapAny(const JSHeapAny&) = default;
  JSHeapAny& operator=(const JSHeapAny&) = default;
  JSHeapAny(JSHeapAny&&) noexcept = default;
  JSHeapAny& operator=(JSHeapAny&&) noexcept = default;

  JSHeapAny() = default;
};

struct JSString : JSHeapAny {
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

struct JSToken {
  size_t offset;
  JSTokenVal val;
};

enum class JS_EVAL_TYPE {
  GLOBAL, MODULE, DIRECT, INDIRECT
};

struct JSVarDef {
  std::shared_ptr<char[]> var_name;
  std::shared_ptr<JSHeapAny> func_pool;

  std::shared_ptr<JSVarDef> prev;
  std::shared_ptr<JSVarDef> next;
};

struct JSVarScope {
  std::shared_ptr<JSVarScope> parent;
  std::shared_ptr<JSVarDef> first;

  std::shared_ptr<JSVarScope> prev;
  std::shared_ptr<JSVarScope> next;
};

struct JSFunctionDef {
  std::shared_ptr<JSVarDef> vars_begin;
  std::shared_ptr<JSVarDef> vars_end;
  std::shared_ptr<JSVarDef> eval_ret;

  std::shared_ptr<JSVarScope> scope_level;
  std::shared_ptr<JSVarDef> scope_first;

  JS_EVAL_TYPE eval_type;
  bool is_struct;
  bool is_global_var;
  uint8_t js_mode;
};

struct JSRuntime {
  std::unordered_map<std::string, std::shared_ptr<JSString>> str_hash;
  std::unordered_set<std::shared_ptr<JSHeapAny>> atom_hash;

  void insert_wellknown() {
    for (int i = JS_ATOM_null; i < JS_ATOM_END; i++) {
      JS_ATOM_TYPE atom_type;
      if (i == JS_ATOM_Private_brand)
        atom_type = JS_ATOM_TYPE::PRIVATE;
      else if (i >= JS_ATOM_Symbol_toPrimitive)
        atom_type = JS_ATOM_TYPE::SYMBOL;
      else
        atom_type = JS_ATOM_TYPE::STRING;
      
      if (atom_type == JS_ATOM_TYPE::STRING) {
        std::string str{js_atom_init[i - 1]};
        std::shared_ptr<JSString> js_str = std::make_shared<JSString>();
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

struct JSContext {
  std::shared_ptr<JSRuntime> rt;
};

JSValue JS_EvalInternal(
  std::shared_ptr<JSContext> ctx,
  JSValue this_obj,
  std::shared_ptr<char[]> input,
  size_t input_len,
  std::shared_ptr<char[]> filename,
  int flags,
  int scope_idx
) {
  std::shared_ptr<JSParseState> s = std::make_shared<JSParseState>(
    JSParseState{
      .ctx = ctx,
      .filename = filename,
      .buf = input,
      .buf_size = input_len
    }
  );

  throw std::runtime_error("unimplemented!");
}

using JSCFunction = std::function<JSValue(
  std::shared_ptr<JSContext> ctx,
  JSValue this_val,
  int argc,
  std::shared_ptr<JSValue[]> argv
)>;

struct JSMchInst {
  virtual ~JSMchInst() = default;
  JSMchInst(const JSMchInst&) = default;
  JSMchInst& operator=(const JSMchInst&) = default;
  JSMchInst(JSMchInst&&) noexcept = default;
  JSMchInst& operator=(JSMchInst&&) noexcept = default;

  JSMchInst() = default;

  std::shared_ptr<JSMchInst> next;
  std::shared_ptr<JSMchInst> prev;
};

struct JSEnterScope : JSMchInst {
  std::shared_ptr<JSVarScope> scope_;
};

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
