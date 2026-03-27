import std;

namespace Manadrain {
struct ParseDriver {
  std::string source_str;
  std::stack<std::string_view> view_stack;

  ParseDriver(std::string str)
      : source_str{std::move(str)}, view_stack{{source_str}} {}

  std::optional<char32_t> next_uchar() {
    if (view_stack.top().empty())
      return std::nullopt;
    char32_t ch = view_stack.top().front();
    view_stack.top() = view_stack.top() | std::views::drop(1);
    return ch;
  }

  void push_view() { view_stack.push(view_stack.top()); }
};

struct UcharAwaiter {
  std::shared_ptr<ParseDriver> driver;

  bool await_ready() { return false; }
  std::optional<char32_t> await_resume() { return driver->next_uchar(); }

  template <typename P>
  std::coroutine_handle<P> await_suspend(std::coroutine_handle<P> h) {
    driver = h.promise().driver;
    return h;
  }
};

template <typename R>
struct ParseCoro;

template <typename R>
struct ParsePromise {
  R result;
  std::exception_ptr except;
  std::coroutine_handle<> caller_handle;
  std::shared_ptr<ParseDriver> driver;

  void return_value(R ret) { result = ret; }
  void unhandled_exception() { except = std::current_exception(); }

  auto get_return_object() {
    std::coroutine_handle coro_handle =
        std::coroutine_handle<ParsePromise>::from_promise(*this);
    return ParseCoro<R>{coro_handle};
  }

  struct Awaiter {
    std::coroutine_handle<> caller_handle;

    std::coroutine_handle<> await_suspend(
        std::coroutine_handle<ParsePromise> h) noexcept {
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
struct ParseCoro {
  using promise_type = ParsePromise<R>;
  std::coroutine_handle<promise_type> coro_handle;

  struct Awaiter {
    ParseCoro parse_coro;

    R await_resume() {
      std::exception_ptr except = parse_coro.coro_handle.promise().except;
      R result = std::move(parse_coro.coro_handle.promise().result);
      std::shared_ptr driver = parse_coro.coro_handle.promise().driver;
      parse_coro.coro_handle.destroy();

      if (except)
        std::rethrow_exception(except);

      std::string_view shifted_view = driver->view_stack.top();
      driver->view_stack.pop();
      if (result)
        driver->view_stack.top() = shifted_view;
      return result;
    }

    template <typename P>
    std::coroutine_handle<promise_type> await_suspend(
        std::coroutine_handle<P> h) {
      h.promise().driver->push_view();
      parse_coro.coro_handle.promise().driver = h.promise().driver;
      parse_coro.coro_handle.promise().caller_handle = h;
      return parse_coro.coro_handle;
    }

    bool await_ready() { return false; }
  };

  Awaiter operator co_await() const noexcept { return {*this}; }
};

ParseCoro<std::optional<std::uint32_t>> hex_digit() {
  std::optional digit{co_await UcharAwaiter{}};
  if (not digit)
    co_return std::nullopt;
  if (*digit >= '0' && *digit <= '9')
    co_return *digit - '0';
  if (*digit >= 'A' && *digit <= 'F')
    co_return *digit - 'A' + 10;
  if (*digit >= 'a' && *digit <= 'f')
    co_return *digit - 'a' + 10;
  co_return std::nullopt;
}

struct Wrapper {
  std::uint32_t num;
};

ParseCoro<Wrapper> hex_digit_wrapper() {
  co_return {*co_await hex_digit()};
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

  Manadrain::ParseCoro parse_coro{Manadrain::hex_digit_wrapper()};
  parse_coro.coro_handle.promise().driver =
      std::make_shared<Manadrain::ParseDriver>(std::move(source_str));
  parse_coro.coro_handle.resume();

  std::println("{}", parse_coro.coro_handle.promise().result.num);
  parse_coro.coro_handle.destroy();

  return 0;
}
