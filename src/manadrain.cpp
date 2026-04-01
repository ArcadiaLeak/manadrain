#include <unicode/uchar.h>
#include <unicode/ustring.h>

#include <coroutine>
#include <deque>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>

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

struct ParseState {
  std::u32string_view src_view;
  bool got_lf;

  std::optional<char32_t> shift() {
    if (src_view.empty())
      return std::nullopt;
    char32_t ch = src_view.front();
    src_view = src_view | std::views::drop(1);
    return ch;
  }

  bool starts_with(std::u32string_view rhs, std::size_t idx = 0) {
    if (src_view.size() <= idx)
      return false;
    return src_view.substr(idx).starts_with(rhs);
  }

  std::size_t drop(std::size_t count = 1) {
    src_view = src_view | std::views::drop(count);
    return count;
  }

  std::optional<char32_t> peek(std::size_t idx = 0) {
    if (src_view.size() <= idx)
      return std::nullopt;
    return src_view[idx];
  }

  std::size_t space_size() {
    std::size_t idx = 0;

    while (true) {
      std::optional ch = peek(idx);
      if (not ch)
        return idx;

      if (u_isWhitespace(*ch)) {
        ++idx;
        continue;
      }

      if (starts_with(U"//", idx)) {
        std::size_t comment_idx{idx};
        ++comment_idx;
        while (true) {
          std::optional lb_opt = peek(++comment_idx);
          if (not lb_opt || lb_opt == '\r' || lb_opt == '\n' ||
              lb_opt == 0x2028 || lb_opt == 0x2029)
            break;
        }
        idx = comment_idx;
        continue;
      }

      if (starts_with(U"/*", idx)) {
        std::size_t comment_idx{idx};
        ++(++comment_idx);
        while (true) {
          std::optional asterisk_opt = peek(comment_idx);
          if (not asterisk_opt)
            return idx;
          std::optional slash_opt = peek(++comment_idx);
          if (asterisk_opt == '*' && slash_opt == '/')
            break;
        }
        idx = comment_idx;
        continue;
      }

      return idx;
    }
  }
};

struct ParsePromise;
struct ParseFrame {
  std::coroutine_handle<ParsePromise> self_handle;
  std::coroutine_handle<ParsePromise> transfer_handle;
  std::shared_ptr<ParseState> state;
};

struct ParseDriver {
  std::shared_ptr<char32_t[]> src_buffer;
  std::size_t src_buffer_size;
  std::stack<ParseFrame> coro_stack;
};

struct NestedCallAwaiter {
  std::shared_ptr<ParseDriver> driver;

  bool await_ready() { return false; }
  bool await_resume();

  std::coroutine_handle<> await_suspend(std::coroutine_handle<ParsePromise>);
};

struct AcquireStatePtr {
  std::shared_ptr<ParseDriver> driver;

  bool await_ready() { return false; }
  std::shared_ptr<ParseState> await_resume() {
    return driver->coro_stack.top().state;
  }

  template <typename P>
  std::coroutine_handle<P> await_suspend(std::coroutine_handle<P> h) {
    driver = h.promise().driver;
    return h;
  }
};

struct InitialAwaiter {
  std::coroutine_handle<ParsePromise> coro_handle;

  bool await_ready() noexcept { return false; }
  void await_resume() noexcept;

  void await_suspend(std::coroutine_handle<ParsePromise> h) noexcept {
    coro_handle = h;
  }
};

struct FinalAwaiter {
  std::coroutine_handle<> await_suspend(
      std::coroutine_handle<ParsePromise>) noexcept;

  bool await_ready() noexcept { return false; }
  void await_resume() noexcept {}
};

struct ParseCoro {
  using promise_type = ParsePromise;
  std::coroutine_handle<ParsePromise> coro_handle;
};

struct ParsePromise {
  bool ended_in_success;
  std::exception_ptr thrown_err;
  std::shared_ptr<ParseDriver> driver;

  void unhandled_exception() { thrown_err = std::current_exception(); }
  void return_value(bool ok) { ended_in_success = ok; }
  void return_value(ParseCoro tail_coro) {
    tail_coro.coro_handle.promise().driver = driver;
    driver->coro_stack.top().transfer_handle = tail_coro.coro_handle;
  }

  ParseCoro get_return_object() {
    std::coroutine_handle coro_handle =
        std::coroutine_handle<ParsePromise>::from_promise(*this);
    return ParseCoro{coro_handle};
  }

  auto initial_suspend() noexcept { return InitialAwaiter{}; }
  auto final_suspend() noexcept { return FinalAwaiter{}; }

  NestedCallAwaiter await_transform(ParseCoro nested_coro) {
    nested_coro.coro_handle.promise().driver = driver;
    driver->coro_stack.push({.self_handle = nested_coro.coro_handle,
                             .state = std::make_shared<ParseState>(
                                 *driver->coro_stack.top().state)});
    return NestedCallAwaiter{};
  }

  template <typename T>
  T&& await_transform(T&& awaiter) {
    return std::forward<T>(awaiter);
  }
};

void InitialAwaiter::await_resume() noexcept {
  std::shared_ptr driver = coro_handle.promise().driver;
  if (not driver->coro_stack.top().transfer_handle)
    return;

  driver->coro_stack.top().self_handle.destroy();
  driver->coro_stack.top().self_handle =
      driver->coro_stack.top().transfer_handle;
  driver->coro_stack.top().transfer_handle = nullptr;
}

std::coroutine_handle<> FinalAwaiter::await_suspend(
    std::coroutine_handle<ParsePromise> h) noexcept {
  std::shared_ptr driver = h.promise().driver;
  bool ended_in_success = h.promise().ended_in_success;

  if (driver->coro_stack.top().transfer_handle)
    return driver->coro_stack.top().transfer_handle;

  if (driver->coro_stack.size() == 1)
    return std::noop_coroutine();

  ParseState frame_state = *driver->coro_stack.top().state;
  driver->coro_stack.pop();

  if (ended_in_success)
    *driver->coro_stack.top().state = frame_state;

  driver->coro_stack.top().transfer_handle = h;
  return driver->coro_stack.top().self_handle;
}

bool NestedCallAwaiter::await_resume() {
  std::coroutine_handle callee_handle =
      driver->coro_stack.top().transfer_handle;
  driver->coro_stack.top().transfer_handle = nullptr;
  bool ended_in_success = callee_handle.promise().ended_in_success;
  std::exception_ptr thrown_err = callee_handle.promise().thrown_err;

  callee_handle.destroy();
  if (thrown_err)
    std::rethrow_exception(thrown_err);

  return ended_in_success;
}

std::coroutine_handle<> NestedCallAwaiter::await_suspend(
    std::coroutine_handle<ParsePromise> caller_handle) {
  driver = caller_handle.promise().driver;
  return driver->coro_stack.top().self_handle;
}

ParseCoro parse_hex(std::uint32_t& parsed) {
  std::shared_ptr cur_state = co_await AcquireStatePtr{};
  std::optional digit = cur_state->shift();
  if (not digit)
    co_return 0;
  if (*digit >= '0' && *digit <= '9') {
    parsed = *digit - '0';
    co_return 1;
  }
  if (*digit >= 'A' && *digit <= 'F') {
    parsed = *digit - 'A' + 10;
    co_return 1;
  }
  if (*digit >= 'a' && *digit <= 'f') {
    parsed = *digit - 'a' + 10;
    co_return 1;
  }
  co_return 0;
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

ParseCoro parse_escape(ESC_RULE esc_rule,
                       std::expected<char32_t, BAD_ESCAPE>& parsed) {
  std::shared_ptr cur_state = co_await AcquireStatePtr{};
  std::optional head = cur_state->shift();
  if (not head) {
    parsed = std::unexpected{BAD_ESCAPE::PER_SE_BACKSLASH};
    co_return parsed.has_value();
  }

  switch (*head) {
    case 'b':
      parsed = '\b';
      co_return parsed.has_value();
    case 'f':
      parsed = '\f';
      co_return parsed.has_value();
    case 'n':
      parsed = '\n';
      co_return parsed.has_value();
    case 'r':
      parsed = '\r';
      co_return parsed.has_value();
    case 't':
      parsed = '\t';
      co_return parsed.has_value();
    case 'v':
      parsed = '\v';
      co_return parsed.has_value();

    case 'x': {
      std::uint32_t hex0{};
      if (not co_await parse_hex(hex0)) {
        parsed = std::unexpected{BAD_ESCAPE::MALFORMED};
        co_return parsed.has_value();
      }
      std::uint32_t hex1{};
      if (not co_await parse_hex(hex1)) {
        parsed = std::unexpected{BAD_ESCAPE::MALFORMED};
        co_return parsed.has_value();
      }
      parsed = (hex0 << 4) | hex1;
      co_return parsed.has_value();
    }

    case 'u': {
      std::optional open_or_uchar = cur_state->shift();
      if (not open_or_uchar) {
        parsed = std::unexpected{BAD_ESCAPE::MALFORMED};
        co_return parsed.has_value();
      }
      if (open_or_uchar == '{' && esc_rule != ESC_RULE::REGEXP_ASCII) {
        char32_t utf16_char = 0;
        while (true) {
          std::uint32_t hex{};
          if (not co_await parse_hex(hex)) {
            std::optional close_or_uchar = cur_state->shift();
            if (not close_or_uchar) {
              parsed = std::unexpected{BAD_ESCAPE::MALFORMED};
              co_return parsed.has_value();
            }
            if (close_or_uchar == '}') {
              parsed = utf16_char;
              co_return parsed.has_value();
            }
          }
          utf16_char = (utf16_char << 4) | hex;
          if (utf16_char > 0x10FFFF) {
            parsed = std::unexpected{BAD_ESCAPE::MALFORMED};
            co_return parsed.has_value();
          }
        }
      } else {
        char32_t high_surr = 0;
        for (int i = 0; i < 4; i++) {
          std::uint32_t hex{};
          if (not co_await parse_hex(hex)) {
            parsed = std::unexpected{BAD_ESCAPE::MALFORMED};
            co_return parsed.has_value();
          }
          high_surr = (high_surr << 4) | hex;
        }

        if (is_hi_surrogate(high_surr) && esc_rule == ESC_RULE::REGEXP_UTF16 &&
            cur_state->starts_with(U"\\u")) {
          cur_state->drop(2);
          char32_t low_surr = 0;
          for (int i = 0; i < 4; i++) {
            std::uint32_t hex{};
            if (not co_await parse_hex(hex)) {
              parsed = high_surr;
              co_return parsed.has_value();
            }
            low_surr = (low_surr << 4) | hex;
          }
          if (is_lo_surrogate(low_surr)) {
            parsed = from_surrogate(high_surr, low_surr);
            co_return parsed.has_value();
          }
        }

        parsed = high_surr;
        co_return parsed.has_value();
      }
    }

    case '0': {
      std::optional ahead = cur_state->peek();
      if (not ahead.transform([](char32_t uch) { return std::isdigit(uch); })
                  .value_or(false)) {
        parsed = 0;
        co_return parsed.has_value();
      }
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
          parsed = std::unexpected{BAD_ESCAPE::OCTAL_SEQ};
          co_return parsed.has_value();

        case ESC_RULE::STRING_IN_TEMPLATE:
        case ESC_RULE::REGEXP_UTF16:
          parsed = std::unexpected{BAD_ESCAPE::MALFORMED};
          co_return parsed.has_value();

        default:
          parsed = *head - '0';
          std::optional<char32_t> ahead;
          ahead = cur_state->peek().transform(
              [](char32_t ahead_digit) { return ahead_digit - '0'; });
          if (not ahead || *ahead > 7)
            co_return parsed.has_value();
          cur_state->drop();
          parsed = (*parsed << 3) | *ahead;

          if (*parsed >= 32)
            co_return parsed.has_value();

          ahead = cur_state->peek().transform(
              [](char32_t ahead_digit) { return ahead_digit - '0'; });
          if (not ahead || *ahead > 7)
            co_return parsed.has_value();
          cur_state->drop();
          parsed = (*parsed << 3) | *ahead;
          co_return parsed.has_value();
      }

    case '8':
    case '9':
      if (esc_rule == ESC_RULE::STRING_IN_STRICT_MODE ||
          esc_rule == ESC_RULE::STRING_IN_TEMPLATE) {
        parsed = std::unexpected{BAD_ESCAPE::MALFORMED};
        co_return parsed.has_value();
      }
      [[fallthrough]];

    default:
      parsed = std::unexpected{BAD_ESCAPE::PER_SE_BACKSLASH};
      co_return parsed.has_value();
  }
}

std::shared_ptr<char32_t[]> make_shared_u32buffer(
    std::shared_ptr<char[]> narrow,
    std::size_t& wide_buf_size) {
  UErrorCode status = U_ZERO_ERROR;

  std::int32_t u16_size{};
  u_strFromUTF8(nullptr, 0, &u16_size, narrow.get(), -1, &status);
  if (status == U_BUFFER_OVERFLOW_ERROR)
    status = U_ZERO_ERROR;

  std::u16string medium{};
  medium.resize(u16_size);
  u_strFromUTF8(medium.data(), u16_size + 1, nullptr, narrow.get(), -1,
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

  wide_buf_size = wide.size();
  std::shared_ptr wide_buf = std::make_shared<char32_t[]>(wide_buf_size);
  std::ranges::copy(wide | std::views::transform([](UChar32 uchar) {
                      return static_cast<char32_t>(uchar);
                    }),
                    wide_buf.get());

  return wide_buf;
}

ParseCoro match_identifier(std::size_t idx, std::u32string_view rhs_view) {
  std::shared_ptr cur_state = co_await AcquireStatePtr{};
  if (not cur_state->starts_with(rhs_view, idx))
    co_return false;
  std::optional ch = cur_state->peek(idx + rhs_view.size());
  if (not ch)
    co_return false;
  co_return !u_hasBinaryProperty(*ch, UCHAR_XID_CONTINUE);
}

enum class STRICTNESS { SLOPPY, STRICT };
enum class BAD_STRING {
  UNEXPECTED_END,
  OCTAL_SEQ_IN_ESCAPE,
  MALFORMED_SEQ_IN_ESCAPE,
  MISMATCH
};

namespace Token {
struct String {
  char32_t sep;
  std::u32string buffer;

  ParseCoro parse_escaped_uchar(const STRICTNESS strictness,
                                std::expected<char32_t, BAD_STRING>& parsed,
                                bool& must_continue) {
    std::shared_ptr cur_state = co_await AcquireStatePtr{};
    std::optional ch = cur_state->peek();
    if (not ch) {
      parsed = std::unexpected{BAD_STRING::UNEXPECTED_END};
      co_return parsed.has_value();
    }
    switch (*ch) {
      case '\'':
      case '\"':
      case '\0':
      case '\\':
        cur_state->drop();
        parsed = *ch;
        co_return parsed.has_value();
      case '\r':
        if (cur_state->peek() == '\n')
          cur_state->drop();
        [[fallthrough]];
      case '\n':
      case 0x2028:
      case 0x2029:
        /* ignore escaped newline sequence */
        cur_state->drop();
        must_continue = true;
        parsed = *ch;
        co_return parsed.has_value();
      default:
        ESC_RULE esc_rule = ESC_RULE::STRING_IN_SLOPPY_MODE;
        if (strictness == STRICTNESS::STRICT)
          esc_rule = ESC_RULE::STRING_IN_STRICT_MODE;
        else if (sep == '`')
          esc_rule = ESC_RULE::STRING_IN_TEMPLATE;
        std::expected<char32_t, BAD_ESCAPE> ch_exp{};
        if (co_await parse_escape(esc_rule, ch_exp))
          ch = *ch_exp;
        else if (ch_exp.error() == BAD_ESCAPE::MALFORMED) {
          parsed = std::unexpected{BAD_STRING::MALFORMED_SEQ_IN_ESCAPE};
          co_return parsed.has_value();
        } else if (ch_exp.error() == BAD_ESCAPE::OCTAL_SEQ) {
          parsed = std::unexpected{BAD_STRING::OCTAL_SEQ_IN_ESCAPE};
          co_return parsed.has_value();
        } else if (ch_exp.error() == BAD_ESCAPE::PER_SE_BACKSLASH)
          /* ignore the '\' (could output a warning) */
          cur_state->drop();
        parsed = *ch;
        co_return parsed.has_value();
    }
  }

  ParseCoro parse(const STRICTNESS strictness,
                  std::optional<BAD_STRING>& err_opt) {
    std::shared_ptr cur_state = co_await AcquireStatePtr{};
    std::optional ch = cur_state->shift();
    if (not ch || not ch.transform([](char32_t qt) {
          return qt == '\'' || qt == '"' || qt == '`';
        })) {
      err_opt = BAD_STRING::MISMATCH;
      co_return !err_opt.has_value();
    }
    sep = *ch;

    while (true) {
      ch = cur_state->peek();
      if (not ch) {
        err_opt = BAD_STRING::UNEXPECTED_END;
        co_return !err_opt.has_value();
      }

      if (sep == '`') {
        if (ch == '\r') {
          if (cur_state->peek() == '\n')
            cur_state->drop();
          ch = '\n';
        }
      } else if (ch == '\r' || ch == '\n') {
        err_opt = BAD_STRING::UNEXPECTED_END;
        co_return !err_opt.has_value();
      }

      cur_state->drop();
      if (ch == sep)
        co_return !err_opt.has_value();

      if (ch == '$' && cur_state->peek() == '{' && sep == '`') {
        cur_state->drop();
        co_return !err_opt.has_value();
      }

      if (ch == '\\') {
        bool must_continue = false;
        std::expected<char32_t, BAD_STRING> esc_exp{};
        if (not co_await parse_escaped_uchar(strictness, esc_exp,
                                             must_continue)) {
          err_opt = esc_exp.error();
          co_return !err_opt.has_value();
        } else if (must_continue)
          continue;
        else
          ch = *esc_exp;
      }
      buffer.push_back(*ch);
    }
  }
};

struct Template {
  char32_t sep;
  std::u32string buffer;

  ParseCoro parse_part(std::optional<BAD_STRING>& err_opt) {
    std::shared_ptr cur_state = co_await AcquireStatePtr{};
    std::optional<char32_t> ch{};
    while (true) {
      ch = cur_state->shift();
      if (not ch) {
        err_opt = BAD_STRING::UNEXPECTED_END;
        co_return !err_opt.has_value();
      }
      if (ch == '`')
        co_return !err_opt.has_value();
      if (ch == '$' && cur_state->peek() == '{') {
        cur_state->drop();
        co_return !err_opt.has_value();
      }
      if (ch == '\\') {
        buffer.push_back(*ch);
        ch = cur_state->shift();
        if (not ch) {
          err_opt = BAD_STRING::UNEXPECTED_END;
          co_return !err_opt.has_value();
        }
      }
      if (ch == '\r') {
        if (cur_state->peek() == '\n')
          cur_state->drop();
        ch = '\n';
      }
      buffer.push_back(*ch);
    }
  }
};

struct Word {
  std::u32string buffer;
  bool ident_has_escape;
  bool is_private;

  ParseCoro parse_id_continue(char32_t& parsed) {
    std::shared_ptr cur_state = co_await AcquireStatePtr{};
    std::optional ch = cur_state->shift();
    if (not ch)
      co_return 0;
    if (ch == '\\' && cur_state->peek() == 'u') {
      std::expected<char32_t, BAD_ESCAPE> ch_esc{};
      if (not co_await parse_escape(ESC_RULE::IDENTIFIER, ch_esc))
        co_return 0;
      ch = *ch_esc;
      ident_has_escape = true;
    }
    if (not u_hasBinaryProperty(*ch, UCHAR_XID_CONTINUE))
      co_return 0;
    parsed = *ch;
    co_return 1;
  }

  ParseCoro parse() {
    std::shared_ptr cur_state = co_await AcquireStatePtr{};
    if (is_private)
      buffer.push_back('#');
    std::optional id_start = cur_state->shift();
    if (not id_start || not u_hasBinaryProperty(*id_start, UCHAR_XID_START))
      co_return 0;
    buffer.push_back(*id_start);
    while (true) {
      char32_t ch{};
      if (not co_await parse_id_continue(ch))
        co_return 1;
      buffer.push_back(ch);
    }
  }
};
}  // namespace Token

struct Variable {
  struct VariableLet {};
  struct VariableCst {};
  struct VariableVar {};
  using Kind = std::variant<VariableVar, VariableLet, VariableCst>;

  Kind kind;
  Token::Word name;
  Token::String init;

  ParseCoro parse() {
    std::shared_ptr cur_state = co_await AcquireStatePtr{};

    Token::Word var_keyword{};
    if (not co_await var_keyword.parse())
      co_return 0;

    static const std::unordered_map<std::u32string_view, Variable::Kind>
        var_kind_match = {{U"let", Variable::VariableLet{}},
                          {U"const", Variable::VariableCst{}},
                          {U"var", Variable::VariableVar{}}};
    std::unordered_map<std::u32string_view, Variable::Kind>::const_iterator
        var_kind_it = var_kind_match.find(var_keyword.buffer);
    if (var_kind_it == var_kind_match.end())
      co_return 0;
    kind = var_kind_it->second;

    if (not cur_state->drop(cur_state->space_size()))
      co_return 0;

    if (not co_await name.parse())
      co_return 0;
    cur_state->drop(cur_state->space_size());

    std::optional ch = cur_state->shift();
    if (ch == '=') {
      cur_state->drop(cur_state->space_size());
      std::optional<BAD_STRING> var_init_err{};
      co_await init.parse(STRICTNESS::SLOPPY, var_init_err);
    }
    co_return 1;
  }
};

ParseCoro parse_statement() {
  std::shared_ptr cur_state = co_await AcquireStatePtr{};
  cur_state->drop(cur_state->space_size());

  std::shared_ptr var_decl = std::make_shared<Variable>();
  co_await var_decl->parse();

  co_return 1;
}

void Parse(std::shared_ptr<char[]> src_buffer) {
  ParseCoro root_coro = parse_statement();
  std::shared_ptr driver = std::make_shared<ParseDriver>();
  driver->src_buffer =
      make_shared_u32buffer(src_buffer, driver->src_buffer_size);

  std::u32string_view root_src_view{driver->src_buffer.get(),
                                    driver->src_buffer_size};
  std::shared_ptr root_state = std::make_shared<ParseState>(root_src_view);
  ParseFrame root_frame{.self_handle = root_coro.coro_handle,
                        .state = root_state};
  driver->coro_stack.push(root_frame);

  driver->coro_stack.top().self_handle.promise().driver = driver;
  driver->coro_stack.top().self_handle.resume();
  driver->coro_stack.top().self_handle.destroy();
}
}  // namespace Manadrain
