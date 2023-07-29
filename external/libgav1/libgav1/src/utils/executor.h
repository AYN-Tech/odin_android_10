#ifndef LIBGAV1_SRC_UTILS_EXECUTOR_H_
#define LIBGAV1_SRC_UTILS_EXECUTOR_H_

#include <functional>

namespace libgav1 {

class Executor {
 public:
  virtual ~Executor();

  // Schedules the specified "callback" for execution in this executor.
  // Depending on the subclass implementation, this may block in some
  // situations.
  virtual void Schedule(std::function<void()> callback) = 0;
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_UTILS_EXECUTOR_H_
