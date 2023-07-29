#include "timer_metric.h"

namespace wvcdm {
namespace metrics {

void TimerMetric::Start() {
  start_ = clock_.now();
  is_started_ = true;
}

void TimerMetric::Clear() {
  is_started_ = false;
}

double TimerMetric::AsMs() const {
 return (clock_.now() - start_) / std::chrono::milliseconds(1);
}

double TimerMetric::AsUs() const {
 return (clock_.now() - start_) / std::chrono::microseconds(1);
}

}  // namespace metrics
}  // namespace wvcdm
