module quickjs:utility;
import std;

namespace JS {
  namespace utility {
    using PaddedBuf = std::ranges::concat_view<
      std::ranges::owning_view<std::string>,
      std::ranges::repeat_view<char>
    >;

    std::shared_ptr<char[]> shared_str(std::string_view str_view) {
      std::shared_ptr str = std::make_shared<char[]>(str_view.size());
      str_view.copy(str.get(), str_view.size());
      return str;
    }
  }
}
