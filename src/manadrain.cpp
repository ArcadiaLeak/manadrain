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

struct ParseDriver {
  std::u32string src_string;
  std::stack<ParseState> state_stack;

  ParseDriver(std::u32string str)
      : src_string{std::move(str)},
        state_stack{{ParseState{std::u32string_view{src_string}}}} {}
};

struct CurrentState {
  std::shared_ptr<ParseDriver> driver;

  bool await_ready() { return false; }
  ParseState& await_resume() { return driver->state_stack.top(); }

  template <typename P>
  std::coroutine_handle<P> await_suspend(std::coroutine_handle<P> h) {
    driver = h.promise().driver;
    return h;
  }
};

#define INJECT_CURRENT_STATE ParseState& cur_state{co_await CurrentState{}};

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

    struct FinalizeAwaiter {
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
    auto final_suspend() noexcept { return FinalizeAwaiter{caller_handle}; }
  };

  std::coroutine_handle<promise_type> coro_handle;

  struct NestedCallAwaiter {
    std::coroutine_handle<promise_type> nested_handle;

    R await_resume() {
      R result = std::move(nested_handle.promise().result);
      std::exception_ptr except = nested_handle.promise().except;
      std::shared_ptr driver = nested_handle.promise().driver;

      nested_handle.destroy();
      if (except)
        std::rethrow_exception(except);

      ParseState top_state = driver->state_stack.top();
      driver->state_stack.pop();
      if (result)
        driver->state_stack.top() = top_state;
      return result;
    }

    template <typename P>
    std::coroutine_handle<promise_type> await_suspend(
        std::coroutine_handle<P> h) {
      std::shared_ptr driver = h.promise().driver;
      driver->state_stack.push(driver->state_stack.top());
      nested_handle.promise().driver = driver;
      nested_handle.promise().caller_handle = h;
      return nested_handle;
    }

    bool await_ready() { return false; }
  };

  NestedCallAwaiter operator co_await() const noexcept { return {coro_handle}; }
};

ParseCoro<bool> parse_hex(std::uint32_t& parsed) {
  INJECT_CURRENT_STATE
  std::optional digit = cur_state.shift();
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

ParseCoro<bool> parse_escape(ESC_RULE esc_rule,
                             std::expected<char32_t, BAD_ESCAPE>& parsed) {
  INJECT_CURRENT_STATE
  std::optional head = cur_state.shift();
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
      std::optional open_or_uchar = cur_state.shift();
      if (not open_or_uchar) {
        parsed = std::unexpected{BAD_ESCAPE::MALFORMED};
        co_return parsed.has_value();
      }
      if (open_or_uchar == '{' && esc_rule != ESC_RULE::REGEXP_ASCII) {
        char32_t utf16_char = 0;
        while (true) {
          std::uint32_t hex{};
          if (not co_await parse_hex(hex)) {
            std::optional close_or_uchar = cur_state.shift();
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
            cur_state.starts_with(U"\\u")) {
          cur_state.drop(2);
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
      std::optional ahead = cur_state.peek();
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
          ahead = cur_state.peek().transform(
              [](char32_t ahead_digit) { return ahead_digit - '0'; });
          if (not ahead || *ahead > 7)
            co_return parsed.has_value();
          cur_state.drop();
          parsed = (*parsed << 3) | *ahead;

          if (*parsed >= 32)
            co_return parsed.has_value();

          ahead = cur_state.peek().transform(
              [](char32_t ahead_digit) { return ahead_digit - '0'; });
          if (not ahead || *ahead > 7)
            co_return parsed.has_value();
          cur_state.drop();
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
  INJECT_CURRENT_STATE
  if (not cur_state.starts_with(rhs_view, idx))
    co_return false;
  std::optional ch = cur_state.peek(idx + rhs_view.size());
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
  std::u32string str;

  ParseCoro<bool> parse_escaped_uchar(
      const STRICTNESS strictness,
      std::expected<char32_t, BAD_STRING>& parsed,
      bool& must_continue) {
    INJECT_CURRENT_STATE
    std::optional ch = cur_state.peek();
    if (not ch) {
      parsed = std::unexpected{BAD_STRING::UNEXPECTED_END};
      co_return parsed.has_value();
    }
    switch (*ch) {
      case '\'':
      case '\"':
      case '\0':
      case '\\':
        cur_state.drop();
        parsed = *ch;
        co_return parsed.has_value();
      case '\r':
        if (cur_state.peek() == '\n')
          cur_state.drop();
        [[fallthrough]];
      case '\n':
      case 0x2028:
      case 0x2029:
        /* ignore escaped newline sequence */
        cur_state.drop();
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
          cur_state.drop();
        parsed = *ch;
        co_return parsed.has_value();
    }
  }

  ParseCoro<bool> parse(
      const STRICTNESS strictness,
      std::expected<std::u32string_view, BAD_STRING>& parsed) {
    INJECT_CURRENT_STATE
    std::optional ch = cur_state.shift();
    if (not ch || not ch.transform([](char32_t qt) {
          return qt == '\'' || qt == '"' || qt == '`';
        })) {
      parsed = std::unexpected{BAD_STRING::MISMATCH};
      co_return parsed.has_value();
    }
    sep = *ch;

    while (true) {
      ch = cur_state.peek();
      if (not ch) {
        parsed = std::unexpected{BAD_STRING::UNEXPECTED_END};
        co_return parsed.has_value();
      }

      if (sep == '`') {
        if (ch == '\r') {
          if (cur_state.peek() == '\n')
            cur_state.drop();
          ch = '\n';
        }
      } else if (ch == '\r' || ch == '\n') {
        parsed = std::unexpected{BAD_STRING::UNEXPECTED_END};
        co_return parsed.has_value();
      }

      cur_state.drop();
      if (ch == sep) {
        parsed = str;
        co_return parsed.has_value();
      }

      if (ch == '$' && cur_state.peek() == '{' && sep == '`') {
        cur_state.drop();
        parsed = str;
        co_return parsed.has_value();
      }

      if (ch == '\\') {
        bool must_continue{false};
        std::expected<char32_t, BAD_STRING> esc_exp{};
        if (not co_await parse_escaped_uchar(strictness, esc_exp,
                                             must_continue)) {
          parsed = std::unexpected{esc_exp.error()};
          co_return parsed.has_value();
        } else if (must_continue)
          continue;
        else
          ch = *esc_exp;
      }

      str.push_back(*ch);
    }
  }
};

struct Template {
  char32_t sep;
  std::u32string str;

  ParseCoro<bool> parse_part(
      std::expected<std::u32string_view, BAD_STRING>& parsed) {
    INJECT_CURRENT_STATE
    std::optional<char32_t> ch{};
    while (true) {
      ch = cur_state.shift();
      if (not ch) {
        parsed = std::unexpected{BAD_STRING::UNEXPECTED_END};
        co_return parsed.has_value();
      }
      if (ch == '`') {
        parsed = str;
        co_return parsed.has_value();
      }
      if (ch == '$' && cur_state.peek() == '{') {
        cur_state.drop();
        parsed = str;
        co_return parsed.has_value();
      }
      if (ch == '\\') {
        str.push_back(*ch);
        ch = cur_state.shift();
        if (not ch) {
          parsed = std::unexpected{BAD_STRING::UNEXPECTED_END};
          co_return parsed.has_value();
        }
      }
      if (ch == '\r') {
        if (cur_state.peek() == '\n')
          cur_state.drop();
        ch = '\n';
      }
      str.push_back(*ch);
    }
  }
};

struct Word {
  std::u32string text;
  bool ident_has_escape;
  bool is_private;

  ParseCoro<std::optional<char32_t>> parse_id_continue() {
    INJECT_CURRENT_STATE
    std::optional ch = cur_state.shift();
    if (not ch)
      co_return std::nullopt;
    if (ch == '\\' && cur_state.peek() == 'u') {
      std::expected<char32_t, BAD_ESCAPE> ch_esc{};
      if (not co_await parse_escape(ESC_RULE::IDENTIFIER, ch_esc))
        co_return std::nullopt;
      ch = *ch_esc;
      ident_has_escape = true;
    }
    if (not u_hasBinaryProperty(*ch, UCHAR_XID_CONTINUE))
      co_return std::nullopt;
    co_return ch;
  }

  ParseCoro<std::optional<std::u32string_view>> parse() {
    INJECT_CURRENT_STATE
    if (is_private)
      text.push_back('#');
    std::optional id_start = cur_state.shift();
    if (not id_start || not u_hasBinaryProperty(*id_start, UCHAR_XID_START))
      co_return std::nullopt;
    text.push_back(*id_start);
    while (true) {
      std::optional ch = co_await parse_id_continue();
      if (not ch)
        co_return text;
      text.push_back(*ch);
    }
  }
};
}  // namespace Token

struct VariableBase {
  Token::Word var_name;
  Token::String initial;
};
struct VariableLet : VariableBase {};
struct VariableCst : VariableBase {};
struct VariableVar : VariableBase {};
using Variable = std::variant<VariableVar, VariableLet, VariableCst>;

ParseCoro<std::shared_ptr<Variable>> parse_var_decl() {
  INJECT_CURRENT_STATE
  Token::Word var_init{};
  if (not co_await var_init.parse())
    co_return nullptr;

  if (not cur_state.drop(cur_state.space_size()))
    co_return nullptr;

  std::shared_ptr<Variable> var_decl{};
  if (var_init.text == U"let")
    var_decl = std::make_shared<Variable>(VariableLet{});
  if (var_init.text == U"const")
    var_decl = std::make_shared<Variable>(VariableCst{});
  if (var_init.text == U"var")
    var_decl = std::make_shared<Variable>(VariableVar{});
  if (not var_decl)
    co_return nullptr;

  Token::Word var_name{};
  if (not co_await var_name.parse())
    co_return nullptr;
  var_decl->visit(
      [&var_name](auto& decl) { decl.var_name = std::move(var_name); });
  cur_state.drop(cur_state.space_size());

  std::optional ch = cur_state.shift();
  if (ch == '=') {
    cur_state.drop(cur_state.space_size());
    Token::String token_owner{};
    std::expected<std::u32string_view, BAD_STRING> token_view;
    co_await token_owner.parse(STRICTNESS::SLOPPY, token_view);
    var_decl->visit(
        [&token_owner](auto& decl) { decl.initial = std::move(token_owner); });
  }

  co_return var_decl;
}

ParseCoro<std::shared_ptr<Variable>> parse_statement() {
  INJECT_CURRENT_STATE
  cur_state.drop(cur_state.space_size());
  co_return co_await parse_var_decl();
}

void Parse(std::string src_string) {
  ParseCoro parse_coro = parse_statement();
  parse_coro.coro_handle.promise().driver =
      std::make_shared<ParseDriver>(utf32_convert(src_string));
  parse_coro.coro_handle.resume();
  parse_coro.coro_handle.destroy();
}
}  // namespace Manadrain
