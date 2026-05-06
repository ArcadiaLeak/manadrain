#include <array>

namespace Manadrain {
inline constexpr std::size_t S_ATOM_const{0};
inline constexpr std::size_t S_ATOM_let{1};
inline constexpr std::size_t S_ATOM_var{2};
inline constexpr std::size_t S_ATOM_class{3};
inline constexpr std::size_t S_ATOM_function{4};
inline constexpr std::size_t S_ATOM_return{5};
inline constexpr std::size_t S_ATOM_import{6};
inline constexpr std::size_t S_ATOM_export{7};
inline constexpr std::size_t S_ATOM_from{8};
inline constexpr std::size_t S_ATOM_as{9};
inline constexpr std::size_t S_ATOM_default{10};
inline constexpr std::size_t S_ATOM_undefined{11};
inline constexpr std::size_t S_ATOM_null{12};
inline constexpr std::size_t S_ATOM_true{13};
inline constexpr std::size_t S_ATOM_false{14};
inline constexpr std::size_t S_ATOM_if{15};
inline constexpr std::size_t S_ATOM_else{16};
inline constexpr std::size_t S_ATOM_while{17};
inline constexpr std::size_t S_ATOM_for{18};
inline constexpr std::size_t S_ATOM_do{19};
inline constexpr std::size_t S_ATOM_break{20};
inline constexpr std::size_t S_ATOM_continue{21};
inline constexpr std::size_t S_ATOM_switch{22};
inline constexpr std::size_t S_ATOM_int{23};
inline constexpr std::size_t S_ATOM_long{24};
inline constexpr std::size_t S_ATOM_uint{25};
inline constexpr std::size_t S_ATOM_ulong{26};
inline constexpr std::size_t S_ATOM_float{27};
inline constexpr std::size_t S_ATOM_double{28};

static const std::array<std::string_view, 29> S_ATOM_ARR{
    {"const",  "let",    "var",   "class",    "function", "return",
     "import", "export", "from",  "as",       "default",  "undefined",
     "null",   "true",   "false", "if",       "else",     "while",
     "for",    "do",     "break", "continue", "switch",   "int",
     "long",   "uint",   "ulong", "float",    "double"}};

static bool is_reserved(std::size_t atom_idx) {
  switch (atom_idx) {
  case S_ATOM_const:
  case S_ATOM_let:
  case S_ATOM_var:
  case S_ATOM_class:
  case S_ATOM_function:
  case S_ATOM_return:
  case S_ATOM_import:
  case S_ATOM_export:
  case S_ATOM_from:
  case S_ATOM_as:
  case S_ATOM_default:
  case S_ATOM_undefined:
  case S_ATOM_null:
  case S_ATOM_true:
  case S_ATOM_false:
  case S_ATOM_if:
  case S_ATOM_else:
  case S_ATOM_while:
  case S_ATOM_for:
  case S_ATOM_do:
  case S_ATOM_break:
  case S_ATOM_continue:
  case S_ATOM_switch:
  case S_ATOM_int:
  case S_ATOM_long:
  case S_ATOM_uint:
  case S_ATOM_ulong:
  case S_ATOM_float:
  case S_ATOM_double:
    return 1;
  default:
    return 0;
  }
}
} // namespace Manadrain
