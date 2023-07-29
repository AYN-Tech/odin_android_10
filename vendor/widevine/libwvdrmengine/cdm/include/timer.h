// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// Timer - Platform independent interface for a Timer class
//
#ifndef CDM_BASE_TIMER_H_
#define CDM_BASE_TIMER_H_

#include <stdint.h>

#include "utils/StrongPointer.h"

#include "disallow_copy_and_assign.h"

namespace wvcdm {

// Timer Handler class.
//
// Derive from this class if you wish to receive events when the timer
// expires. Provide the handler when setting up a new Timer.

class TimerHandler {
 public:
  TimerHandler() {};
  virtual ~TimerHandler() {};

  virtual void OnTimerEvent() = 0;
};

// Timer class. The implementation is platform dependent.
//
// This class provides a simple recurring timer API. The class receiving
// timer expiry events should derive from TimerHandler.
// Specify the receiver class and the periodicty of timer events when
// the timer is initiated by calling Start.

class Timer {
 public:
  class Impl;

  Timer();
  ~Timer();

  bool Start(TimerHandler *handler, uint32_t time_in_secs);
  void Stop();
  bool IsRunning();

 private:
  android::sp<Impl> impl_;

  CORE_DISALLOW_COPY_AND_ASSIGN(Timer);
};

}  // namespace wvcdm

#endif  // CDM_BASE_TIMER_H_
