import std;

namespace Manadrain {
struct UcharAwaitable {};

template <typename R>
struct Parser {
  template <typename P>
  struct UcharAwaiter {
    std::coroutine_handle<P> coro_handle;

    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<P> h) { coro_handle = h; }
    char32_t await_resume() { return coro_handle.promise().uchar; }
  };

  struct Promise {
    std::expected<R, int> result;
    char32_t uchar;

    auto initial_suspend() noexcept { return std::suspend_never{}; }
    auto final_suspend() noexcept { return std::suspend_always{}; }

    void unhandled_exception() { result = std::unexpected{-1}; }
    void return_value(R ret) { result = ret; }

    auto get_return_object() {
      return Parser{std::coroutine_handle<Promise>::from_promise(*this)};
    }

    auto await_transform(UcharAwaitable awaitable) {
      return UcharAwaiter<Promise>{};
    }
  };
  using promise_type = Promise;

  std::coroutine_handle<Promise> coro_handle;
  bool done() { return coro_handle.done(); }
  void push_uchar_and_resume(char32_t uchar) {
    coro_handle.promise().uchar = uchar;
    coro_handle.resume();
  }
};

Parser<char32_t> hex_digit() {
  char32_t digit{co_await UcharAwaitable{}};
  if (digit >= '0' && digit <= '9')
    co_return digit - '0';
  if (digit >= 'A' && digit <= 'F')
    co_return digit - 'A' + 10;
  if (digit >= 'a' && digit <= 'f')
    co_return digit - 'a' + 10;
  throw std::exception{};
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

  std::println(
      "{}", static_cast<std::uint32_t>(*parser.coro_handle.promise().result));
  parser.coro_handle.destroy();

  return 0;
}
