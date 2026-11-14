#include "gc.hpp"

#include <cassert>
#include <iostream>

namespace {

class Node final : public gc::Object {
public:
  gc::Object* child = nullptr;

  void trace(gc::Visitor& visitor) const override {
    if (child != nullptr) {
      visitor.visit(child);
    }
  }
};

void reachable_objects_survive() {
  gc::Heap heap;
  gc::Root<Node> root(heap, heap.allocate<Node>());
  root->child = heap.allocate<Node>();

  heap.collect();

  assert(heap.liveObjects() == 2);
  assert(heap.collected() == 0);
  assert(heap.collections() == 1);
}

void unreachable_objects_are_freed() {
  gc::Heap heap;
  {
    gc::Root<Node> root(heap, heap.allocate<Node>());
    root->child = heap.allocate<Node>();
  }

  assert(heap.liveObjects() == 2);

  heap.collect();

  assert(heap.liveObjects() == 0);
  assert(heap.collected() == 2);
}

void objects_reachable_through_a_child_survive() {
  gc::Heap heap;
  gc::Root<Node> parent(heap, heap.allocate<Node>());
  {
    gc::Root<Node> child(heap, heap.allocate<Node>());
    parent->child = child.get();
  }

  heap.collect();

  assert(heap.liveObjects() == 2);
  assert(heap.collected() == 0);
}

void cycles_are_collected() {
  gc::Heap heap;
  {
    gc::Root<Node> a(heap, heap.allocate<Node>());
    gc::Root<Node> b(heap, heap.allocate<Node>());
    a->child = b.get();
    b->child = a.get();

    heap.collect();
    assert(heap.liveObjects() == 2);
  }

  heap.collect();

  assert(heap.liveObjects() == 0);
  assert(heap.collected() == 2);
}

void mark_bits_are_reset_after_each_collection() {
  gc::Heap heap;
  gc::Root<Node> root(heap, heap.allocate<Node>());

  heap.collect();
  heap.collect();

  assert(heap.liveObjects() == 1);
  assert(heap.collected() == 0);
  assert(heap.collections() == 2);
}

void moved_roots_keep_objects_alive() {
  gc::Heap heap;
  gc::Root<Node> first(heap, heap.allocate<Node>());
  gc::Root<Node> second(std::move(first));

  heap.collect();

  assert(heap.liveObjects() == 1);
}

} // namespace

int main() {
  reachable_objects_survive();
  unreachable_objects_are_freed();
  objects_reachable_through_a_child_survive();
  cycles_are_collected();
  mark_bits_are_reset_after_each_collection();
  moved_roots_keep_objects_alive();

  std::cout << "all GC tests passed\n";
  return 0;
}
