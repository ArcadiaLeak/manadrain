#include <cassert>

#include <unictype.h>
#include <unistr.h>

#include "language.hpp"
#include "static_atoms.hpp"

namespace Manadrain {
void Language::prev() {
  assert(not backtrace.empty());
  position -= backtrace.top();
  backtrace.pop();
}

std::int32_t Language::next() {
  if (position >= text_input.size()) {
    backtrace.emplace();
    return -1;
  } else {
    ucs4_t ch;
    int advance{u8_mbtoucr(&ch, text_input.data() + position,
                           text_input.size() - position)};
    assert(advance >= 0);
    position += advance;
    backtrace.push(advance);
    return ch;
  }
}

void Language::backtrack(std::size_t N) {
  for (int i = 0; i < N; ++i)
    prev();
}

Language::TOKEN Language::tokenize() { return std::monostate{}; }
} // namespace Manadrain
