module quickjs:common;
import std;

namespace JS {
  namespace common {
    using PaddedBuf = std::ranges::concat_view<
      std::ranges::owning_view<std::string>,
      std::ranges::repeat_view<char>
    >;
  }
}
