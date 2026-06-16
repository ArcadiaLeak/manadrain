#include <cstdint>
#include <memory>
#include <vector>

namespace Manadrain {
class Error {
public:
  Error(const char *m) : message{m} {}
  Error() : message{"unspecified error!"} {}
  const char *what() noexcept { return message; }

private:
  const char *message;
};

class UnexpectedStringEnd final : public Error {
public:
  UnexpectedStringEnd() : Error{"unexpected string end!"} {}
};
class UnexpectedToken final : public Error {
public:
  UnexpectedToken() : Error{"unexpected token!"} {}
};
class MissingPunctuation final : public Error {
public:
  MissingPunctuation(char32_t ch)
      : Error{"missing punctuation!"}, must_be{ch} {}
  char32_t must_be;
};
class MissingFormalParameter final : public Error {
public:
  MissingFormalParameter() : Error{"missing formal parameter!"} {}
};
class MissingFunctionName final : public Error {
public:
  MissingFunctionName() : Error{"missing function_name!"} {}
};
class DuplicateDeclaration final : public Error {
public:
  DuplicateDeclaration() : Error{"duplicate declaration!"} {}
};
class MissingPropertyName final : public Error {
public:
  MissingPropertyName() : Error{"missing property name!"} {}
};
class InvalidPropertyName final : public Error {
public:
  InvalidPropertyName() : Error{"invalid property name!"} {}
};
class InvalidPropertyAccess final : public Error {
public:
  InvalidPropertyAccess() : Error{"invalid property access!"} {}
};
class MissingVariableName final : public Error {
public:
  MissingVariableName() : Error{"missing variable name!"} {}
};
class InvalidMethodAccess final : public Error {
public:
  InvalidMethodAccess() : Error{"invalid method access!"} {}
};
class InvalidVariableAccess final : public Error {
public:
  InvalidVariableAccess() : Error{"invalid variable access!"} {}
};
class InvalidReturnStatement final : public Error {
public:
  InvalidReturnStatement() : Error{"invalid return statement!"} {}
};
class InvalidAssignment final : public Error {
public:
  InvalidAssignment() : Error{"invalid assignment!"} {}
};
class UnresolvableCircularity final : public Error {
public:
  UnresolvableCircularity() : Error{"unresolvable circularity!"} {}
};

using VariantError = std::variant<
    Error, UnexpectedStringEnd, UnexpectedToken, MissingPunctuation,
    MissingFormalParameter, MissingFunctionName, DuplicateDeclaration,
    MissingPropertyName, InvalidPropertyName, InvalidPropertyAccess,
    MissingVariableName, InvalidMethodAccess, InvalidVariableAccess,
    InvalidReturnStatement, InvalidAssignment, UnresolvableCircularity>;

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
  VariantError variant_error;

  bool compile_and_execute();

private:
  std::unique_ptr<Machine> machine;
};
} // namespace Manadrain
