#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace gc {

class Object;

// A visitor receives every reference held by an Object. The collector
// uses this during the mark phase to discover the reachable graph.
class Visitor {
public:
  virtual ~Visitor() = default;
  virtual void visit(Object* object) = 0;
};

// Base class for every heap-managed object.
//
// Objects must implement trace() and report every GC-managed pointer they
// own. The collector only knows about references it can see through trace().
class Object {
  friend class Heap;

public:
  Object() = default;
  Object(const Object&) = delete;
  Object& operator=(const Object&) = delete;

  virtual ~Object() = default;

  virtual void trace(Visitor& visitor) const = 0;

private:
  bool marked_ = false;
};

// A precise, stop-the-world, mark-and-sweep heap.
//
// - allocate() creates an object owned by the heap.
// - addRoot()/removeRoot() define the entry points of the object graph.
// - collect() marks everything reachable from the roots, then frees the rest.
class Heap {
public:
  Heap() = default;
  Heap(const Heap&) = delete;
  Heap& operator=(const Heap&) = delete;

  template <typename T, typename... Args>
  T* allocate(Args&&... args) {
    static_assert(std::is_base_of_v<Object, T>,
                  "gc::Heap can only allocate types derived from gc::Object");

    auto object = std::make_unique<T>(std::forward<Args>(args)...);
    T* raw = object.get();
    objects_.push_back(std::move(object));
    ++allocations_;
    return raw;
  }

  void addRoot(Object* object) {
    if (object != nullptr) {
      roots_.insert(object);
    }
  }

  void removeRoot(Object* object) {
    if (object != nullptr) {
      roots_.erase(object);
    }
  }

  void collect() {
    for (Object* root : roots_) {
      mark(root);
    }

    std::vector<std::unique_ptr<Object>> survivors;
    survivors.reserve(objects_.size());

    for (auto& object : objects_) {
      if (object->marked_) {
        object->marked_ = false;
        survivors.push_back(std::move(object));
      }
    }

    collected_ += objects_.size() - survivors.size();
    objects_ = std::move(survivors);
    ++collections_;
  }

  std::size_t liveObjects() const { return objects_.size(); }
  std::size_t allocations() const { return allocations_; }
  std::size_t collections() const { return collections_; }
  std::size_t collected() const { return collected_; }

private:
  void markIfUnmarked(Object* object, std::vector<Object*>& worklist) {
    if (object != nullptr && !object->marked_) {
      object->marked_ = true;
      worklist.push_back(object);
    }
  }

  class Marker final : public Visitor {
  public:
    Marker(Heap& heap, std::vector<Object*>& worklist)
        : heap_(heap), worklist_(worklist) {}

    void visit(Object* object) override {
      heap_.markIfUnmarked(object, worklist_);
    }

  private:
    Heap& heap_;
    std::vector<Object*>& worklist_;
  };

  void mark(Object* root) {
    std::vector<Object*> worklist;
    markIfUnmarked(root, worklist);

    Marker marker(*this, worklist);
    while (!worklist.empty()) {
      Object* object = worklist.back();
      worklist.pop_back();
      object->trace(marker);
    }
  }

  std::vector<std::unique_ptr<Object>> objects_;
  std::unordered_set<Object*> roots_;
  std::size_t allocations_ = 0;
  std::size_t collections_ = 0;
  std::size_t collected_ = 0;
};

// RAII root handle. While a Root exists, its object (and everything the
// object references) is reachable and therefore survives collection.
template <typename T>
class Root {
public:
  Root(Heap& heap, T* object) : heap_(&heap), object_(object) {
    static_assert(std::is_base_of_v<Object, T>,
                  "gc::Root can only hold types derived from gc::Object");
    heap_->addRoot(object_);
  }

  ~Root() {
    if (heap_ != nullptr) {
      heap_->removeRoot(object_);
    }
  }

  Root(const Root&) = delete;
  Root& operator=(const Root&) = delete;

  Root(Root&& other) noexcept : heap_(other.heap_), object_(other.object_) {
    other.heap_ = nullptr;
    other.object_ = nullptr;
  }

  Root& operator=(Root&& other) noexcept {
    if (this != &other) {
      if (heap_ != nullptr) {
        heap_->removeRoot(object_);
      }
      heap_ = other.heap_;
      object_ = other.object_;
      other.heap_ = nullptr;
      other.object_ = nullptr;
    }
    return *this;
  }

  T* get() const { return object_; }
  T* operator->() const { return object_; }
  T& operator*() const { return *object_; }
  explicit operator bool() const { return object_ != nullptr; }

private:
  Heap* heap_ = nullptr;
  T* object_ = nullptr;
};

} // namespace gc
