import std;

struct Parser {
  std::shared_ptr<void> handle_ptr;

  struct promise_type {
    std::optional<char32_t> result;

    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto final_suspend() noexcept { return std::suspend_always{}; }
    void unhandled_exception() {}

    Parser get_return_object() {
      std::coroutine_handle<promise_type> outer_handle{
          std::coroutine_handle<promise_type>::from_promise(*this)};

      std::shared_ptr<void> ptr{
          outer_handle.address(), [](void* addr) {
            std::coroutine_handle<promise_type> inner_handle{
                std::coroutine_handle<promise_type>::from_address(addr)};
            if (addr)
              inner_handle.destroy();
          }};

      return {ptr};
    }

    void return_value(std::optional<char32_t> char_opt) { result = char_opt; }
  };

  struct Uchar {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<promise_type> handle) {}
    char32_t await_resume() { return 'a'; }
  };
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

int main(int argc, char* argv[]) {
  Parser parser{hex_digit()};

  std::coroutine_handle handle =
      std::coroutine_handle<Parser::promise_type>::from_address(
          parser.handle_ptr.get());
  std::println("{}", handle.done());
  handle.resume();
  std::println("{}", handle.done());
  handle.resume();
  std::println("{}", handle.done());
  std::println("{}",
               static_cast<std::uint32_t>(handle.promise().result.value()));

  return 0;
}
