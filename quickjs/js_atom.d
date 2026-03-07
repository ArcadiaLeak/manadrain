module quickjs.js_atom;
import quickjs;

class JSAtom {
  const uint katom;
  JSAtomTree.Color clratom;
  JSAtom latom;
  JSAtom ratom;
  JSAtom parent_atom;
}

class JSAtomStr : JSAtom {
  const uint kstr;
  JSAtomTree.Color clrstr;
  JSAtomStr lstr;
  JSAtomStr rstr;
  JSAtomStr parent_str;
}

struct JSAtomTree {
  enum Color { RED, BLACK };

  JSAtom root_atom;
  JSAtomStr root_str;

  static struct JSAtomFwd {
    JSAtom node_;
    
    JSAtom front() => node_;
    bool empty() => node_ is null;

    void popFront() {
      assert(node_ !is null);
      if (node_.ratom !is null) {
        node_ = minatom(node_.ratom);
      } else {
        JSAtom p = node_.parent_atom;
        while (p !is null && node_ is p.ratom) {
          node_ = p;
          p = p.parent_atom;
        }
        node_ = p;
      }
    }

    JSAtomFwd save() => this;
  }

  static struct JSAtomRew {
    JSAtom node_;
    
    JSAtom front() => node_;
    bool empty() => node_ is null;

    void popFront() {
      assert(node_ !is null);
      if (node_.latom !is null) {
        node_ = maxatom(node_.latom);
      } else {
        JSAtom p = node_.parent_atom;
        while (p !is null && node_ is p.latom) {
          node_ = p;
          p = p.parent_atom;
        }
        node_ = p;
      }
    }

    JSAtomRew save() => this;
  }

  static struct JSStringFwd {
    JSAtomStr node_;
    
    JSAtomStr front() => node_;
    bool empty() => node_ is null;

    void popFront() {
      assert(node_ !is null);
      if (node_.rstr !is null) {
        node_ = minstr(node_.rstr);
      } else {
        JSAtomStr p = node_.parent_str;
        while (p !is null && node_ is p.rstr) {
          node_ = p;
          p = p.parent_str;
        }
        node_ = p;
      }
    }

    JSStringFwd save() => this;
  }

  static struct JSStringRew {
    JSAtomStr node_;
    
    JSAtomStr front() => node_;
    bool empty() => node_ is null;

    void popFront() {
      assert(node_ !is null);
      if (node_.lstr !is null) {
        node_ = maxstr(node_.lstr);
      } else {
        JSAtomStr p = node_.parent_str;
        while (p !is null && node_ is p.lstr) {
          node_ = p;
          p = p.parent_str;
        }
        node_ = p;
      }
    }

    JSStringRew save() => this;
  }

  static struct JSStringEqu {
    JSAtomStr begin_;
    const JSAtomStr end_;
    
    JSAtomStr front() => begin_;
    bool empty() => begin_ is end_;

    void popFront() {
      assert(begin_ !is null);
      if (begin_.rstr !is null) {
        begin_ = minstr(begin_.rstr);
      } else {
        JSAtomStr p = begin_.parent_str;
        while (p !is null && begin_ is p.rstr) {
          begin_ = p;
          p = p.parent_str;
        }
        begin_ = p;
      }
    }

    JSStringEqu save() => this;
  }

  static JSAtom minatom(JSAtom x) {
    if (!x) return null;
    while (x.latom) x = x.latom;
    return x;
  }

  static JSAtom maxatom(JSAtom x) {
    if (!x) return null;
    while (x.ratom) x = x.ratom;
    return x;
  }

  JSAtomFwd fwdatom() => JSAtomFwd(minatom(root_atom));
  JSAtomRew rewatom() => JSAtomRew(maxatom(root_atom));
  JSAtom getatom(uint katom) {
    JSAtom cur = root_atom;
    while (cur) {
      if (katom < cur.katom) {
        cur = cur.latom;
      } else if (katom > cur.katom) {
        cur = cur.ratom;
      } else {
        return cur;
      }
    }
    return null;
  }

  static JSAtomStr minstr(JSAtomStr x) {
    if (!x) return null;
    while (x.lstr) x = x.lstr;
    return x;
  }

  static JSAtomStr maxstr(JSAtomStr x) {
    if (!x) return null;
    while (x.rstr) x = x.rstr;
    return x;
  }

  JSStringFwd fwdstr() => JSStringFwd(minstr(root_str));
  JSStringRew rewstr() => JSStringRew(maxstr(root_str));
  JSStringEqu equstr(uint kstr) =>
    JSStringEqu(lbound_str(kstr), ubound_str(kstr));

  JSAtomStr lbound_str(uint kstr) {
    JSAtomStr x = root_str;
    JSAtomStr y = null;
    while (x) {
      if (kstr <= x.kstr) {
        y = x;
        x = x.lstr;
      } else {
        x = x.rstr;
      }
    }
    return y;
  }

  JSAtomStr ubound_str(uint kstr) {
    JSAtomStr x = root_str;
    JSAtomStr y = null;
    while (x) {
      if (kstr < x.kstr) {
        y = x;
        x = x.lstr;
      } else {
        x = x.rstr;
      }
    }
    return y;
  }

  void insert_str(JSAtomStr z) {
    JSAtomStr y = null;
    JSAtomStr x = root_str;
    while (x) {
      y = x;
      if (z.kstr < x.kstr)
        x = x.lstr;
      else
        x = x.rstr;
    }
    z.parent_str = y;
    if (y is null) {
      root_str = z;
    } else if (z.kstr < y.kstr) {
      y.lstr = z;
    } else {
      y.rstr = z;
    }
    insert_str_fixup(z);
  }

  void insert_str_fixup(JSAtomStr z) {
    while (z.parent_str && z.parent_str.clrstr == Color.RED) {
      if (z.parent_str is z.parent_str.parent_str.lstr) {
        JSAtomStr y = z.parent_str.parent_str.rstr;
        if (y && y.clrstr == Color.RED) {
          z.parent_str.clrstr = Color.BLACK;
          y.clrstr = Color.BLACK;
          z.parent_str.parent_str.clrstr = Color.RED;
          z = z.parent_str.parent_str;
        } else {
          if (z is z.parent_str.rstr) {
            z = z.parent_str;
            lstr_rotate(z);
          }
          z.parent_str.clrstr = Color.BLACK;
          z.parent_str.parent_str.clrstr = Color.RED;
          rstr_rotate(z.parent_str.parent_str);
        }
      } else {
        JSAtomStr y = z.parent_str.parent_str.lstr;
        if (y && y.clrstr == Color.RED) {
          z.parent_str.clrstr = Color.BLACK;
          y.clrstr = Color.BLACK;
          z.parent_str.parent_str.clrstr = Color.RED;
          z = z.parent_str.parent_str;
        } else {
          if (z is z.parent_str.lstr) {
            z = z.parent_str;
            rstr_rotate(z);
          }
          z.parent_str.clrstr = Color.BLACK;
          z.parent_str.parent_str.clrstr = Color.RED;
          lstr_rotate(z.parent_str.parent_str);
        }
      }
    }
    root_str.clrstr = Color.BLACK;
  }

  void lstr_rotate(JSAtomStr x) {
    JSAtomStr y = x.rstr;
    x.rstr = y.lstr;
    if (y.lstr) y.lstr.parent_str = x;
    y.parent_str = x.parent_str;
    if (x.parent_str is null) {
      root_str = y;
    } else if (x is x.parent_str.lstr) {
      x.parent_str.lstr = y;
    } else {
      x.parent_str.rstr = y;
    }
    y.lstr = x;
    x.parent_str = y;
  }

  void rstr_rotate(JSAtomStr y) {
    JSAtomStr x = y.lstr;
    y.lstr = x.rstr;
    if (x.rstr) x.rstr.parent_str = y;
    x.parent_str = y.parent_str;
    if (y.parent_str is null) {
      root_str = x;
    } else if (y is y.parent_str.lstr) {
      y.parent_str.lstr = x;
    } else {
      y.parent_str.rstr = x;
    }
    x.rstr = y;
    y.parent_str = x;
  }

  void insert_atom(JSAtom z) {
    JSAtom y = null;
    JSAtom x = root_atom;
    while (x) {
      y = x;
      assert(z.katom != x.katom);
      if (z.katom < x.katom)
        x = x.latom;
      else
        x = x.ratom;
    }
    z.parent_atom = y;
    if (y is null) {
      root_atom = z;
    } else if (z.katom < y.katom) {
      y.latom = z;
    } else {
      y.ratom = z;
    }
    insert_atom_fixup(z);
  }

  void insert_atom_fixup(JSAtom z) {
    while (z.parent_atom && z.parent_atom.clratom == Color.RED) {
      if (z.parent_atom is z.parent_atom.parent_atom.latom) {
        JSAtom y = z.parent_atom.parent_atom.ratom;
        if (y && y.clratom == Color.RED) {
          z.parent_atom.clratom = Color.BLACK;
          y.clratom = Color.BLACK;
          z.parent_atom.parent_atom.clratom = Color.RED;
          z = z.parent_atom.parent_atom;
        } else {
          if (z is z.parent_atom.ratom) {
            z = z.parent_atom;
            latom_rotate(z);
          }
          z.parent_atom.clratom = Color.BLACK;
          z.parent_atom.parent_atom.clratom = Color.RED;
          ratom_rotate(z.parent_atom.parent_atom);
        }
      } else {
        JSAtom y = z.parent_atom.parent_atom.latom;
        if (y && y.clratom == Color.RED) {
          z.parent_atom.clratom = Color.BLACK;
          y.clratom = Color.BLACK;
          z.parent_atom.parent_atom.clratom = Color.RED;
          z = z.parent_atom.parent_atom;
        } else {
          if (z is z.parent_atom.latom) {
            z = z.parent_atom;
            ratom_rotate(z);
          }
          z.parent_atom.clratom = Color.BLACK;
          z.parent_atom.parent_atom.clratom = Color.RED;
          latom_rotate(z.parent_atom.parent_atom);
        }
      }
    }
    root_atom.clratom = Color.BLACK;
  }

  void latom_rotate(JSAtom x) {
    JSAtom y = x.ratom;
    x.ratom = y.latom;
    if (y.latom) y.latom.parent_atom = x;
    y.parent_atom = x.parent_atom;
    if (x.parent_atom is null) {
      root_atom = y;
    } else if (x is x.parent_atom.latom) {
      x.parent_atom.latom = y;
    } else {
      x.parent_atom.ratom = y;
    }
    y.latom = x;
    x.parent_atom = y;
  }

  void ratom_rotate(JSAtom y) {
    JSAtom x = y.latom;
    y.latom = x.ratom;
    if (x.ratom) x.ratom.parent_atom = y;
    x.parent_atom = y.parent_atom;
    if (y.parent_atom is null) {
      root_atom = x;
    } else if (y is y.parent_atom.latom) {
      y.parent_atom.latom = x;
    } else {
      y.parent_atom.ratom = x;
    }
    x.ratom = y;
    y.parent_atom = x;
  }

  void erase_str(JSAtomStr z) {
    JSAtomStr y = z;
    JSAtomStr x;
    JSAtomStr x_parent;
    Color y_original_color = y.clrstr;

    if (z.lstr is null) {
      x = z.rstr;
      transplant_str(z, z.rstr);
      x_parent = z.parent_str;
    } else if (z.rstr is null) {
      x = z.lstr;
      transplant_str(z, z.lstr);
      x_parent = z.parent_str;
    } else {
      y = minstr(z.rstr);
      y_original_color = y.clrstr;
      x = y.rstr;
      if (y.parent_str is z) {
        x_parent = y;
      } else {
        x_parent = y.parent_str;
        transplant_str(y, y.rstr);
        y.rstr = z.rstr;
        y.rstr.parent_str = y;
      }
      transplant_str(z, y);
      y.lstr = z.lstr;
      y.lstr.parent_str = y;
      y.clrstr = z.clrstr;
    }

    if (y_original_color == Color.BLACK) {
      delstr_fixup(x, x_parent);
    }
  }

  void transplant_str(JSAtomStr u, JSAtomStr v) {
    if (u.parent_str is null) {
      root_str = v;
    } else if (u is u.parent_str.lstr) {
      u.parent_str.lstr = v;
    } else {
      u.parent_str.rstr = v;
    }
    if (v) v.parent_str = u.parent_str;
  }

  void delstr_fixup(JSAtomStr x, JSAtomStr parent_str) {
    while (x !is root_str && (x is null || x.clrstr == Color.BLACK)) {
      if (x is parent_str.lstr) {
        JSAtomStr w = parent_str.rstr;
        if (w && w.clrstr == Color.RED) {
          w.clrstr = Color.BLACK;
          parent_str.clrstr = Color.RED;
          lstr_rotate(parent_str);
          w = parent_str.rstr;
        }
        if (w is null || (
          (w.lstr is null || w.lstr.clrstr == Color.BLACK) &&
          (w.rstr is null || w.rstr.clrstr == Color.BLACK)
        )) {
          if (w) w.clrstr = Color.RED;
          x = parent_str;
          parent_str = x.parent_str;
        } else {
          if (w && (w.rstr is null || w.rstr.clrstr == Color.BLACK)) {
            if (w.lstr) w.lstr.clrstr = Color.BLACK;
            w.clrstr = Color.RED;
            rstr_rotate(w);
            w = parent_str.rstr;
          }
          if (w) {
            w.clrstr = parent_str.clrstr;
            parent_str.clrstr = Color.BLACK;
            if (w.rstr) w.rstr.clrstr = Color.BLACK;
            lstr_rotate(parent_str);
          }
          x = root_str;
        }
      } else {
        JSAtomStr w = parent_str.lstr;
        if (w && w.clrstr == Color.RED) {
          w.clrstr = Color.BLACK;
          parent_str.clrstr = Color.RED;
          rstr_rotate(parent_str);
          w = parent_str.lstr;
        }
        if (w is null || (
          (w.lstr is null || w.lstr.clrstr == Color.BLACK) &&
          (w.rstr is null || w.rstr.clrstr == Color.BLACK)
        )) {
          if (w) w.clrstr = Color.RED;
          x = parent_str;
          parent_str = x.parent_str;
        } else {
          if (w && (w.lstr is null || w.lstr.clrstr == Color.BLACK)) {
            if (w.rstr) w.rstr.clrstr = Color.BLACK;
            w.clrstr = Color.RED;
            lstr_rotate(w);
            w = parent_str.lstr;
          }
          if (w) {
            w.clrstr = parent_str.clrstr;
            parent_str.clrstr = Color.BLACK;
            if (w.lstr) w.lstr.clrstr = Color.BLACK;
            rstr_rotate(parent_str);
          }
          x = root_str;
        }
      }
    }
    if (x) x.clrstr = Color.BLACK;
  }
}
