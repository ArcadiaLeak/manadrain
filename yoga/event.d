module yoga.event;

import core.atomic;
import std.traits;
import yoga;

enum LayoutType : int {
  kLayout = 0,
  kMeasure = 1,
  kCachedLayout = 2,
  kCachedMeasure = 3
}

enum LayoutPassReason : int {
  kInitial = 0,
  kAbsLayout = 1,
  kStretch = 2,
  kMultilineStretch = 3,
  kFlexLayout = 4,
  kMeasureChild = 5,
  kAbsMeasureChild = 6,
  kFlexMeasure = 7
}

struct LayoutData {
  int layouts = 0;
  int measures = 0;
  uint maxMeasureCache = 0;
  int cachedLayouts = 0;
  int cachedMeasures = 0;
  int measureCallbacks = 0;
  int[EnumMembers!LayoutPassReason.length]
    measureCallbackReasonsCount;
};

private {
  immutable struct SubscriberNode {
    Event.Subscriber subscriber;
    SubscriberNode* next;
  }

  shared(SubscriberNode*) subscribers = null;

  SubscriberNode* push(SubscriberNode* newHead) @safe {
    SubscriberNode* oldHead = null;

    do {
      oldHead = subscribers.atomicLoad!(MemoryOrder.raw);
      if (newHead !is null) {
        newHead = new SubscriberNode(
          newHead.subscriber,
          oldHead
        );
      }
    } while (
      casWeak!
        (MemoryOrder.rel, MemoryOrder.raw)
        (&subscribers, oldHead, newHead) != true
    );

    return oldHead;
  }
}

struct Event {
  enum Type {
    NodeAllocation,
    NodeDeallocation,
    NodeLayout,
    LayoutPassStart,
    LayoutPassEnd,
    MeasureCallbackStart,
    MeasureCallbackEnd,
    NodeBaselineStart,
    NodeBaselineEnd,
  }

  alias Subscriber = void delegate(const Node, Type, Data);

  const struct Data {
    private void* data_;

    this(Type E)(in TypedData!E data) {
      data_ = &data;
    }
  }

  static void reset() {
    push(null);
  }

  const struct TypedData(Type E) {}

  const struct TypedData(Type E : Type.LayoutPassEnd) {
    LayoutData* layoutData;
  }

  const struct TypedData(Type E : Type.NodeLayout) {
    LayoutType layoutType;
  }

  const struct TypedData(Type E : Type.MeasureCallbackEnd) {
    float width;
    MeasureMode widthMeasureMode;
    float height;
    MeasureMode heightMeasureMode;
    float measuredWidth;
    float measuredHeight;
    const LayoutPassReason reason;
  }

  const struct TypedData(Type E : Type.NodeAllocation) {
    const(Config*) config;
  }

  const struct TypedData(Type E : Type.NodeDeallocation) {
    const(Config*) config;
  }

  static void subscribe(Subscriber subscriber) {
    push(new SubscriberNode(subscriber));
  }

  static void publish(Type E)(
    Node node,
    in TypedData!E eventData = TypedData!E()
  ) {
    publish(node, E, Data().__ctor!E(eventData));
  }

  private static void publish(
    const Node node,
    Type eventType,
    in Data eventData
  ) {
    for (
      auto subscriber = subscribers.atomicLoad!(MemoryOrder.raw);
      subscriber != null;
      subscriber = subscriber.next
    ) {
      subscriber.subscriber(node, eventType, eventData);
    }
  }
}