export module manadrain;

import :parsing;
export import :prelude;
import :unicode;
import :utility;

export namespace std {
  template<>
  struct hash<JS::String> {
    std::size_t operator()(const JS::String& str) const noexcept {
      return std::hash<std::string_view>{}(str.view());
    }
  };
}

export namespace JS {
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

export namespace JS {
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

export namespace JS {
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
    std::unordered_map<String, std::size_t> atom_hash;
    std::vector<Atom> atom_array;
    std::stack<std::size_t> atom_free_idx;

    std::map<Trigraph, String> class_rb;
    std::uint32_t max_class_id;

    std::list<std::weak_ptr<Context>> context_list;
    std::list<std::shared_ptr<HeapVal>> gc_obj_list;

    std::size_t NewAtom(String str, ATOM_TYPE atom_type);

    void NewClass(Trigraph class_id, String class_name);

    void add_gc_object(std::shared_ptr<HeapVal> heap_val) {
      gc_obj_list.push_back(heap_val);
    }
  };

  struct Context : HeapVal {
    std::weak_ptr<Runtime> rt;
    std::map<Trigraph, Value> class_proto;

    Context(std::shared_ptr<Runtime> rt) : rt{rt} {}

    int AddIntrinsicBasicObjects();

    std::shared_ptr<Context> asContext() override {
      return std::static_pointer_cast<Context>(shared_from_this());
    }
  };
}

export namespace JS {
  void Runtime::NewClass(Trigraph class_id, String class_name) {
    class_rb[class_id] = class_name;
  }
}

export namespace JS {
  std::shared_ptr<HeapVal> DupValue(std::shared_ptr<HeapVal> heap_val) {
    heap_val->ref_count++;
    return heap_val;
  }
}

export namespace JS {
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

export namespace JS {
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

namespace JS {
  bool ParseState::lexeme_is_pseudo_keyword(std::string_view word) {
    if (not std::holds_alternative<LexemeIde>(lexeme))
      return false;

    std::shared_ptr rt = ctx->rt.lock();
    std::size_t atom_idx = rt->NewAtom(word, ATOM_TYPE::STRING);
    return std::get<LexemeIde>(lexeme).str == rt->atom_array[atom_idx].str;
  }

  std::size_t ParseState::push_scope() {
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
    emit_op(OP::enter_scope);
    emit_u32(scope_idx);
    return scope_idx;
  }

  void ParseState::emit_op(OP val) {
    cur_func->last_opcode_pos = cur_func->byte_code.size();
    cur_func->byte_code.push_back(std::to_underlying(val));
  }

  void ParseState::emit_u32(std::uint64_t val) {
    cur_func->byte_code.push_back(val);
  }

  void ParseState::parse_program() {
    lexeme_next();

    while (lexeme != LexemeVar{Trigraph{'e', 'o', 'f'}})
      parse_source_element();
  }

  int ParseState::parse_source_element() {
    if (
      lexeme == LexemeVar{Trigraph{'f', 'u', 'n'}} ||
      lexeme_is_async_func()
    )
      return -1;
    else if (
      not cur_func->module_def.expired() &&
      lexeme == LexemeVar{Trigraph{'e', 'x', 'p'}}
    )
      return -1;
    else if (
      not cur_func->module_def.expired() &&
      lexeme_is_static_import()
    )
      return -1;
    else
      return parse_statement_or_decl(DECL_MASK_ALL);
  }

  int ParseState::parse_statement_or_decl(std::bitset<8> decl_mask) {
    return -1;
  }

  bool ParseState::lexeme_is_static_import() {
    if (lexeme != LexemeVar{Trigraph{'i', 'm', 'p'}})
      return false;
    Trigraph peeked = lexeme_peek(false);
    return (
      peeked != Trigraph{0, 0, '('} &&
      peeked != Trigraph{0, 0, '.'}
    );
  }

  bool ParseState::lexeme_is_async_func() {
    return (
      lexeme_is_pseudo_keyword("async") &&
      lexeme_peek(true) == Trigraph{'f', 'u', 'n'}
    );
  }

  Trigraph ParseState::lexeme_peek(bool no_line_feed) {
    return lexeme_next_simple(
      buf, lastly_at, no_line_feed
    );
  }

  LexemeVar ParseState::parse_string(
    std::int32_t delim, std::size_t& idx
  ) {
    std::int32_t ch = buf[idx];
    std::string parsed{};

    while (true) {
      if (idx >= input_sz)
        goto invalid_char;

      ch = buf[idx]; idx++;
      if (ch == delim) break;

      if (ch == '$' && buf[idx] == '{' && delim == '`')
        { idx++; break; }
      
      if (ch == '\\') {
        std::size_t idx_escape = idx - 1;
        ch = buf[idx];

        switch (ch) {
          case '\0': if (idx >= input_sz)
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
    }

    invalid_char: throw;
  }

  void ParseState::lexeme_next() {
    std::size_t idx = lastly_at;
    std::uint32_t ch = buf[idx];

    before_at = lastly_at;
    got_line_feed = false;

    LexemeVar discovered = [&]() -> LexemeVar {
      redo: lexeme_at = lastly_at;
      ch = buf[idx];

      switch (ch) {
        case 0:
        if (idx >= input_sz)
          return Trigraph{'e', 'o', 'f'};
        else
          goto def_lexeme;
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
        return parse_string(ch, ++idx);

        default: def_lexeme: return Trigraph{0, 0, ch};
        idx++; break;
      }
    }();

    lexeme = discovered;
    lastly_at = idx;
  }
}

export namespace JS {
  std::size_t Runtime::NewAtom(String str, ATOM_TYPE atom_type) {
    if (atom_type == ATOM_TYPE::STRING) {
      auto hash_entry = atom_hash.find(str);
      if (hash_entry != atom_hash.end()) {
        return hash_entry->second;
      }
    }

    Atom ret = Atom{str, atom_type};
    std::size_t taken_idx{};

    if (atom_free_idx.empty()) {
      taken_idx = atom_array.size();
      atom_array.push_back(ret);
    } else {
      taken_idx = atom_free_idx.top();
      atom_array[taken_idx] = ret;
      atom_free_idx.pop();
    }

    if (atom_type == ATOM_TYPE::STRING)
      atom_hash[str] = taken_idx;

    return taken_idx;
  }
}

export namespace JS {
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

export namespace JS {
  int Context::AddIntrinsicBasicObjects() {
    // class_proto[CLASS_OBJECT] = NewObjectProtoClassAlloc(
    //   asContext(), Unit::TAG_NULL, CLASS_OBJECT, Object_proto_funcs.size()
    // );

    return -1;
  }
}

export namespace JS {
  std::shared_ptr<Context> NewContext(std::shared_ptr<Runtime> rt) {
    std::shared_ptr ctx = std::make_shared<Context>(rt);
    rt->add_gc_object(ctx);
    rt->context_list.push_back(ctx);
    return ctx;
  }
}

export namespace JS {
  Value EvalInternal(
    std::shared_ptr<Context> ctx,
    Value this_obj,
    std::string input,
    std::string filename,
    std::bitset<8> flags,
    std::int64_t scope_idx = -1
  ) {
    std::bitset<8> js_mode = 0;
    std::size_t input_sz = input.size();

    ParseState state{
      .ctx = ctx,
      .filename = filename,
      .buf = std::ranges::concat_view{
        std::ranges::owning_view{std::move(input)},
        std::ranges::repeat_view{'\0'}
      },
      .input_sz = input_sz
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
    func_def.func_name = ctx->rt.lock()->NewAtom(String{"<eval>"}, ATOM_TYPE::STRING);
    state.is_module = false;
    state.push_scope();
    func_def.body_scope = func_def.scope_level;
    
    state.parse_program();
    throw std::exception{};
  }
}
