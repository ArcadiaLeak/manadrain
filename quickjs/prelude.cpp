
export module quickjs:prelude;
import :utility;
import std;

namespace JS {
  enum class OP {
    enter_scope
  };

  enum class ATOM_TYPE {
    STRING,
    GLOBAL_SYMBOL,
    SYMBOL,
    PRIVATE,
  };
}

namespace JS {
  constexpr std::bitset<8> EVAL_TYPE_GLOBAL = 0;
  constexpr std::bitset<8> EVAL_TYPE_MODULE = 1;
  constexpr std::bitset<8> EVAL_TYPE_DIRECT = 2;
  constexpr std::bitset<8> EVAL_TYPE_INDIRECT = 3;
  constexpr std::bitset<8> EVAL_TYPE_MASK = 3;

  constexpr std::bitset<8> EVAL_FLAG_STRICT = 1 << 3;

  constexpr std::bitset<8> MODE_STRICT = 1 << 0;
  constexpr std::bitset<8> MODE_ASYNC = 1 << 2;

  constexpr std::bitset<8> DECL_MASK_FUNC = 1 << 0;
  constexpr std::bitset<8> DECL_MASK_FUNC_WITH_LABEL = 1 << 1;
  constexpr std::bitset<8> DECL_MASK_OTHER = 1 << 2;
  constexpr std::bitset<8> DECL_MASK_ALL = DECL_MASK_FUNC |
    DECL_MASK_FUNC_WITH_LABEL | DECL_MASK_OTHER;

  constexpr std::size_t PROP_INITIAL_SIZE = 2;
}

export namespace JS {
  struct String {
    const std::shared_ptr<const char[]> data;
    const std::size_t size;
    
    String(std::string_view str_view)
      : data{utility::shared_str(str_view)}, size{str_view.size()} {}

    std::string_view view() const {
      return std::string_view{data.get(), size};
    }

    bool operator==(const String& other) const {
      return view() == other.view();
    }
  };
}

export namespace JS {
  struct Object;
  struct Context;
  struct Runtime;
}

namespace JS {
  struct HeapVal : std::enable_shared_from_this<HeapVal> {
    std::int32_t ref_count = 1;

    HeapVal() = default;
    HeapVal(const HeapVal&) = default;
    HeapVal& operator=(const HeapVal&) = default;
    HeapVal(HeapVal&&) noexcept = default;
    HeapVal& operator=(HeapVal&&) noexcept = default;
    virtual ~HeapVal() = default;
    
    virtual std::shared_ptr<Object> asObject() { return nullptr; }
    virtual std::shared_ptr<Context> asContext() { return nullptr; }
  };
}

namespace JS {
  struct AtomStruct {
    String str;
    ATOM_TYPE atom_type;
    std::size_t idx = -1;

    AtomStruct(const AtomStruct&) = default;
    AtomStruct& operator=(const AtomStruct&) = default;
    AtomStruct(AtomStruct&&) noexcept = default;
    AtomStruct& operator=(AtomStruct&&) noexcept = default;
    virtual ~AtomStruct() = default;

    AtomStruct(String str, ATOM_TYPE atom_type)
      : str{str}, atom_type{atom_type} {}
  };

  struct Atom : AtomStruct, HeapVal {
    Atom(String str, ATOM_TYPE atom_type)
      : AtomStruct{str, atom_type} {}
  };
}

export namespace JS {
  enum class Unit {
    TAG_NULL,
    TAG_UNDEFINED,
    TAG_UNINITIALIZED,
    TAG_FALSE,
    TAG_TRUE,
    TAG_EXCEPTION
  };

  using Value = std::variant<Unit, std::weak_ptr<HeapVal>>;
}

namespace JS {
  struct TokString {
    Value str;
    std::int32_t sep;
  };

  struct TokNumber {
    Value num;
  };

  struct TokIdent {
    std::weak_ptr<Atom> str;
    bool has_escape;
    bool is_reserved;
  };

  struct TokRegexp {
    Value body;
    Value flags;
  };

  using TokVariant = std::variant<
    std::int32_t, TokString, TokNumber, TokIdent, TokRegexp
  >;
}