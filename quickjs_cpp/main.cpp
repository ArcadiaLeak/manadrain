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

#include "js_atom_enum.hpp"
#include "js_class_enum.hpp"

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

struct JSContext {
  std::shared_ptr<struct JSRuntime> rt;
};

using JSValue = std::variant<int32_t, int64_t, double, std::shared_ptr<JSHeapAny>>;
using JSCFunction = std::function<JSValue(
  std::shared_ptr<JSContext> ctx, JSValue this_val,
  int argc, std::shared_ptr<JSValue[]> argv
)>;

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

struct JSParseState {
  std::shared_ptr<JSContext> ctx;
  JSToken token;

  std::shared_ptr<char[]> buf;
  size_t buf_prev;
  size_t buf_curr;
  size_t buf_size;
};

enum JS_TOK {
  JS_TOK_NUMBER = -128, JS_TOK_STRING, JS_TOK_TEMPLATE, JS_TOK_IDENT,
  JS_TOK_REGEXP, JS_TOK_MUL_ASSIGN, JS_TOK_DIV_ASSIGN, JS_TOK_MOD_ASSIGN,
  JS_TOK_PLUS_ASSIGN, JS_TOK_MINUS_ASSIGN, JS_TOK_SHL_ASSIGN,
  JS_TOK_SAR_ASSIGN, JS_TOK_SHR_ASSIGN, JS_TOK_AND_ASSIGN,
  JS_TOK_XOR_ASSIGN, JS_TOK_OR_ASSIGN, JS_TOK_POW_ASSIGN,
  JS_TOK_LAND_ASSIGN, JS_TOK_LOR_ASSIGN, JS_TOK_DOUBLE_QUESTION_MARK_ASSIGN,
  JS_TOK_DEC, JS_TOK_INC, JS_TOK_SHL, JS_TOK_SAR, JS_TOK_SHR, JS_TOK_LT,
  JS_TOK_LTE, JS_TOK_GT, JS_TOK_GTE, JS_TOK_EQ, JS_TOK_STRICT_EQ, JS_TOK_NEQ,
  JS_TOK_STRICT_NEQ, JS_TOK_LAND, JS_TOK_LOR, JS_TOK_POW, JS_TOK_ARROW,
  JS_TOK_ELLIPSIS, JS_TOK_DOUBLE_QUESTION_MARK, JS_TOK_QUESTION_MARK_DOT,
  JS_TOK_ERROR, JS_TOK_PRIVATE_NAME, JS_TOK_EOF, JS_TOK_NULL, JS_TOK_FALSE,
  JS_TOK_TRUE, JS_TOK_IF, JS_TOK_ELSE, JS_TOK_RETURN, JS_TOK_VAR, JS_TOK_THIS,
  JS_TOK_DELETE, JS_TOK_VOID, JS_TOK_TYPEOF, JS_TOK_NEW, JS_TOK_IN,
  JS_TOK_INSTANCEOF, JS_TOK_DO, JS_TOK_WHILE, JS_TOK_FOR, JS_TOK_BREAK,
  JS_TOK_CONTINUE, JS_TOK_SWITCH, JS_TOK_CASE, JS_TOK_DEFAULT, JS_TOK_THROW,
  JS_TOK_TRY, JS_TOK_CATCH, JS_TOK_FINALLY, JS_TOK_FUNCTION, JS_TOK_DEBUGGER,
  JS_TOK_WITH, JS_TOK_CLASS, JS_TOK_CONST, JS_TOK_ENUM, JS_TOK_EXPORT,
  JS_TOK_EXTENDS, JS_TOK_IMPORT, JS_TOK_SUPER, JS_TOK_IMPLEMENTS,
  JS_TOK_INTERFACE, JS_TOK_LET, JS_TOK_PACKAGE, JS_TOK_PRIVATE,
  JS_TOK_PROTECTED, JS_TOK_PUBLIC, JS_TOK_STATIC, JS_TOK_YIELD,
  JS_TOK_AWAIT, JS_TOK_OF
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
