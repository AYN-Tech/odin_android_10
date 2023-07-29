#ifndef LIBGAV1_SRC_UTILS_STACK_H_
#define LIBGAV1_SRC_UTILS_STACK_H_

#include <cassert>
#include <utility>

namespace libgav1 {

// A LIFO stack of a fixed capacity. The elements are moved using std::move, so
// the element type T has to be movable.
//
// WARNING: No error checking is performed.
template <typename T, int capacity>
class Stack {
 public:
  // Pushes the element |value| to the top of the stack. It is an error to call
  // Push() when the stack is full.
  void Push(T value) {
    ++top_;
    assert(top_ < capacity);
    elements_[top_] = std::move(value);
  }

  // Returns the element at the top of the stack and removes it from the stack.
  // It is an error to call Pop() when the stack is empty.
  T Pop() {
    assert(top_ >= 0);
    return std::move(elements_[top_--]);
  }

  // Returns true if the stack is empty.
  bool Empty() const { return top_ < 0; }

 private:
  static_assert(capacity > 0, "");
  T elements_[capacity];
  // The array index of the top of the stack. The stack is empty if top_ is -1.
  int top_ = -1;
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_UTILS_STACK_H_
