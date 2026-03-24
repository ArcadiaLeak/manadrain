module yoga.node.layoutable_children;

import core.builtins;
import std.container.slist;
import std.typecons;
import yoga;

struct LayoutableChildren(T : Node) {
  struct Range {
    @property bool empty() {
      return node_ is null && childIndex_ == 0;
    }

    @property T front() {
      return node_.getChild(childIndex_);
    }

    void popFront() {
      if (childIndex_ + 1 >= node_.getChildCount) {
        if (backtrack_.empty.likely) {
          this = Range();
        } else {
          auto back = backtrack_.front;
          node_ = back[0];
          childIndex_ = back[1];
          backtrack_.removeFront();

          popFront();
        }
      } else {
        ++childIndex_;

        if (
          unlikely(
            node_.getChild(childIndex_).style.display ==
            Display.Contents
          )
        ) {
          skipContentsNodes();
        }
      }
    }

    private void skipContentsNodes() {
      T currentNode = node_.getChild(childIndex_);
      while (
        currentNode.style.display == Display.Contents &&
        currentNode.getChildCount > 0
      ) {
        backtrack_.insertFront(tuple(node_, childIndex_));
        node_ = currentNode;
        childIndex_ = 0;

        currentNode = currentNode.getChild(childIndex_);
      }

      if (currentNode.style.display == Display.Contents) {
        popFront();
      }
    }

    private {
      T node_ = null;
      size_t childIndex_ = 0;
      SList!(Tuple!(T, size_t)) backtrack_;
    }
  }

  this(T node) {
    node_ = node;
  }

  Range opSlice() {
    if (node_.getChildCount > 0) {
      auto result = Range(node_, 0);
      if (unlikely(node_.getChild(0).style.display == Display.Contents)) {
        result.skipContentsNodes();
      }
      return result;
    } else {
      return Range();
    }
  }

  private T node_;
}