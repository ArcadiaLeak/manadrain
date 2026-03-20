
export module manadrain:prelude;
import :utility;

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
    String(std::string_view str_view)
      : data{utility::shared_str(str_view)}, size{str_view.size()} {}
    String() = default;

    std::string_view view() const {
      return std::string_view{data.get(), size};
    }

    bool operator==(const String& other) const {
      return view() == other.view();
    }

    private:
    std::shared_ptr<const char[]> data;
    std::size_t size;
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
  struct Atom {
    String str;
    ATOM_TYPE atom_type;
  };
}

export namespace JS {
  using Trigraph = std::array<std::uint32_t, 3>;

  enum class Unit {
    TAG_NULL,
    TAG_UNDEFINED,
    TAG_UNINITIALIZED,
    TAG_FALSE,
    TAG_TRUE,
    TAG_EXCEPTION
  };

  using Value = std::variant<Unit, std::weak_ptr<HeapVal>>;
  bool operator==(
    const std::weak_ptr<HeapVal>& lhs,
    const std::weak_ptr<HeapVal>& rhs
  ) { return lhs.lock() == rhs.lock(); }
}

namespace JS {
  struct LexemeStr {
    Value str;
    std::int32_t sep;
    bool operator==(const LexemeStr& other) const = default; 
  };

  struct LexemeNum {
    Value num;
    bool operator==(const LexemeNum& other) const = default; 
  };

  struct LexemeIde {
    String str;
    bool has_escape;
    bool is_reserved;
    bool operator==(const LexemeIde& other) const = default; 
  };

  struct LexemeRxp {
    Value body;
    Value flags;
    bool operator==(const LexemeRxp& other) const = default; 
  };

  using LexemeVar = std::variant<
    Trigraph, LexemeStr, LexemeNum, LexemeIde, LexemeRxp
  >;
}
