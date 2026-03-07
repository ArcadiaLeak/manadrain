module quickjs.js_atom_tree;
import quickjs;

struct JSAtomTree {
  enum Color { RED, BLACK };

  JSAtomClazz root;
  size_t size_;

  static struct FwdRange {
    JSAtomClazz node_;
    
    JSAtomClazz front() => node_;
    bool empty() => node_ is null;

    void popFront() {
      assert(node_ !is null);
      if (node_.right !is null) {
        node_ = minimum(node_.right);
      } else {
        JSAtomClazz p = node_.parent;
        while (p !is null && node_ is p.right) {
          node_ = p;
          p = p.parent;
        }
        node_ = p;
      }
    }

    FwdRange save() => this;
  }

  static struct RewRange {
    JSAtomClazz node_;
    
    JSAtomClazz front() => node_;
    bool empty() => node_ is null;

    void popFront() {
      assert(node_ !is null);
      if (node_.left !is null) {
        node_ = maximum(node_.left);
      } else {
        JSAtomClazz p = node_.parent;
        while (p !is null && node_ is p.left) {
          node_ = p;
          p = p.parent;
        }
        node_ = p;
      }
    }

    RewRange save() => this;
  }

  static struct EquRange {
    JSAtomClazz begin_;
    const JSAtomClazz end_;
    
    JSAtomClazz front() => begin_;
    bool empty() => begin_ is end_;

    void popFront() {
      assert(begin_ !is null);
      if (begin_.right !is null) {
        begin_ = minimum(begin_.right);
      } else {
        JSAtomClazz p = begin_.parent;
        while (p !is null && begin_ is p.right) {
          begin_ = p;
          p = p.parent;
        }
        begin_ = p;
      }
    }

    EquRange save() => this;
  }

  static JSAtomClazz minimum(JSAtomClazz x) {
    if (!x) return null;
    while (x.left) x = x.left;
    return x;
  }

  static JSAtomClazz maximum(JSAtomClazz x) {
    if (!x) return null;
    while (x.right) x = x.right;
    return x;
  }

  FwdRange fwd() => FwdRange(minimum(root));
  RewRange rew() => RewRange(maximum(root));

  EquRange equ(uint key) =>
    EquRange(lower_bound_impl(key), upper_bound_impl(key));

  bool empty() => size_ == 0;
  size_t size() => size_;

  FwdRange insert(JSAtomClazz new_node) {
    insert_node(new_node);
    return FwdRange(new_node);
  }

  FwdRange erase(FwdRange pos) {
    assert(!pos.empty);
    JSAtomClazz node = pos.node_;
    pos.popFront;
    erase_node(node);
    return pos;
  }

  JSAtomClazz lower_bound_impl(uint key) {
    JSAtomClazz x = root;
    JSAtomClazz y = null;
    while (x) {
      if (key <= x.key) {
        y = x;
        x = x.left;
      } else {
        x = x.right;
      }
    }
    return y;
  }

  JSAtomClazz upper_bound_impl(uint key) {
    JSAtomClazz x = root;
    JSAtomClazz y = null;
    while (x) {
      if (key < x.key) {
        y = x;
        x = x.left;
      } else {
        x = x.right;
      }
    }
    return y;
  }

  void insert_node(JSAtomClazz z) {
    JSAtomClazz y = null;
    JSAtomClazz x = root;
    while (x) {
      y = x;
      if (z.key < x.key)
        x = x.left;
      else
        x = x.right;
    }
    z.parent = y;
    if (y is null) {
      root = z;
    } else if (z.key < y.key) {
      y.left = z;
    } else {
      y.right = z;
    }
    insert_fixup(z);
    ++size_;
  }

  void insert_fixup(JSAtomClazz z) {
    while (z.parent && z.parent.color == Color.RED) {
      if (z.parent is z.parent.parent.left) {
        JSAtomClazz y = z.parent.parent.right;
        if (y && y.color == Color.RED) {
          z.parent.color = Color.BLACK;
          y.color = Color.BLACK;
          z.parent.parent.color = Color.RED;
          z = z.parent.parent;
        } else {
          if (z is z.parent.right) {
            z = z.parent;
            left_rotate(z);
          }
          z.parent.color = Color.BLACK;
          z.parent.parent.color = Color.RED;
          right_rotate(z.parent.parent);
        }
      } else {
        JSAtomClazz y = z.parent.parent.left;
        if (y && y.color == Color.RED) {
          z.parent.color = Color.BLACK;
          y.color = Color.BLACK;
          z.parent.parent.color = Color.RED;
          z = z.parent.parent;
        } else {
          if (z is z.parent.left) {
            z = z.parent;
            right_rotate(z);
          }
          z.parent.color = Color.BLACK;
          z.parent.parent.color = Color.RED;
          left_rotate(z.parent.parent);
        }
      }
    }
    root.color = Color.BLACK;
  }

  void left_rotate(JSAtomClazz x) {
    JSAtomClazz y = x.right;
    x.right = y.left;
    if (y.left) y.left.parent = x;
    y.parent = x.parent;
    if (x.parent is null) {
      root = y;
    } else if (x is x.parent.left) {
      x.parent.left = y;
    } else {
      x.parent.right = y;
    }
    y.left = x;
    x.parent = y;
  }

  void right_rotate(JSAtomClazz y) {
    JSAtomClazz x = y.left;
    y.left = x.right;
    if (x.right) x.right.parent = y;
    x.parent = y.parent;
    if (y.parent is null) {
      root = x;
    } else if (y is y.parent.left) {
      y.parent.left = x;
    } else {
      y.parent.right = x;
    }
    x.right = y;
    y.parent = x;
  }

  void erase_node(JSAtomClazz z) {
    JSAtomClazz y = z;
    JSAtomClazz x;
    JSAtomClazz x_parent;
    Color y_original_color = y.color;

    if (z.left is null) {
      x = z.right;
      transplant(z, z.right);
      x_parent = z.parent;
    } else if (z.right is null) {
      x = z.left;
      transplant(z, z.left);
      x_parent = z.parent;
    } else {
      y = minimum(z.right);
      y_original_color = y.color;
      x = y.right;
      if (y.parent is z) {
        x_parent = y;
      } else {
        x_parent = y.parent;
        transplant(y, y.right);
        y.right = z.right;
        y.right.parent = y;
      }
      transplant(z, y);
      y.left = z.left;
      y.left.parent = y;
      y.color = z.color;
    }

    --size_;

    if (y_original_color == Color.BLACK) {
      delete_fixup(x, x_parent);
    }
  }

  void transplant(JSAtomClazz u, JSAtomClazz v) {
    if (u.parent is null) {
      root = v;
    } else if (u is u.parent.left) {
      u.parent.left = v;
    } else {
      u.parent.right = v;
    }
    if (v) v.parent = u.parent;
  }

  void delete_fixup(JSAtomClazz x, JSAtomClazz parent) {
    while (x !is root && (x is null || x.color == Color.BLACK)) {
      if (x is parent.left) {
        JSAtomClazz w = parent.right;
        if (w && w.color == Color.RED) {
          w.color = Color.BLACK;
          parent.color = Color.RED;
          left_rotate(parent);
          w = parent.right;
        }
        if (w is null || (
          (w.left is null || w.left.color == Color.BLACK) &&
          (w.right is null || w.right.color == Color.BLACK)
        )) {
          if (w) w.color = Color.RED;
          x = parent;
          parent = x.parent;
        } else {
          if (w && (w.right is null || w.right.color == Color.BLACK)) {
            if (w.left) w.left.color = Color.BLACK;
            w.color = Color.RED;
            right_rotate(w);
            w = parent.right;
          }
          if (w) {
            w.color = parent.color;
            parent.color = Color.BLACK;
            if (w.right) w.right.color = Color.BLACK;
            left_rotate(parent);
          }
          x = root;
        }
      } else {
        JSAtomClazz w = parent.left;
        if (w && w.color == Color.RED) {
          w.color = Color.BLACK;
          parent.color = Color.RED;
          right_rotate(parent);
          w = parent.left;
        }
        if (w is null || (
          (w.left is null || w.left.color == Color.BLACK) &&
          (w.right is null || w.right.color == Color.BLACK)
        )) {
          if (w) w.color = Color.RED;
          x = parent;
          parent = x.parent;
        } else {
          if (w && (w.left is null || w.left.color == Color.BLACK)) {
            if (w.right) w.right.color = Color.BLACK;
            w.color = Color.RED;
            left_rotate(w);
            w = parent.left;
          }
          if (w) {
            w.color = parent.color;
            parent.color = Color.BLACK;
            if (w.left) w.left.color = Color.BLACK;
            right_rotate(parent);
          }
          x = root;
        }
      }
    }
    if (x) x.color = Color.BLACK;
  }
}
