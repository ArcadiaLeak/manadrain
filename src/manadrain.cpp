module;
#include <unicode/uchar.h>
#include <unicode/ustring.h>
export module manadrain;
import std;

namespace Manadrain {
bool is_hi_surrogate(char32_t c) {
  return (c >> 10) == (0xD800 >> 10); /* 0xD800-0xDBFF */
}
bool is_lo_surrogate(char32_t c) {
  return (c >> 10) == (0xDC00 >> 10); /* 0xDC00-0xDFFF */
}
char32_t from_surrogate(char32_t hi, char32_t lo) {
  return 0x10000 + 0x400 * (hi - 0xD800) + (lo - 0xDC00);
}

struct ParseDriver {
  std::u32string source_str;
  std::stack<std::u32string_view> view_stack;

  ParseDriver(std::u32string str)
      : source_str{std::move(str)}, view_stack{{source_str}} {}
};

namespace Command {
struct Shift {
  std::shared_ptr<ParseDriver> driver;

  bool await_ready() { return false; }
  std::optional<char32_t> await_resume() {
    if (driver->view_stack.top().empty())
      return std::nullopt;
    char32_t ch = driver->view_stack.top().front();
    driver->view_stack.top() = driver->view_stack.top() | std::views::drop(1);
    return ch;
  }

  template <typename P>
  std::coroutine_handle<P> await_suspend(std::coroutine_handle<P> h) {
    driver = h.promise().driver;
    return h;
  }
};

struct StartsWith {
  std::u32string_view rhs;
  std::size_t idx;
  std::shared_ptr<ParseDriver> driver;

  bool await_ready() { return false; }
  bool await_resume() {
    if (driver->view_stack.top().size() <= idx)
      return false;
    return driver->view_stack.top().substr(idx).starts_with(rhs);
  }

  template <typename P>
  std::coroutine_handle<P> await_suspend(std::coroutine_handle<P> h) {
    driver = h.promise().driver;
    return h;
  }
};

struct Drop {
  std::size_t count;
  std::shared_ptr<ParseDriver> driver;

  bool await_ready() { return false; }
  void await_resume() {
    driver->view_stack.top() =
        driver->view_stack.top() | std::views::drop(count);
  }

  template <typename P>
  std::coroutine_handle<P> await_suspend(std::coroutine_handle<P> h) {
    driver = h.promise().driver;
    return h;
  }
};

struct Peek {
  std::size_t idx;
  std::shared_ptr<ParseDriver> driver;

  bool await_ready() { return false; }
  std::optional<char32_t> await_resume() {
    if (driver->view_stack.top().size() <= idx)
      return std::nullopt;
    return driver->view_stack.top()[idx];
  }

  template <typename P>
  std::coroutine_handle<P> await_suspend(std::coroutine_handle<P> h) {
    driver = h.promise().driver;
    return h;
  }
};
}  // namespace Command

template <typename R>
struct ParseCoro {
  struct promise_type {
    R result;
    std::exception_ptr except;
    std::coroutine_handle<> caller_handle;
    std::shared_ptr<ParseDriver> driver;

    void return_value(R ret) { result = std::move(ret); }
    void unhandled_exception() { except = std::current_exception(); }

    auto get_return_object() {
      std::coroutine_handle coro_handle =
          std::coroutine_handle<promise_type>::from_promise(*this);
      return ParseCoro<R>{coro_handle};
    }

    struct FinalizingAwaiter {
      std::coroutine_handle<> caller_handle;

      std::coroutine_handle<> await_suspend(
          std::coroutine_handle<promise_type> h) noexcept {
        if (caller_handle)
          return caller_handle;
        return std::noop_coroutine();
      }

      bool await_ready() noexcept { return false; }
      void await_resume() noexcept {}
    };

    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto final_suspend() noexcept { return FinalizingAwaiter{caller_handle}; }
  };

  std::coroutine_handle<promise_type> coro_handle;

  struct NestingAwaiter {
    std::coroutine_handle<promise_type> nested_handle;

    R await_resume() {
      R result = std::move(nested_handle.promise().result);
      std::exception_ptr except = nested_handle.promise().except;
      std::shared_ptr driver = nested_handle.promise().driver;

      nested_handle.destroy();
      if (except)
        std::rethrow_exception(except);

      std::u32string_view shifted_view = driver->view_stack.top();
      driver->view_stack.pop();
      if (result)
        driver->view_stack.top() = shifted_view;
      return result;
    }

    template <typename P>
    std::coroutine_handle<promise_type> await_suspend(
        std::coroutine_handle<P> h) {
      std::shared_ptr driver = h.promise().driver;
      driver->view_stack.push(driver->view_stack.top());
      nested_handle.promise().driver = driver;
      nested_handle.promise().caller_handle = h;
      return nested_handle;
    }

    bool await_ready() { return false; }
  };

  NestingAwaiter operator co_await() const noexcept { return {coro_handle}; }
};

template <typename T>
using ParseOptional = ParseCoro<std::optional<T>>;
template <typename T, typename U>
using ParseExpected = ParseCoro<std::expected<T, U>>;

ParseOptional<std::uint32_t> parse_hex() {
  std::optional digit{co_await Command::Shift{}};
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

enum class BAD_ESCAPE { MALFORMED, PER_SE_BACKSLASH, OCTAL_SEQ };
enum class ESC_RULE {
  IDENTIFIER,
  REGEXP_ASCII,
  REGEXP_UTF16,
  STRING_IN_SLOPPY_MODE,
  STRING_IN_STRICT_MODE,
  STRING_IN_TEMPLATE
};

ParseExpected<char32_t, BAD_ESCAPE> parse_escape(ESC_RULE esc_rule) {
  std::optional head = co_await Command::Shift{};
  if (not head)
    co_return std::unexpected{BAD_ESCAPE::PER_SE_BACKSLASH};

  switch (*head) {
    case 'b':
      co_return '\b';
    case 'f':
      co_return '\f';
    case 'n':
      co_return '\n';
    case 'r':
      co_return '\r';
    case 't':
      co_return '\t';
    case 'v':
      co_return '\v';

    case 'x': {
      std::optional hex0 = co_await parse_hex();
      if (not hex0)
        co_return std::unexpected{BAD_ESCAPE::MALFORMED};
      std::optional hex1 = co_await parse_hex();
      if (not hex1)
        co_return std::unexpected{BAD_ESCAPE::MALFORMED};
      co_return (*hex0 << 4) | *hex1;
    }

    case 'u': {
      std::optional open_or_uchar = co_await Command::Shift{};
      if (not open_or_uchar)
        co_return std::unexpected{BAD_ESCAPE::MALFORMED};
      if (open_or_uchar == '{' && esc_rule != ESC_RULE::REGEXP_ASCII) {
        char32_t utf16_char = 0;
        do {
          std::optional hex = co_await parse_hex();
          if (not hex) {
            std::optional close_or_uchar = co_await Command::Shift{};
            if (not close_or_uchar)
              co_return std::unexpected{BAD_ESCAPE::MALFORMED};
            if (close_or_uchar == '}')
              co_return utf16_char;
          }
          utf16_char = (utf16_char << 4) | *hex;
          if (utf16_char > 0x10FFFF)
            co_return std::unexpected{BAD_ESCAPE::MALFORMED};
        } while (true);
      } else {
        char32_t high_surr = 0;
        for (int i = 0; i < 4; i++) {
          std::optional hex = co_await parse_hex();
          if (not hex)
            co_return std::unexpected{BAD_ESCAPE::MALFORMED};
          high_surr = (high_surr << 4) | *hex;
        }

        if (is_hi_surrogate(high_surr) && esc_rule == ESC_RULE::REGEXP_UTF16 &&
            (co_await Command::StartsWith{U"\\u"})) {
          co_await Command::Drop{2};
          char32_t low_surr = 0;
          for (int i = 0; i < 4; i++) {
            std::optional hex = co_await parse_hex();
            if (not hex)
              co_return high_surr;
            low_surr = (low_surr << 4) | *hex;
          }
          if (is_lo_surrogate(low_surr))
            co_return from_surrogate(high_surr, low_surr);
        }

        co_return high_surr;
      }
    }

    case '0': {
      std::optional ahead = co_await Command::Peek{};
      if (not ahead.transform([](char32_t uch) { return std::isdigit(uch); })
                  .value_or(false))
        co_return 0;
    }
      [[fallthrough]];

    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
      switch (esc_rule) {
        case ESC_RULE::STRING_IN_STRICT_MODE:
          co_return std::unexpected{BAD_ESCAPE::OCTAL_SEQ};

        case ESC_RULE::STRING_IN_TEMPLATE:
        case ESC_RULE::REGEXP_UTF16:
          co_return std::unexpected{BAD_ESCAPE::MALFORMED};

        default:
          char32_t octal = *head - '0';
          std::optional<char32_t> ahead;
          ahead =
              (co_await Command::Peek{}).transform([](char32_t ahead_digit) {
                return ahead_digit - '0';
              });
          if (not ahead || *ahead > 7)
            co_return octal;
          co_await Command::Drop{1};
          octal = (octal << 3) | *ahead;

          if (octal >= 32)
            co_return octal;

          ahead =
              (co_await Command::Peek{}).transform([](char32_t ahead_digit) {
                return ahead_digit - '0';
              });
          if (not ahead || *ahead > 7)
            co_return octal;
          co_await Command::Drop{1};
          octal = (octal << 3) | *ahead;
          co_return octal;
      }

    case '8':
    case '9':
      if (esc_rule == ESC_RULE::STRING_IN_STRICT_MODE ||
          esc_rule == ESC_RULE::STRING_IN_TEMPLATE)
        co_return std::unexpected{BAD_ESCAPE::MALFORMED};
      [[fallthrough]];

    default:
      co_return std::unexpected{BAD_ESCAPE::PER_SE_BACKSLASH};
  }
}

std::u32string utf32_convert(const std::string& narrow) {
  UErrorCode status = U_ZERO_ERROR;

  std::int32_t u16_size{};
  u_strFromUTF8(nullptr, 0, &u16_size, narrow.data(), -1, &status);
  if (status == U_BUFFER_OVERFLOW_ERROR)
    status = U_ZERO_ERROR;

  std::u16string medium{};
  medium.resize(u16_size);
  u_strFromUTF8(medium.data(), u16_size + 1, nullptr, narrow.data(), -1,
                &status);

  if (U_FAILURE(status))
    throw std::runtime_error{
        std::format("UTF-8 to UTF-16 failed: {}", u_errorName(status))};

  std::int32_t u32_size{};
  u_strToUTF32(nullptr, 0, &u32_size, medium.data(), -1, &status);
  if (status == U_BUFFER_OVERFLOW_ERROR)
    status = U_ZERO_ERROR;

  std::basic_string<UChar32> wide{};
  wide.resize(u32_size);
  u_strToUTF32(wide.data(), u32_size + 1, nullptr, medium.data(), -1, &status);

  if (U_FAILURE(status))
    throw std::runtime_error{
        std::format("UTF-16 to UTF-32 failed: {}", u_errorName(status))};

  return std::u32string{std::from_range,
                        wide | std::views::transform([](UChar32 uchar) {
                          return static_cast<char32_t>(uchar);
                        })};
}

ParseCoro<bool> match_identifier(std::size_t idx,
                                 std::u32string_view rhs_view) {
  if (not co_await Command::StartsWith{rhs_view, idx})
    co_return false;
  std::optional ch = co_await Command::Peek{idx + rhs_view.size()};
  if (not ch)
    co_return false;
  co_return !u_hasBinaryProperty(*ch, UCHAR_XID_CONTINUE);
}

enum class STRICTNESS { SLOPPY, STRICT };
enum class BAD_STRING {
  UNEXPECTED_END,
  OCTAL_SEQ_IN_ESCAPE,
  MALFORMED_SEQ_IN_ESCAPE
};

namespace Token {
struct String {
  const char32_t sep;
  std::u32string str;

  ParseExpected<char32_t, BAD_STRING> parse_escaped_uchar(
      const STRICTNESS strictness,
      bool& must_continue) {
    std::optional ch = co_await Command::Peek{};
    if (not ch)
      co_return std::unexpected{BAD_STRING::UNEXPECTED_END};
    switch (*ch) {
      case '\'':
      case '\"':
      case '\0':
      case '\\':
        co_await Command::Drop{1};
        co_return *ch;
      case '\r':
        if (co_await Command::Peek{1} == '\n')
          co_await Command::Drop{1};
        [[fallthrough]];
      case '\n':
      case 0x2028:
      case 0x2029:
        /* ignore escaped newline sequence */
        co_await Command::Drop{1};
        must_continue = true;
        co_return *ch;
      default:
        ESC_RULE esc_rule = ESC_RULE::STRING_IN_SLOPPY_MODE;
        if (strictness == STRICTNESS::STRICT)
          esc_rule = ESC_RULE::STRING_IN_STRICT_MODE;
        else if (sep == '`')
          esc_rule = ESC_RULE::STRING_IN_TEMPLATE;
        std::expected ch_exp = co_await parse_escape(esc_rule);
        if (ch_exp)
          ch = *ch_exp;
        else if (ch_exp.error() == BAD_ESCAPE::MALFORMED)
          co_return std::unexpected{BAD_STRING::MALFORMED_SEQ_IN_ESCAPE};
        else if (ch_exp.error() == BAD_ESCAPE::OCTAL_SEQ)
          co_return std::unexpected{BAD_STRING::OCTAL_SEQ_IN_ESCAPE};
        else if (ch_exp.error() == BAD_ESCAPE::PER_SE_BACKSLASH)
          /* ignore the '\' (could output a warning) */
          co_await Command::Drop{1};
        co_return *ch;
    }
  }

  ParseExpected<std::u32string_view, BAD_STRING> parse(
      const STRICTNESS strictness) {
    std::optional<char32_t> ch{};
    do {
      ch = co_await Command::Peek{};
      if (not ch)
        co_return std::unexpected{BAD_STRING::UNEXPECTED_END};

      if (sep == '`') {
        if (ch == '\r') {
          if (co_await Command::Peek{1} == '\n')
            co_await Command::Drop{1};
          ch = '\n';
        }
      } else if (ch == '\r' || ch == '\n')
        co_return std::unexpected{BAD_STRING::UNEXPECTED_END};

      co_await Command::Drop{1};
      if (ch == sep)
        co_return str;

      if (ch == '$' && co_await Command::Peek{} == '{' && sep == '`') {
        co_await Command::Drop{1};
        co_return str;
      }

      if (ch == '\\') {
        bool must_continue = false;
        std::expected esc_exp =
            co_await parse_escaped_uchar(strictness, must_continue);
        if (not esc_exp)
          co_return std::unexpected{esc_exp.error()};
        else if (must_continue)
          continue;
        else
          ch = *esc_exp;
      }

      str.push_back(*ch);
    } while (true);
  }
};

struct Template {
  char32_t sep;
  std::u32string str;

  ParseExpected<std::u32string_view, BAD_STRING> parse_part() {
    std::optional<char32_t> ch{};
    do {
      ch = co_await Command::Shift{};
      if (not ch)
        co_return std::unexpected{BAD_STRING::UNEXPECTED_END};
      if (ch == '`')
        co_return str;
      if (ch == '$' && co_await Command::Peek{} == '{') {
        co_await Command::Drop{1};
        co_return str;
      }
      if (ch == '\\') {
        str.push_back(*ch);
        ch = co_await Command::Shift{};
        if (not ch)
          co_return std::unexpected{BAD_STRING::UNEXPECTED_END};
      }
      if (ch == '\r') {
        if (co_await Command::Peek{} == '\n')
          co_await Command::Drop{1};
        ch = '\n';
      }
      str.push_back(*ch);
    } while (true);
  }
};

struct Identifier {
  std::u32string str;

  ParseOptional<char32_t> parse_id_continue(bool& ident_has_escape) {
    std::optional ch = co_await Command::Shift{};
    if (not ch)
      co_return std::nullopt;
    if (ch == '\\' && co_await Command::Peek{} == 'u') {
      std::expected ch_esc = co_await parse_escape(ESC_RULE::IDENTIFIER);
      if (not ch_esc)
        co_return std::nullopt;
      ch = *ch_esc;
      ident_has_escape = true;
    }
    if (not u_hasBinaryProperty(*ch, UCHAR_XID_CONTINUE))
      co_return std::nullopt;
    co_return ch;
  }

  ParseOptional<std::u32string_view> parse(char32_t id_start,
                                           bool& ident_has_escape,
                                           bool is_private) {
    if (is_private)
      str.push_back('#');
    str.push_back(id_start);
    do {
      std::optional ch = co_await parse_id_continue(ident_has_escape);
      if (not ch)
        co_return str;
      str.push_back(*ch);
    } while (true);
  }
};

ParseCoro<std::size_t> whitespace() {
  std::size_t idx = 0;
  while (true) {
    std::optional ch = co_await Command::Peek{idx};
    if (not ch)
      co_return idx;

    if (u_isWhitespace(*ch)) {
      ++idx;
      continue;
    }

    if (ch == '\\') {
      std::size_t esc_idx{idx};
      std::optional esc_opt = co_await Command::Peek{++esc_idx};
      if (not esc_opt)
        co_return idx;
      if (esc_opt == '\r' || esc_opt == '\n' || esc_opt == 0x2028 ||
          esc_opt == 0x2029) {
        idx = esc_idx;
        continue;
      }
    }

    if (co_await Command::StartsWith{U"//", idx}) {
      std::size_t comment_idx{idx};
      ++comment_idx;
      while (true) {
        std::optional lb_opt = co_await Command::Peek{++comment_idx};
        if (not lb_opt || lb_opt == '\r' || lb_opt == '\n' ||
            lb_opt == 0x2028 || lb_opt == 0x2029)
          break;
      }
      idx = comment_idx;
      continue;
    }

    if (co_await Command::StartsWith{U"/*", idx}) {
      std::size_t comment_idx{idx};
      ++(++comment_idx);
      while (true) {
        std::optional asterisk_opt = co_await Command::Peek{comment_idx};
        if (not asterisk_opt)
          co_return idx;
        std::optional slash_opt = co_await Command::Peek{++comment_idx};
        if (asterisk_opt == '*' && slash_opt == '/')
          break;
      }
      idx = comment_idx;
      continue;
    }

    co_return idx;
  }
}
}  // namespace Token
}  // namespace Manadrain

export namespace Manadrain {
void Parse(std::string source_str) {
  Token::String token_str{'\"'};
  ParseCoro parse_coro{token_str.parse(STRICTNESS::SLOPPY)};
  parse_coro.coro_handle.promise().driver =
      std::make_shared<ParseDriver>(utf32_convert(source_str));
  parse_coro.coro_handle.resume();
  parse_coro.coro_handle.destroy();
}
}  // namespace Manadrain
