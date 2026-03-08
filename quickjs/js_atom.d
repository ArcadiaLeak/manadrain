module quickjs.js_atom;
import quickjs;

class JSAtom {
  JSAtom prev;
  JSAtom next;
  JSString as_string() => null;
}

class JSString : JSAtom {
  const wstring str;
  JSStrTree.Color color;
  JSString left;
  JSString right;
  JSString parent;

  this(wstring s) { str = s; }

  override JSString as_string() => this;
}

struct JSStrTree {
  enum Color { RED, BLACK };

  JSString root;
  size_t size_;

  static struct FwdRange {
    JSString node_;
    
    JSString front() => node_;
    bool empty() => node_ is null;

    void popFront() {
      assert(node_ !is null);
      if (node_.right !is null) {
        node_ = minimum(node_.right);
      } else {
        JSString p = node_.parent;
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
    JSString node_;
    
    JSString front() => node_;
    bool empty() => node_ is null;

    void popFront() {
      assert(node_ !is null);
      if (node_.left !is null) {
        node_ = maximum(node_.left);
      } else {
        JSString p = node_.parent;
        while (p !is null && node_ is p.left) {
          node_ = p;
          p = p.parent;
        }
        node_ = p;
      }
    }

    RewRange save() => this;
  }

  static JSString minimum(JSString x) {
    if (!x) return null;
    while (x.left) x = x.left;
    return x;
  }

  static JSString maximum(JSString x) {
    if (!x) return null;
    while (x.right) x = x.right;
    return x;
  }

  FwdRange fwd() => FwdRange(minimum(root));
  RewRange rew() => RewRange(maximum(root));

  bool empty() => size_ == 0;
  size_t size() => size_;

  JSString find(wstring str) {
    JSString cur = root;
    while (cur) {
      if (str < cur.str)
        cur = cur.left;
      else if (cur.str < str)
        cur = cur.right;
      else
        return cur;
    }
    return null;
  }

  void insert(JSString z) {
    JSString y = null;
    JSString x = root;
    while (x) {
      y = x;
      if (z.str < x.str)
        x = x.left;
      else if (z.str > x.str)
        x = x.right;
      else
        assert(0);
    }
    z.parent = y;
    if (y is null)
      root = z;
    else if (z.str < y.str)
      y.left = z;
    else
      y.right = z;
    insert_fixup(z);
    ++size_;
  }

  void insert_fixup(JSString z) {
    while (z.parent && z.parent.color == Color.RED) {
      if (z.parent is z.parent.parent.left) {
        JSString y = z.parent.parent.right;
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
        JSString y = z.parent.parent.left;
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

  void left_rotate(JSString x) {
    JSString y = x.right;
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

  void right_rotate(JSString y) {
    JSString x = y.left;
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

  void erase(JSString z) {
    JSString y = z;
    JSString x;
    JSString x_parent;
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

  void transplant(JSString u, JSString v) {
    if (u.parent is null) {
      root = v;
    } else if (u is u.parent.left) {
      u.parent.left = v;
    } else {
      u.parent.right = v;
    }
    if (v) v.parent = u.parent;
  }

  void delete_fixup(JSString x, JSString parent) {
    while (x !is root && (x is null || x.color == Color.BLACK)) {
      if (x is parent.left) {
        JSString w = parent.right;
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
        JSString w = parent.left;
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
