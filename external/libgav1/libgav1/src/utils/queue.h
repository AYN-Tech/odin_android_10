#ifndef LIBGAV1_SRC_UTILS_QUEUE_H_
#define LIBGAV1_SRC_UTILS_QUEUE_H_

#include <cassert>
#include <cstddef>
#include <memory>
#include <new>

#include "src/utils/compiler_attributes.h"

namespace libgav1 {

// A FIFO queue of a fixed capacity. The elements are copied, so the element
// type T should be small.
//
// WARNING: No error checking is performed.
template <typename T>
class Queue {
 public:
  LIBGAV1_MUST_USE_RESULT bool Init(size_t capacity) {
    elements_.reset(new (std::nothrow) T[capacity]);
    if (elements_ == nullptr) return false;
    capacity_ = capacity;
    return true;
  }

  // Pushes the element |value| to the end of the queue. It is an error to call
  // Push() when the queue is full.
  void Push(T value) {
    assert(size_ < capacity_);
    elements_[back_++] = value;
    if (back_ == capacity_) back_ = 0;
    ++size_;
  }

  // Returns the element at the front of the queue and removes it from the
  // queue. It is an error to call Pop() when the queue is empty.
  T Pop() {
    assert(size_ != 0);
    const T front_element = elements_[front_++];
    if (front_ == capacity_) front_ = 0;
    --size_;
    return front_element;
  }

  // Returns true if the queue is empty.
  bool Empty() const { return size_ == 0; }

  // Returns true if the queue is full.
  bool Full() const { return size_ >= capacity_; }

  // Returns the number of elements in the queue.
  size_t Size() const { return size_; }

 private:
  // An array of |capacity| elements. Used as a circular array.
  std::unique_ptr<T[]> elements_;
  size_t capacity_ = 0;
  // The index of the element to be removed by Pop().
  size_t front_ = 0;
  // The index where the new element is inserted by Push().
  size_t back_ = 0;
  size_t size_ = 0;
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_UTILS_QUEUE_H_
