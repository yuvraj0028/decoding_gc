#include "gc.hpp"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

using gc::Heap;
using gc::Object;
using gc::Root;
using gc::Visitor;

class Node final : public Object {
public:
  explicit Node(std::string name) : name_(std::move(name)) {}

  ~Node() override {
    std::cout << "  [sweep] " << name_ << " freed\n";
  }

  const std::string& name() const { return name_; }

  void addChild(Node* child) { children_.push_back(child); }

  void trace(Visitor& visitor) const override {
    for (Node* child : children_) {
      visitor.visit(child);
    }
  }

private:
  std::string name_;
  std::vector<Node*> children_;
};

static void printStats(const char* label, const Heap& heap) {
  std::cout << label << "\n"
            << "  live=" << heap.liveObjects()
            << "  allocated=" << heap.allocations()
            << "  collected=" << heap.collected()
            << "  collections=" << heap.collections() << "\n\n";
}

int main() {
  std::cout << "== Mark-and-sweep GC demo ==\n\n";
  Heap heap;

  std::cout << "[1] Build a reachable graph and collect\n";
  {
    Root<Node> root(heap, heap.allocate<Node>("root"));
    Node* a = heap.allocate<Node>("a");
    Node* b = heap.allocate<Node>("b");
    Node* c = heap.allocate<Node>("c");

    root->addChild(a);
    root->addChild(b);
    a->addChild(c);

    printStats("  before collect #1", heap);
    heap.collect();
    printStats("  after collect #1 (root is alive)", heap);

    std::cout << "[2] Drop the last root, then collect again\n";
  }

  printStats("  before collect #2", heap);
  heap.collect();
  printStats("  after collect #2 (graph is unreachable)", heap);

  std::cout << "[3] Cycles without external roots are collected too\n";
  {
    Root<Node> a(heap, heap.allocate<Node>("cycle-a"));
    Root<Node> b(heap, heap.allocate<Node>("cycle-b"));
    a->addChild(b.get());
    b->addChild(a.get());

    heap.collect();
    printStats("  after collect #3 (cycle is still rooted)", heap);
  }

  heap.collect();
  printStats("  after collect #4 (cycle is garbage)", heap);

  return 0;
}
