#include <cstdint>
#include <memory>
#include <vector>

namespace Manadrain {
class Machine;

class Language {
public:
  Language();

  Language(Language &&other) noexcept;
  Language &operator=(Language &&other) noexcept;

  Language(const Language &) = delete;
  Language &operator=(const Language &) = delete;

  ~Language();

  std::unique_ptr<const std::vector<std::uint8_t>> text_buffer;
  void compile_and_execute();

private:
  std::unique_ptr<Machine> machine;
};
} // namespace Manadrain
