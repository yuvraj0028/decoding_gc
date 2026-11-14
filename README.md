# Decoding Garbage Collection

A tiny, header-only **mark-and-sweep garbage collector** written in C++17.

The goal is not to replace a real GC. The goal is to make the algorithm
visible: roots, reachability, mark bits, sweep, and cycle collection in about
100 lines of readable code.

## Why build a GC?

C++ gives you deterministic memory management. That is a feature. But "GC
magic" in Java, Go, C#, JavaScript, Python, and Kotlin hides a simple idea:

> An object is alive if it can be reached from a root.

Everything else can be freed.

## How mark-and-sweep works

1. **Roots** — Start from known live references (stack handles, globals,
   registers).
2. **Mark** — Walk the object graph from each root. Set a mark bit on every
   object you visit.
3. **Sweep** — Walk the heap. Free every object whose mark bit is unset, then
   clear the bits on survivors for the next cycle.

```text
Before:                    Mark:                 Sweep:
root ──▶ A ──▶ B            root ──▶ A(*) ──▶ B(*)  A and B survive
         C (garbage)                 C           C is freed
```

The collector here is **precise** (objects describe their references),
**tracing** (reachability-based), and **stop-the-world** (the world pauses
during a collection).

## The core idea in code

Every managed type derives from `gc::Object` and tells the collector which
objects it references:

```cpp
class Node final : public gc::Object {
public:
  std::vector<Node*> children;

  void trace(gc::Visitor& visitor) const override {
    for (Node* child : children) {
      visitor.visit(child);
    }
  }
};
```

The heap owns allocations and a set of roots:

```cpp
gc::Heap heap;
gc::Root<Node> root(heap, heap.allocate<Node>());

root->children.push_back(heap.allocate<Node>()); // reachable
heap.collect();                                  // both objects survive

root = ...;                                      // drop the root
heap.collect();                                  // graph is reclaimed
```

`gc::Root` is an RAII handle: while it exists, its object is a root.

## Why cycles are interesting

Reference counting (`std::shared_ptr`) leaks on cycles:

```text
A ──▶ B
▲     │
└─────┘   no external references, but refcounts never hit zero
```

Mark-and-sweep handles this naturally. A cycle that is not reachable from a
root is simply not marked, so it is swept. This repo includes a test and a demo
for exactly that case.

## Project layout

```text
.
├── include/gc.hpp          # the entire collector
├── src/demo.cpp            # narrated demo
├── tests/gc_tests.cpp      # assert-based tests
├── CMakeLists.txt
└── .github/workflows/      # CI: build + test on every push
```

## Build and run

```bash
cmake -S . -B build
cmake --build build -j
./build/gc_demo
ctest --test-dir build --output-on-failure
```

Expected demo output:

```text
== Mark-and-sweep GC demo ==

[1] Build a reachable graph and collect
  before collect #1
  live=4  allocated=4  collected=0  collections=0

  after collect #1 (root is alive)
  live=4  allocated=4  collected=0  collections=1

[2] Drop the last root, then collect again
  before collect #2
  live=4  allocated=4  collected=0  collections=1

  [sweep] root freed
  [sweep] a freed
  [sweep] b freed
  [sweep] c freed
  after collect #2 (graph is unreachable)
  live=0  allocated=4  collected=4  collections=2

[3] Cycles without external roots are collected too
  after collect #3 (cycle is still rooted)
  live=2  allocated=6  collected=4  collections=3

  [sweep] cycle-a freed
  [sweep] cycle-b freed
  after collect #4 (cycle is garbage)
  live=0  allocated=6  collected=6  collections=4
```

## Algorithm details

| Phase | Work | Complexity |
| --- | --- | --- |
| Mark | Visit every live object once and trace its references | `O(live objects + references)` |
| Sweep | Scan every allocation and free unmarked objects | `O(heap size)` |

The mark phase uses an explicit worklist instead of recursion, so a long linked
list does not blow the C++ call stack.

## Limitations (on purpose)

This is a teaching implementation, not a production runtime.

- **Stop-the-world** — collection blocks the program.
- **No compaction** — freed slots are not defragmented.
- **Precise only** — types must implement `trace()` correctly.
- **Manual roots** — `gc::Root` handles define reachability.
- **No generational or incremental collection** — every cycle scans everything.
- **Raw child pointers** — as with any precise GC, keep them inside managed
  objects (or roots) so the collector can see them.

## Ideas to take it further

- Tri-color marking with an incremental collector
- Generational GC (young/old generations)
- Compaction and pointer updating
- Allocation-triggered automatic collections
- A small VM with a real stack and register root scanning
