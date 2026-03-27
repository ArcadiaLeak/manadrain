import std;

namespace Manadrain {
struct Driver {
  char32_t request_uchar() { return 'b'; }
};

struct UcharAwaiter {
  Driver driver;

  bool await_ready() { return false; }
  char32_t await_resume() { return driver.request_uchar(); }

  template <typename P>
  std::coroutine_handle<P> await_suspend(std::coroutine_handle<P> h) {
    driver = h.promise().driver;
    return h;
  }
};

template <typename R>
struct Parser;

template <typename R>
struct ParserPromise {
  std::expected<R, std::exception_ptr> result;
  std::coroutine_handle<> caller_handle;
  Driver driver;

  void return_value(R ret) { result = ret; }
  void unhandled_exception() {
    result = std::unexpected{std::current_exception()};
  }

  auto get_return_object() {
    std::coroutine_handle coro_handle =
        std::coroutine_handle<ParserPromise>::from_promise(*this);
    return Parser<R>{coro_handle};
  }

  struct Awaiter {
    std::coroutine_handle<> caller_handle;

    std::coroutine_handle<> await_suspend(
        std::coroutine_handle<ParserPromise> h) noexcept {
      if (caller_handle)
        return caller_handle;
      return std::noop_coroutine();
    }

    bool await_ready() noexcept { return false; }
    void await_resume() noexcept {}
  };

  auto initial_suspend() noexcept { return std::suspend_always{}; }
  auto final_suspend() noexcept { return Awaiter{caller_handle}; }
};

template <typename R>
struct Parser {
  using promise_type = ParserPromise<R>;
  std::coroutine_handle<promise_type> coro_handle;

  struct Awaiter {
    Parser parser;

    R await_resume() {
      std::expected<R, std::exception_ptr> result =
          std::move(parser.coro_handle.promise().result);
      parser.coro_handle.destroy();
      if (result)
        return *result;
      std::rethrow_exception(result.error());
    }

    template <typename P>
    std::coroutine_handle<promise_type> await_suspend(
        std::coroutine_handle<P> h) {
      parser.coro_handle.promise().driver = h.promise().driver;
      parser.coro_handle.promise().caller_handle = h;
      return parser.coro_handle;
    }

    bool await_ready() { return false; }
  };

  Awaiter operator co_await() const noexcept { return {*this}; }
};

Parser<std::uint32_t> hex_digit() {
  char32_t digit{co_await UcharAwaiter{}};
  if (digit >= '0' && digit <= '9')
    co_return digit - '0';
  if (digit >= 'A' && digit <= 'F')
    co_return digit - 'A' + 10;
  if (digit >= 'a' && digit <= 'f')
    co_return digit - 'a' + 10;
  throw std::exception{};
}

struct Wrapper {
  std::uint32_t num;
};

Parser<Wrapper> hex_digit_wrapper() {
  co_return {co_await hex_digit()};
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

  Manadrain::Parser parser{Manadrain::hex_digit_wrapper()};
  parser.coro_handle.promise().driver = Manadrain::Driver{};
  parser.coro_handle.resume();

  std::println("{}", parser.coro_handle.promise().result->num);
  parser.coro_handle.destroy();

  return 0;
}
