#include <print>
#include <fstream>
#include <filesystem>
#include <string>
#include <cstdio>
#include <ranges>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "js_atom_enum.hpp"

enum {
  JS_ATOM_TYPE_STRING = 1,
  JS_ATOM_TYPE_GLOBAL_SYMBOL,
  JS_ATOM_TYPE_SYMBOL,
  JS_ATOM_TYPE_PRIVATE,
};

struct JS_GC_OBJECT {};
struct JSAtom : JS_GC_OBJECT {};
struct JSString : JSAtom {};

struct JSRuntime {
  std::unordered_map<std::string, std::shared_ptr<JSString>> str_hash;
  std::unordered_set<std::shared_ptr<JS_GC_OBJECT>> gc_obj_hash;

  void PopulateWithWellknown() {
    for (int i = JS_ATOM_null; i < JS_ATOM_END; i++) {
      int atom_type;
      if (i == JS_ATOM_Private_brand)
        atom_type = JS_ATOM_TYPE_PRIVATE;
      else if (i >= JS_ATOM_Symbol_toPrimitive)
        atom_type = JS_ATOM_TYPE_SYMBOL;
      else
        atom_type = JS_ATOM_TYPE_STRING;
      
      if (atom_type == JS_ATOM_TYPE_STRING) {
        std::string str{js_atom_init[i - 1]};
        std::shared_ptr<JSString> js_str = std::make_shared<JSString>();
        str_hash[str] = js_str;
        gc_obj_hash.insert(js_str);
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
  rt.PopulateWithWellknown();

  std::println("{} {}", rt.str_hash.size(), rt.gc_obj_hash.size());

  return 0;
}
