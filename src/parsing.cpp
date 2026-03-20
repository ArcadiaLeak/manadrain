
module manadrain:parsing;

import :prelude;
import :utility;
import :unicode;

namespace JS {
  struct VarDef {
    Atom var_name;
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

    std::size_t func_name;

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
  bool match_identifier(
    utility::PaddedBuf& buf, std::size_t idx,
    std::string rhs
  ) {
    std::string lhs = std::ranges::to<std::string>(
      std::ranges::subrange(
        std::next(buf.begin(), idx),
        std::next(buf.begin(), idx + rhs.size())
      )
    );
    
    if (lhs == rhs) return !unicode::is_id_continue_byte(
      *std::next(buf.begin(), idx + rhs.size())
    );

    return false;
  }
  
  Trigraph lexeme_next_simple(
    utility::PaddedBuf& buf,
    std::size_t& begin_idx,
    bool no_line_feed
  ) {
    using namespace unicode;
    std::size_t idx = begin_idx;
    std::uint32_t ch = buf[idx];

    while (true) {
      ch = buf[idx++];
      switch (ch) {
        case '\r': case '\n':
        if (no_line_feed)
          return {0, 0, '\n'};
        continue;
        
        case ' ': case '\t': case '\v': case '\f':
        continue;

        case '/':
        if (buf[idx] == '/') {
          if (no_line_feed)
            return {0, 0, '\n'};
          auto cch = buf[idx];
          while (cch && cch != '\r' && cch != '\n')
            idx++;
          continue;
        }
        if (buf[idx] == '*') {
          while (buf[++idx]) {
            auto cch = buf[idx];
            if ((cch == '\r' || cch == '\n') && no_line_feed)
              return {0, 0, '\n'};
            if (cch == '*' && buf[idx + 1] == '/')
              { idx += 2; break; }
          }
          continue;
        }
        break;

        case '=': if (buf[idx] == '>')
          return {0, '=', '>'};
        break;

        case 'i': if (match_identifier(buf, idx, "n"))
          return {0, 'i', 'n'};
        if (match_identifier(buf, idx, "mport")) {
          begin_idx = idx + 5;
          return {'i', 'm', 'p'};
        }
        return {'i', 'd', 'e'};

        case 'o': if (match_identifier(buf, idx, "f"))
          return {0, 'o', 'f'};
        return {'i', 'd', 'e'};

        case 'e': if (match_identifier(buf, idx, "xport"))
          return {'e', 'x', 'p'};
        return {'i', 'd', 'e'};

        case 'f': if (match_identifier(buf, idx, "unction"))
          return {'f', 'u', 'n'};
        return {'i', 'd', 'e'};
        
        case '\\': if (buf[idx] == 'u') {
          if (is_id_start_byte(parse_escape(buf, idx)))
            return {'i', 'd', 'e'};
        }
        break;

        default: if (ch >= 128) {
          ch = parse_uchar(buf, idx - 1, idx);
          if (no_line_feed && (ch == CP_PS || ch == CP_LS))
            return {0, 0, '\n'};
        }
        if (std::isspace(ch))
          continue;
        if (is_id_start_byte(ch))
          return {'i', 'd', 'e'};
        break;
      }
      return {0, 0, ch};
    }
  }
}

namespace JS {
  struct ParseState {
    std::shared_ptr<Context> ctx;
    std::string filename;
    bool got_line_feed;

    std::size_t lexeme_at;
    LexemeVar lexeme;
    
    utility::PaddedBuf buf;
    std::size_t before_at;
    std::size_t lastly_at;
    std::size_t input_sz;

    std::shared_ptr<FunctionDef> cur_func;
    bool is_module;

    std::size_t push_scope();

    void emit_op(OP val);
    void emit_u32(std::uint64_t val);

    int parse_source_element();
    int parse_statement_or_decl(std::bitset<8> decl_mask);
    LexemeVar parse_string(std::int32_t sep, std::size_t& idx);
    void parse_program();

    bool lexeme_is_static_import();
    bool lexeme_is_async_func();
    bool lexeme_is_pseudo_keyword(std::string_view atom);

    Trigraph lexeme_peek(bool no_line_feed);
    void lexeme_next();
  };
}
