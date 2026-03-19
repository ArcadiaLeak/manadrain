export module quickjs;

import :predef.atom;
import :predef.clazz;
import :predef.token;
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
    std::unordered_map<String, std::weak_ptr<Atom>> atom_hash;
    std::vector<std::shared_ptr<Atom>> atom_array;
    std::stack<std::size_t> atom_free_idx;

    std::vector<std::weak_ptr<Atom>> class_array;

    std::list<std::weak_ptr<Context>> context_list;
    std::list<std::shared_ptr<HeapVal>> gc_obj_list;

    std::map<std::weak_ptr<Object>, std::weak_ptr<Shape>> shape_hash;

    std::shared_ptr<Atom> NewAtom(String str, ATOM_TYPE atom_type);
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

export namespace JS {
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
  bool ParseState::token_is_pseudo_keyword(std::size_t atom) {
    return std::holds_alternative<TokIdent>(token) &&
      std::get<TokIdent>(token).str.lock() == ctx->rt.lock()->atom_array[atom] &&
      std::get<TokIdent>(token).has_escape;
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

  int ParseState::parse_program() {
    if (next_token())
      return -1;
    while (token.index() != 0 || std::get<0>(token) != TOK_EOF) {
      if (parse_source_element())
        return -1;
    }
    return 0;
  }

  int ParseState::parse_source_element() {
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

  int ParseState::parse_statement_or_decl(std::bitset<8> decl_mask) {
    return -1;
  }

  bool ParseState::token_is_static_import() {
    if (token.index() != 0 || std::get<0>(token) != TOK_IMPORT)
      return false;
    std::int32_t tok = peek_token(false);
    return tok != '(' && tok != '.';
  }

  bool ParseState::token_is_async_func() {
    return (
      token_is_pseudo_keyword(ATOM_async) &&
      peek_token(true) == TOK_FUNCTION
    );
  }

  std::int32_t ParseState::peek_token(bool no_line_feed) {
    return simple_next_token(
      buf, curr_idx, no_line_feed
    );
  }

  int ParseState::parse_string(
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

  int ParseState::parse_error(std::size_t offset, std::string message) {
    throw;
  }

  int ParseState::next_token() {
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
}

export namespace JS {
  void Runtime::init_atom_range() {
    for (std::size_t i = 0; i < atom_init.size(); i++) {
      ATOM_TYPE atom_type;
      if (i == ATOM_Private_brand)
        atom_type = ATOM_TYPE::PRIVATE;
      else if (i >= ATOM_Symbol_toPrimitive)
        atom_type = ATOM_TYPE::SYMBOL;
      else
        atom_type = ATOM_TYPE::STRING;
      NewAtom(atom_init[i], atom_type);
    }
  }

  std::shared_ptr<Atom> Runtime::NewAtom(String str, ATOM_TYPE atom_type) {
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

export namespace JS {
  constexpr bool atom_is_const(std::size_t idx) {
    return idx < ATOM_END;
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
    class_proto[CLASS_OBJECT] = NewObjectProtoClassAlloc(
      asContext(), Unit::TAG_NULL, CLASS_OBJECT, Object_proto_funcs.size()
    );

    return -1;
  }
}

export namespace JS {
  void Runtime::init_class_range(std::span<const std::int32_t> table, std::int32_t start) {
    for (int i = 0; i < table.size(); i++) {
      std::int32_t class_id = i + start;
      NewClass(class_id, DupAtom(table[i]));
    }
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
    const std::string& input,
    const std::string& filename,
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
