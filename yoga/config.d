module yoga.config;

import yoga;

alias YGCloneNodeFunc = Node function(
  const Node oldNode,
  const Node owner,
  size_t childIndex
);

Node YGNodeClone(const Node oldNode) {
  auto node = new Node(oldNode);
  Event.publish!(Event.Type.NodeAllocation)(
    node,
    Event.TypedData!(Event.Type.NodeAllocation)(
      node.getConfig
    )
  );
  node.setOwner = null;
  return node;
}

struct Config {
  private {
    YGCloneNodeFunc cloneNodeCallback_;

    uint version_ = 0;
    float pointScaleFactor_ = 1.0f;
  }

  float getPointScaleFactor() pure inout {
    return pointScaleFactor_;
  }

  void setPointScaleFactor(float pointScaleFactor) {
    if (pointScaleFactor_ != pointScaleFactor) {
      pointScaleFactor_ = pointScaleFactor;
      version_++;
    }
  }

  void setCloneNodeCallback(YGCloneNodeFunc cloneNode) {
    cloneNodeCallback_ = cloneNode;
  }

  Node cloneNode(
    const Node node,
    const Node owner,
    size_t childIndex
  ) const {
    Node clone = null;
    if (cloneNodeCallback_ !is null) {
      clone = cloneNodeCallback_(node, owner, childIndex);
    }
    if (clone is null) {
      // clone = 
    }
    return clone;
  }
}