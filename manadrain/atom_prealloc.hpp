#include <array>

namespace Manadrain {
namespace Syntax {
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

static const std::array<std::string_view, 15> atom_prealloc{
    {"const", "let", "var", "class", "function", "return", "import", "export",
     "from", "as", "default", "undefined", "null", "true", "false"}};
} // namespace Syntax
} // namespace Manadrain
