module quickjs.js_atom_tree;
import quickjs;

import std.typecons;

struct AtomTree(Atom) {
  enum Color { RED, BLACK };
  static class Node {
    Tuple!(const uint, Atom) data;
    Color color;
    Node left;
    Node right;
    Node parent;

    this(uint key, Atom value) {
      data = tuple(key, value);
      color = Color.RED;
    }
  }

  Node root;
  size_t size_;

  static struct FwdRange {
    Node node_;
    
    Atom front() => node_.data[1];
    bool empty() => node_ is null;

    void popFront() {
      assert(node_ !is null);
      if (node_.right !is null) {
        node_ = minimum(node_.right);
      } else {
        Node p = node_.parent;
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
    Node node_;
    
    Atom front() => node_.data[1];
    bool empty() => node_ is null;

    void popFront() {
      assert(node_ !is null);
      if (node_.left !is null) {
        node_ = maximum(node_.left);
      } else {
        Node p = node_.parent;
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
    Node begin_;
    const Node end_;
    
    Atom front() => begin_.data[1];
    bool empty() => begin_ is end_;

    void popFront() {
      assert(begin_ !is null);
      if (begin_.right !is null) {
        begin_ = minimum(begin_.right);
      } else {
        Node p = begin_.parent;
        while (p !is null && begin_ is p.right) {
          begin_ = p;
          p = p.parent;
        }
        begin_ = p;
      }
    }

    EquRange save() => this;
  }

  static Node minimum(Node x) {
    if (!x) return null;
    while (x.left) x = x.left;
    return x;
  }

  static const(Node) minimum(const Node x, Node ret = null) {
    if (!x) return ret;
    return minimum(x.left);
  }

  static Node maximum(Node x) {
    if (!x) return null;
    while (x.right) x = x.right;
    return x;
  }

  static const(Node) maximum(const Node x, Node ret = null) {
    if (!x) return ret;
    return maximum(x.right);
  }

  FwdRange fwd() => FwdRange(minimum(root));
  RewRange rew() => RewRange(maximum(root));

  EquRange equ(uint key) =>
    EquRange(lower_bound_impl(key), upper_bound_impl(key));

  bool empty() => size_ == 0;
  size_t size() => size_;

  FwdRange insert(uint key, Atom value) {
    Node new_node = new Node(key, value);
    insert_node(new_node);
    return FwdRange(new_node);
  }

  FwdRange erase(FwdRange pos) {
    assert(!pos.empty);
    Node node = pos.node_;
    pos.popFront;
    erase_node(node);
    return pos;
  }

  Node lower_bound_impl(uint key) {
    Node x = root;
    Node y = null;
    while (x) {
      if (key <= x.data[0]) {
        y = x;
        x = x.left;
      } else {
        x = x.right;
      }
    }
    return y;
  }

  Node upper_bound_impl(uint key) {
    Node x = root;
    Node y = null;
    while (x) {
      if (key < x.data[0]) {
        y = x;
        x = x.left;
      } else {
        x = x.right;
      }
    }
    return y;
  }

  void insert_node(Node z) {
    Node y = null;
    Node x = root;
    while (x) {
      y = x;
      if (z.data[0] < x.data[0])
        x = x.left;
      else
        x = x.right;
    }
    z.parent = y;
    if (y is null) {
      root = z;
    } else if (z.data[0] < y.data[0]) {
      y.left = z;
    } else {
      y.right = z;
    }
    insert_fixup(z);
    ++size_;
  }

  void insert_fixup(Node z) {
    while (z.parent && z.parent.color == Color.RED) {
      if (z.parent is z.parent.parent.left) {
        Node y = z.parent.parent.right;
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
        Node y = z.parent.parent.left;
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

  void left_rotate(Node x) {
    Node y = x.right;
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

  void right_rotate(Node y) {
    Node x = y.left;
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

  void erase_node(Node z) {
    Node y = z;
    Node x;
    Node x_parent;
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

  void transplant(Node u, Node v) {
    if (u.parent is null) {
      root = v;
    } else if (u is u.parent.left) {
      u.parent.left = v;
    } else {
      u.parent.right = v;
    }
    if (v) v.parent = u.parent;
  }

  void delete_fixup(Node x, Node parent) {
    while (x !is root && (x is null || x.color == Color.BLACK)) {
      if (x is parent.left) {
        Node w = parent.right;
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
        Node w = parent.left;
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

unittest {
  AtomTree!int map;

  assert(map.empty);
  assert(map.size == 0);

  auto it1 = map.insert(10, 100);
  assert(map.size == 1);
  assert(!map.empty);
  assert(it1.front == 100);


  auto it2 = map.insert(5, 50);
  assert(map.size == 2);
  assert(it2.front == 50);

  auto f1 = map.equ(10);
  assert(!f1.empty);
  assert(f1.front == 100);

  auto f2 = map.equ(5);
  assert(!f2.empty);
  assert(f2.front == 50);

  auto f3 = map.equ(42);
  assert(f3.empty);
}

unittest {
  import std.array;
  AtomTree!string map;

  map.insert(1, "one");
  map.insert(2, "two");
  map.insert(1, "uno");
  map.insert(1, "eins");
  map.insert(3, "three");

  assert(map.size == 5);

  string[] expected = ["one", "uno", "eins", "two", "three"];
  assert(map.fwd.array == expected);
}

unittest {
  import std.array;
  AtomTree!int map;

  map.insert(10, 1);
  map.insert(20, 2);
  map.insert(10, 3);
  map.insert(30, 4);
  map.insert(10, 5);
  map.insert(20, 6);
  map.insert(40, 7);

  auto range10 = map.equ(10);
  int[] expected10 = [1, 3, 5];
  assert(range10.array == expected10);

  auto range20 = map.equ(20);
  int[] expected20 = [2, 6];
  assert(range20.array == expected20);

  auto range30 = map.equ(30);
  int[] expected30 = [4];
  assert(range30.array == expected30);

  auto range99 = map.equ(99);
  assert(range99.begin_ is range99.end_);
}

unittest {
  AtomTree!int map;
  foreach (i; 0..10)
    map.insert(i % 3, i);

  assert(map.size == 10);

  auto it = map.fwd;
  assert(it.front == 0);
  it = map.erase(it);
  assert(map.size == 9);
  assert(!it.empty);
  assert(it.front == 3);

  auto last = map.rew;
  assert(last.front == 8);
  auto erased = map.erase(AtomTree!int.FwdRange(last.node_));
  assert(erased.empty);
  assert(map.size == 8);

  import std.array;
  assert(map.fwd.array == [3, 6, 9, 1, 4, 7, 2, 5]);
}
