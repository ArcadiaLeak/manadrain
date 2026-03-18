import std;

import quickjs.predef.atom;
import quickjs.predef.clazz;
import quickjs.predef.token;

namespace JS {
  enum class ATOM_TYPE {
    STRING,
    GLOBAL_SYMBOL,
    SYMBOL,
    PRIVATE,
  };

  enum class CFUNC {
    generic,
    generic_magic,
    constructor,
    constructor_magic,
    constructor_or_func,
    constructor_or_func_magic,
    f_f,
    f_f_f,
    getter,
    setter,
    getter_magic,
    setter_magic,
    iterator_next,
  };
}

enum {
  OP_enter_scope
};

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

namespace unicode {
  constexpr std::uint8_t UTF8_CHAR_LEN_MAX = 6;
  constexpr std::int32_t CP_LS = 0x2028;
  constexpr std::int32_t CP_PS = 0x2029;
}

namespace JS {
  struct Context;
  struct Object;
  
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
  struct Atom : HeapVal {
    std::string str;
    ATOM_TYPE atom_type;
    std::int64_t idx = -1;

    Atom(
      std::string str, ATOM_TYPE atom_type
    ): str{str}, atom_type{atom_type} {}
  };
}

namespace JS {
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

namespace JS {
  struct VarDef {
    std::weak_ptr<Atom> var_name;
  };

  struct VarScope {
    std::size_t parent;
    std::size_t first;
  };

  struct FunctionDef {
    std::weak_ptr<Context> ctx;
    std::int64_t parent = -1;

    bool is_eval;
    bool is_global_var;
    bool is_func_expr;

    std::bitset<8> eval_type;
    bool new_target_allowed;
    bool super_call_allowed;
    bool super_allowed;
    bool arguments_allowed;
    std::bitset<8> js_mode;

    std::weak_ptr<Atom> func_name;

    std::vector<VarDef> vars;
    std::size_t eval_ret_idx;

    std::size_t scope_level;
    std::size_t scope_first;
    std::vector<VarScope> scopes;
    std::size_t body_scope;

    std::deque<std::uint64_t> byte_code;
    std::size_t last_opcode_pos;

    std::weak_ptr<struct ModuleDef> module_def;
  }; 
}

namespace JS {
  struct Shape : HeapVal {
    std::weak_ptr<Object> proto;
    std::size_t prop_size;
  };

  std::shared_ptr<Object> get_proto_obj(Value proto_val) {
    if (proto_val.index() == 1)
      return std::get<1>(proto_val).lock()->asObject();
    return nullptr;
  }
}

namespace JS {
  struct Property {};

  struct Object : HeapVal {
    std::int32_t class_id;
    std::weak_ptr<Shape> shape;
    std::vector<Property> prop;
    std::bitset<8> flags;

    Object(std::shared_ptr<Shape> sh, std::int32_t class_id)
      : shape{sh}, class_id{class_id}, prop{sh->prop_size} {}

    std::shared_ptr<Object> asObject() override {
      return std::static_pointer_cast<Object>(shared_from_this());
    }
  };

  struct Array : Object {
    Array(std::shared_ptr<Shape> sh, std::int32_t class_id)
      : Object{sh, class_id} {}
  };

  struct Runtime {
    std::unordered_map<std::string, std::weak_ptr<Atom>> atom_hash;
    std::vector<std::shared_ptr<Atom>> atom_array;
    std::stack<std::size_t> atom_free_idx;

    std::vector<std::weak_ptr<Atom>> class_array;

    std::list<std::weak_ptr<Context>> context_list;
    std::list<std::shared_ptr<HeapVal>> gc_obj_list;

    std::map<std::weak_ptr<Object>, std::weak_ptr<Shape>> shape_hash;

    std::shared_ptr<Atom> NewAtom(std::string str, ATOM_TYPE atom_type);
    std::shared_ptr<Atom> DupAtom(std::size_t idx);

    void NewClass(std::int32_t class_id, std::shared_ptr<Atom> class_name);
    
    void init_atom_range();
    void init_class_range(std::span<const std::int32_t> tab, std::int32_t start);

    void add_gc_object(std::shared_ptr<HeapVal> heap_val) {
      gc_obj_list.push_back(heap_val);
    }
  };

  struct Context : HeapVal {
    std::weak_ptr<Runtime> rt;
    std::vector<Value> class_proto;

    Context(std::shared_ptr<Runtime> rt)
      : rt{rt}, class_proto{rt->class_array.size()} {}

    int AddIntrinsicBasicObjects();
    
    std::shared_ptr<Atom> DupAtom(std::size_t idx);

    std::shared_ptr<Context> asContext() override {
      return std::static_pointer_cast<Context>(shared_from_this());
    }
  };
}

namespace JS {
  void Runtime::NewClass(
    std::int32_t class_id, std::shared_ptr<Atom> class_name
  ) {
    if (class_id >= class_array.size()) {
      class_array.resize(
        std::max<std::size_t>(
          CLASS_INIT_COUNT,
          std::max<std::size_t>(class_id + 1, class_array.size() * 3 / 2)
        )
      );
    
      for (std::weak_ptr ctx : context_list) {
        ctx.lock()->class_proto.resize(class_array.size());
      }
    }
    class_array.at(class_id) = class_name;
  }
}

namespace JS {
  std::shared_ptr<HeapVal> DupValue(std::shared_ptr<HeapVal> heap_val) {
    heap_val->ref_count++;
    return heap_val;
  }
}

namespace JS {
  Value NewObjectFromShape(
    std::shared_ptr<Context> ctx,
    std::shared_ptr<Shape> sh,
    std::int32_t class_id
  ) {
    std::shared_ptr<Object> obj = nullptr;
    switch (class_id) {
      default: obj = std::make_shared<Object>(sh, class_id);
      break;
    }
    ctx->rt.lock()->add_gc_object(obj);
    return obj;
  }
}

namespace JS {
  std::shared_ptr<Shape> new_shape_nohash(
    std::shared_ptr<Context> ctx,
    std::shared_ptr<Object> proto,
    std::size_t prop_size
  ) {
    std::shared_ptr sh = std::make_shared<Shape>();
    ctx->rt.lock()->add_gc_object(sh);
    if (proto) DupValue(proto);
    sh->proto = proto;
    sh->prop_size = prop_size;
    return sh;
  }

  Value NewObjectProtoClassAlloc(
    std::shared_ptr<Context> ctx, Value proto_val,
    std::int32_t class_id, std::size_t n_alloc_props
  ) {
    std::shared_ptr proto = get_proto_obj(proto_val);
    std::shared_ptr sh = new_shape_nohash(ctx, proto, n_alloc_props);
    if (not sh)
      return Unit::TAG_EXCEPTION;
    return NewObjectFromShape(ctx, sh, class_id);
  }
}

namespace common {
  using PaddedBuf = std::ranges::concat_view<
    std::ranges::owning_view<std::string>,
    std::ranges::repeat_view<char>
  >;
}

namespace unicode {
  std::int32_t from_hex(std::int32_t c) {
    if (c >= '0' && c <= '9')
      return c - '0';
    else if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    else
      return -1;
  }

  bool is_hi_surrogate(std::uint32_t c) {
    return (c >> 10) == (0xD800 >> 10); // 0xD800-0xDBFF
  }

  bool is_lo_surrogate(std::uint32_t c) {
    return (c >> 10) == (0xDC00 >> 10); // 0xDC00-0xDFFF
  }

  std::uint32_t from_surrogate(std::uint32_t hi, std::uint32_t lo) {
    return 0x10000 + 0x400 * (hi - 0xD800) + (lo - 0xDC00);
  }
}

namespace lre {
  using namespace unicode;

  bool is_id_start_byte(std::int32_t c) {
    return std::isalpha(c) || c == '$' || c == '_';
  }

  bool is_id_continue_byte(std::int32_t c) {
    return std::isalnum(c) || c == '$' || c == '_';
  }

  std::int32_t parse_escape(
    common::PaddedBuf& buf, std::size_t& begin_idx,
    int allow_utf16
  ) {
    auto p = std::next(buf.begin(), begin_idx);
    std::uint32_t c = *p++;

    switch(c) {
      case 'b': c = '\b';
      break;

      case 'f': c = '\f';
      break;

      case 'n': c = '\n';
      break;

      case 'r': c = '\r';
      break;

      case 't': c = '\t';
      break;

      case 'v': c = '\v';
      break;

      case 'x': {
        std::int32_t h0 = from_hex(*p++);
        if (h0 < 0)
          return -1;
        std::int32_t h1 = from_hex(*p++);
        if (h1 < 0)
          return -1;
        c = (h0 << 4) | h1;
      }
      break;

      case 'u': {
        std::int32_t h, i;

        if (*p == '{' && allow_utf16) {
          p++; c = 0;
          while (true) {
            h = from_hex(*p++);
            if (h < 0)
              return -1;
            c = (c << 4) | h;
            if (c > 0x10FFFF)
              return -1;
            if (*p == '}')
              break;
          }
          p++;
        } else {
          c = 0;
          for(i = 0; i < 4; i++) {
            h = from_hex(*p++);
            if (h < 0) {
              return -1;
            }
            c = (c << 4) | h;
          }
          if (
            is_hi_surrogate(c) && allow_utf16 == 2 &&
            p[0] == '\\' && p[1] == 'u'
          ) {
            std::uint32_t c1 = 0;
            for(i = 0; i < 4; i++) {
              h = from_hex(p[2 + i]);
              if (h < 0)
                break;
              c1 = (c1 << 4) | h;
            }
            if (i == 4 && is_lo_surrogate(c1)) {
              p += 6;
              c = from_surrogate(c, c1);
            }
          }
        }
      }
      break;

      case '0': case '1': case '2': case '3':
      case '4': case '5': case '6': case '7':
      c -= '0';
      if (allow_utf16 == 2) {
        if (c != 0 || std::isdigit(*p))
          return -1;
      } else {
        std::uint32_t v = *p - '0';
        if (v > 7)
          break;
        c = (c << 3) | v;
        p++;
        if (c >= 32)
          break;
        v = *p - '0';
        if (v > 7)
          break;
        c = (c << 3) | v;
        p++;
      }
      break;

      default: return -2;
    }

    begin_idx = std::distance(buf.begin(), p);
    return c;
  }
}

namespace unicode {
  constexpr std::uint32_t utf8_min_code[5] = {
    0x80, 0x800, 0x10000, 0x00200000, 0x04000000,
  };

  constexpr std::uint8_t utf8_first_code_mask[5] = {
    0x1f, 0xf, 0x7, 0x3, 0x1,
  };

  std::int32_t from_utf8(
    common::PaddedBuf& buf, std::size_t begin_idx,
    std::int32_t max_len, std::size_t& end_idx
  ) {
    auto p = std::next(buf.begin(), begin_idx);
    std::int32_t l, c = *p++;
    if (c < 0x80) {
      end_idx = std::distance(buf.begin(), p);
      return c;
    }
    switch(c) {
      case 0xc0: case 0xc1: case 0xc2: case 0xc3:
      case 0xc4: case 0xc5: case 0xc6: case 0xc7:
      case 0xc8: case 0xc9: case 0xca: case 0xcb:
      case 0xcc: case 0xcd: case 0xce: case 0xcf:
      case 0xd0: case 0xd1: case 0xd2: case 0xd3:
      case 0xd4: case 0xd5: case 0xd6: case 0xd7:
      case 0xd8: case 0xd9: case 0xda: case 0xdb:
      case 0xdc: case 0xdd: case 0xde: case 0xdf:
      l = 1; break;

      case 0xe0: case 0xe1: case 0xe2: case 0xe3:
      case 0xe4: case 0xe5: case 0xe6: case 0xe7:
      case 0xe8: case 0xe9: case 0xea: case 0xeb:
      case 0xec: case 0xed: case 0xee: case 0xef:
      l = 2; break;

      case 0xf0: case 0xf1: case 0xf2: case 0xf3:
      case 0xf4: case 0xf5: case 0xf6: case 0xf7:
      l = 3; break;

      case 0xf8: case 0xf9: case 0xfa: case 0xfb:
      l = 4; break;

      case 0xfc: case 0xfd:
      l = 5; break;

      default: return -1;
    }
    if (l > (max_len - 1))
      return -1;
    c &= utf8_first_code_mask[l - 1];
    for (std::int32_t i = 0; i < l; i++) {
      std::int32_t b = *p++;
      if (b < 0x80 || b >= 0xc0)
        return -1;
      c = (c << 6) | (b & 0x3f);
    }
    if (c < utf8_min_code[l - 1])
      return -1;
    end_idx = std::distance(buf.begin(), p);
    return c;
  }
}

namespace JS {
  bool match_identifier(
    common::PaddedBuf& buf, std::size_t idx,
    std::string rhs
  ) {
    std::string lhs = std::ranges::to<std::string>(
      std::ranges::subrange(
        std::next(buf.begin(), idx),
        std::next(buf.begin(), idx + rhs.size())
      )
    );
    
    if (lhs == rhs) return !lre::is_id_continue_byte(
      *std::next(buf.begin(), idx + rhs.size())
    );

    return false;
  }
  
  std::int32_t simple_next_token(
    common::PaddedBuf& buf,
    std::size_t& begin_idx,
    bool no_line_feed
  ) {
    std::size_t idx = begin_idx;
    std::uint32_t ch = buf[idx];

    while (true) {
      ch = buf[idx++];
      switch (ch) {
        case '\r': case '\n':
        if (no_line_feed)
          return '\n';
        continue;
        
        case ' ': case '\t': case '\v': case '\f':
        continue;

        case '/':
        if (buf[idx] == '/') {
          if (no_line_feed)
            return '\n';
          auto cch = buf[idx];
          while (cch && cch != '\r' && cch != '\n')
            idx++;
          continue;
        }
        if (buf[idx] == '*') {
          while (buf[++idx]) {
            auto cch = buf[idx];
            if ((cch == '\r' || cch == '\n') && no_line_feed)
              return '\n';
            if (cch == '*' && buf[idx + 1] == '/')
              { idx += 2; break; }
          }
          continue;
        }
        break;

        case '=': if (buf[idx] == '>')
          return TOK_ARROW;
        break;

        case 'i': if (match_identifier(buf, idx, "n"))
          return TOK_IN;
        if (match_identifier(buf, idx, "mport")) {
          begin_idx = idx + 5;
          return TOK_IMPORT;
        }
        return TOK_IDENT;

        case 'o': if (match_identifier(buf, idx, "f"))
          return TOK_OF;
        return TOK_IDENT;

        case 'e': if (match_identifier(buf, idx, "xport"))
          return TOK_EXPORT;
        return TOK_IDENT;

        case 'f': if (match_identifier(buf, idx, "unction"))
          return TOK_FUNCTION;
        return TOK_IDENT;
        
        case '\\': if (buf[idx] == 'u') {
          if (lre::is_id_start_byte(lre::parse_escape(buf, idx, true)))
            return TOK_IDENT;
        }
        break;

        default: if (ch >= 128) {
          using namespace unicode;
          ch = from_utf8(buf, idx - 1, UTF8_CHAR_LEN_MAX, idx);
          if (no_line_feed && (ch == CP_PS || ch == CP_LS))
            return '\n';
        }
        if (std::isspace(ch))
          continue;
        if (lre::is_id_start_byte(ch))
          return TOK_IDENT;
        break;
      }
      return ch;
    }
  }
}

namespace JS {
  struct ParseState {
    std::shared_ptr<Context> ctx;
    std::string filename;
    bool got_line_feed;

    std::size_t token_idx;
    TokVariant token;
    
    common::PaddedBuf buf;
    std::size_t last_idx;
    std::size_t curr_idx;
    std::size_t buf_size;

    std::shared_ptr<FunctionDef> cur_func;
    bool is_module;

    std::size_t push_scope() {
      if (not cur_func)
        return 0;

      FunctionDef& fd = *cur_func;
      VarScope scope{
        .parent = fd.scope_level,
        .first = fd.scope_first
      };
      std::uint64_t scope_idx = fd.scopes.size();
      fd.scopes.emplace_back(scope);
      fd.scope_level = scope_idx;
      emit_op(OP_enter_scope);
      emit_u32(scope_idx);
      return scope_idx;
    }

    void emit_op(std::uint64_t val) {
      cur_func->last_opcode_pos = cur_func->byte_code.size();
      cur_func->byte_code.push_back(val);
    }

    void emit_u32(std::uint64_t val) {
      cur_func->byte_code.push_back(val);
    }

    int parse_program() {
      if (next_token())
        return -1;

      while (token.index() != 0 || std::get<0>(token) != TOK_EOF) {
        if (parse_source_element())
          return -1;
      }

      return 0;
    }

    int parse_source_element() {
      if (
        token.index() == 0 && std::get<0>(token) == TOK_FUNCTION ||
        token_is_async_func()
      )
        return -1;
      else if (
        not cur_func->module_def.expired() &&
        token.index() == 0 && std::get<0>(token) == TOK_EXPORT
      )
        return -1;
      else if (
        not cur_func->module_def.expired() &&
        token_is_static_import()
      )
        return -1;
      else
        return parse_statement_or_decl(DECL_MASK_ALL);
    }

    int parse_statement_or_decl(std::bitset<8> decl_mask) {
      return -1;
    }

    bool token_is_static_import() {
      if (token.index() != 0 || std::get<0>(token) != TOK_IMPORT)
        return false;
      std::int32_t tok = peek_token(false);
      return tok != '(' && tok != '.';
    }

    bool token_is_async_func() {
      return (
        token_is_pseudo_keyword(ATOM_async) &&
        peek_token(true) == TOK_FUNCTION
      );
    }

    bool token_is_pseudo_keyword(std::size_t atom) {
      return std::holds_alternative<TokIdent>(token) &&
        std::get<TokIdent>(token).str.lock() == ctx->rt.lock()->atom_array[atom] &&
        std::get<TokIdent>(token).has_escape;
    }

    std::int32_t peek_token(bool no_line_feed) {
      return simple_next_token(
        buf, curr_idx, no_line_feed
      );
    }

    int parse_string(
      std::int32_t sep, bool do_throw, std::size_t idx,
      TokVariant& token, std::size_t& idxref
    ) {
      auto ch = buf[idx];
      auto strbuf = std::string{""};

      while (true) {
        if (idx >= buf_size)
          goto invalid_char;

        ch = buf[idx]; idx++;
        if (ch == sep) break;

        if (ch == '$' && buf[idx] == '{' && sep == '`')
          { idx++; break; }
        
        if (ch == '\\') {
          std::size_t idx_escape = idx - 1;
          ch = buf[idx];

          switch (ch) {
            case '\0': if (idx >= buf_size)
              goto invalid_char;
            idx++; break;

            case '\'': case '\"': case '\\':
            idx++; break;

            case '\r':
            if (buf[idx + 1] == '\n')
              idx++;
            [[fallthrough]];

            case '\n': idx++;
            continue;

            default: break;
          }
        }

        strbuf.push_back(ch);
      }

      invalid_char: throw;
    }

    int parse_error(std::size_t offset, std::string message) {
      throw;
    }

    int next_token() {
      auto idx = curr_idx;
      auto ch = buf[idx];

      last_idx = curr_idx;
      got_line_feed = false;

      redo: token_idx = curr_idx;
      ch = buf[idx];

      switch (ch) {
        case 0:
        if (idx >= buf_size)
          token = TOK_EOF;
        else
          goto def_token;
        break;

        case '\r':
        if (buf[idx + 1] == '\n')
          idx++;
        [[fallthrough]];

        case '\n': idx++;
        line_feed: got_line_feed = true;
        goto redo;

        case '\f': case '\v': case ' ': case '\t': idx++;
        goto redo;

        case '\'': case '\"':
        if (parse_string(ch, true, idx + 1, token, idx))
          goto fail;
        break;

        default: def_token: token = ch;
        idx++; break;
      }

      curr_idx = idx;
      return 0;

      fail: token = TOK_ERROR;
      return -1;
    }
  };
}

namespace JS {
  void Runtime::init_atom_range() {
    for (std::size_t i = 0; i < atom_init.size(); i++) {
      ATOM_TYPE atom_type;
      if (i == ATOM_Private_brand)
        atom_type = ATOM_TYPE::PRIVATE;
      else if (i >= ATOM_Symbol_toPrimitive)
        atom_type = ATOM_TYPE::SYMBOL;
      else
        atom_type = ATOM_TYPE::STRING;
      NewAtom(std::string{atom_init[i]}, atom_type);
    }
  }

  std::shared_ptr<Atom> Runtime::NewAtom(std::string str, ATOM_TYPE atom_type) {
    if (atom_type == ATOM_TYPE::STRING) {
      auto atom_iter = atom_hash.find(str);
      if (
        atom_iter != atom_hash.end() &&
        not atom_iter->second.expired()
      ) {
        return atom_iter->second.lock();
      }
    }

    std::shared_ptr ret = std::make_shared<Atom>(str, atom_type);

    if (atom_free_idx.empty()) {
      ret->idx = atom_array.size();
      atom_array.push_back(ret);
    } else {
      ret->idx = atom_free_idx.top();
      atom_free_idx.pop();
      atom_array[ret->idx] = ret;
    }

    if (atom_type == ATOM_TYPE::STRING)
      atom_hash[str] = ret;

    return ret;
  }
}

namespace JS {
  constexpr bool atom_is_const(std::size_t idx) {
    return idx > ATOM_END;
  }

  std::shared_ptr<Atom> Runtime::DupAtom(std::size_t idx) {
    std::shared_ptr<Atom> ret = atom_array[idx];
    if (not atom_is_const(idx))
      ret->ref_count++;
    return ret;
  }

  std::shared_ptr<Atom> Context::DupAtom(std::size_t idx) {
    return rt.lock()->DupAtom(idx);
  }
}

namespace JS {
  constexpr std::array Object_proto_funcs = std::to_array<std::string_view>({
    "toString",
    "toLocaleString",
    "valueOf",
    "hasOwnProperty",
    "isPrototypeOf",
    "propertyIsEnumerable",
    "__proto__",
    "__defineGetter__",
    "__defineSetter__",
    "__lookupGetter__",
    "__lookupSetter__",
  });
}

namespace JS {
  int Context::AddIntrinsicBasicObjects() {
    class_proto[CLASS_OBJECT] = NewObjectProtoClassAlloc(
      asContext(), Unit::TAG_NULL, CLASS_OBJECT, Object_proto_funcs.size()
    );

    return -1;
  }
}

namespace JS {
  void Runtime::init_class_range(std::span<const std::int32_t> table, std::int32_t start) {
    for (int i = 0; i < table.size(); i++) {
      std::int32_t class_id = i + start;
      NewClass(class_id, DupAtom(table[i]));
    }
  }
}

namespace JS {
  std::shared_ptr<Context> NewContext(std::shared_ptr<Runtime> rt) {
    std::shared_ptr ctx = std::make_shared<Context>(rt);
    rt->add_gc_object(ctx);
    rt->context_list.push_back(ctx);
    return ctx;
  }
}

namespace JS {
  Value EvalInternal(
    std::shared_ptr<Context> ctx,
    Value this_obj,
    std::string input,
    std::string filename,
    std::bitset<8> flags,
    std::int64_t scope_idx = -1
  ) {
    std::bitset<8> js_mode = 0;

    ParseState state{
      .ctx = ctx,
      .filename = filename,
      .buf = std::ranges::concat_view{
        std::ranges::owning_view{std::string{input}},
        std::ranges::repeat_view{'\0'}
      },
      .buf_size = input.size()
    };

    std::bitset eval_type = flags & EVAL_TYPE_MASK;
    if (eval_type == EVAL_TYPE_DIRECT) {
      throw std::runtime_error("unimplemented!");
    } else {
      std::bitset eval_flag = flags & EVAL_FLAG_STRICT;
      if (eval_flag.any()) js_mode |= MODE_STRICT;
    }
    state.cur_func = std::make_shared<FunctionDef>(
      FunctionDef{
        .ctx = ctx,
        .is_eval = true,
        .is_func_expr = false
      }
    );
    FunctionDef& func_def = *state.cur_func;
    func_def.eval_type = eval_type;
    if (eval_type == EVAL_TYPE_DIRECT) {
      throw std::runtime_error("unimplemented!");
    } else {
      func_def.new_target_allowed = false;
      func_def.super_call_allowed = false;
      func_def.super_allowed = false;
      func_def.arguments_allowed = true;
    }
    func_def.js_mode = js_mode;
    func_def.func_name = ctx->DupAtom(ATOM__eval_);
    state.is_module = false;
    state.push_scope();
    func_def.body_scope = func_def.scope_level;
    state.parse_program();

    throw;
  }
}

std::string source_str(std::string filepath) {
  std::ifstream file{filepath};
  if (not file.is_open()) throw std::runtime_error{
    std::format("could not open file: {}", filepath)
  };

  return std::ranges::to<std::string>(
    std::ranges::subrange(
      std::istreambuf_iterator<char>{file},
      std::istreambuf_iterator<char>{}
    )
  );
}

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

  {
    using namespace JS;
    std::shared_ptr rt = std::make_shared<Runtime>();
    rt->init_atom_range();
    rt->init_class_range(std_class_def, CLASS_OBJECT);

    std::shared_ptr ctx = NewContext(rt);
    EvalInternal(
      ctx, Unit::TAG_NULL, source_str(filepath), "", 0
    );
  }
  
  return 0;
}
