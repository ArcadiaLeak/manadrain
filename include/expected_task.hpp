#include <coroutine>
#include <expected>
#include <stdexcept>

template <typename T, typename E> struct expected_task {
  struct promise_type {
    std::expected<T, E> result;
    std::exception_ptr eptr;

    std::suspend_never initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    expected_task get_return_object() {
      return expected_task{HANDLE::from_promise(*this)};
    }
    void unhandled_exception() { eptr = std::current_exception(); }
    template <typename U> auto await_transform(std::expected<U, E> &&e) {
      struct Awaiter {
        std::expected<U, E> value;
        bool await_ready() { return value.has_value(); }
        void await_suspend(std::coroutine_handle<promise_type> handle) {
          handle.promise().result = std::unexpected{value.error()};
        }
        U await_resume() { return *value; }
      };
      return Awaiter{std::move(e)};
    }
    void return_value(std::expected<T, E> &&e) { result = std::move(e); }
  };
  using HANDLE = std::coroutine_handle<promise_type>;
  HANDLE handle;

  expected_task(HANDLE h) : handle{h} {}
  ~expected_task() {
    if (handle)
      handle.destroy();
  }

  expected_task(const expected_task &) = delete;
  expected_task(expected_task &&other) noexcept : handle{other.handle} {
    other.handle = nullptr;
  }

  std::expected<T, E> ok() const {
    if (handle.promise().eptr)
      std::rethrow_exception(handle.promise().eptr);
    return std::move(handle.promise().result);
  }
};
