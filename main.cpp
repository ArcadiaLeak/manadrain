import std;

namespace Manadrain {
struct Parser {
  struct UcharTaker {
    virtual void take_uchar(char32_t) = 0;
  };

  struct Uchar {
    struct Awaiter : UcharTaker {
      char32_t uchar;

      void take_uchar(char32_t passed_uchar) override { uchar = passed_uchar; }

      bool await_ready() { return false; }
      char32_t await_resume() { return uchar; }

      template <typename T>
      void await_suspend(std::coroutine_handle<T> h) {
        h.promise().cur_awaiter = this;
      }
    };
  };

  struct Invoke {
    char32_t await_resume() { return 'a'; }
    bool await_ready() { return false; }

    template <typename T>
    void await_suspend(std::coroutine_handle<T> h) {}
  };

  struct Promise;

  struct HandleDeleter {
    using pointer = std::coroutine_handle<Promise>;
    void operator()(pointer h) const { h.destroy(); }
  };

  using Handle = std::unique_ptr<Promise, HandleDeleter>;
  Handle coro_handle;

  struct Promise {
    std::optional<char32_t> result;
    UcharTaker* cur_awaiter;

    auto initial_suspend() noexcept { return std::suspend_never{}; }
    auto final_suspend() noexcept { return std::suspend_always{}; }
    void unhandled_exception() {}

    void return_value(std::optional<char32_t> char_opt) { result = char_opt; }

    Uchar::Awaiter await_transform(Uchar awaitable) { return Uchar::Awaiter{}; }

    Parser get_return_object() {
      return Parser{
          Handle{std::coroutine_handle<Promise>::from_promise(*this)}};
    }
  };
  using promise_type = Promise;

  Promise& promise() { return coro_handle.get().promise(); }

  void push_uchar_and_resume(char32_t uchar) {
    coro_handle.get().promise().cur_awaiter->take_uchar(uchar);
    coro_handle.get().promise().cur_awaiter = nullptr;
    coro_handle.get().resume();
  }

  bool done() { return coro_handle.get().done(); }
};

Parser hex_digit() {
  char32_t digit{co_await Parser::Uchar{}};
  if (digit >= '0' && digit <= '9')
    co_return digit - '0';
  if (digit >= 'A' && digit <= 'F')
    co_return digit - 'A' + 10;
  if (digit >= 'a' && digit <= 'f')
    co_return digit - 'a' + 10;
  co_return std::nullopt;
}
}  // namespace Manadrain

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::println(std::cerr, "Usage: {} <filepath>", argv[0]);
    return 1;
  }

  std::string filepath = argv[1];
  if (not std::filesystem::exists(filepath)) {
    std::println(std::cerr, "Error: file does not exist: {}", filepath);
    return 1;
  }

  std::ifstream file{filepath};
  if (not file.is_open())
    throw std::runtime_error{std::format("could not open file: {}", filepath)};

  std::string source_str = std::ranges::to<std::string>(std::ranges::subrange(
      std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}));

  Manadrain::Parser parser{Manadrain::hex_digit()};

  for (char32_t uchar : source_str) {
    if (parser.done())
      break;
    parser.push_uchar_and_resume(uchar);
  }

  std::println("{}",
               static_cast<std::uint32_t>(parser.promise().result.value()));

  return 0;
}
