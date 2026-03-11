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

struct JSHeapMem {};

struct JSString : JSHeapMem {
  std::string str;
};

struct JSContext {};

using JSValue = std::variant<int32_t, int64_t, double, std::shared_ptr<JSHeapMem>>;
using JSCFunction = std::function<JSValue(
  std::shared_ptr<JSContext> ctx, JSValue this_val,
  int argc, std::shared_ptr<JSValue[]> argv
)>;

struct JSRuntime {
  std::unordered_map<std::string, std::shared_ptr<JSString>> str_hash;
  std::unordered_set<std::shared_ptr<JSHeapMem>> atom_hash;

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
        js_str->str = str;
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

  JSRuntime rt{};
  rt.insert_wellknown();

  std::println("{}", rt.str_hash[js_atom_init[JS_ATOM__ret_ - 1]]->str);

  return 0;
}
