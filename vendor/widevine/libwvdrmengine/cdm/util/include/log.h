// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// Log - Platform independent interface for a Logging class
//
#ifndef WVCDM_UTIL_LOG_H_
#define WVCDM_UTIL_LOG_H_

#include "util_common.h"

namespace wvcdm {

// Simple logging class. The implementation is platform dependent.

typedef enum {
  // This log level should only be used for |g_cutoff|, in order to silence all
  // logging. It should never be passed to |Log()| as a log level.
  LOG_SILENT = -1,

  LOG_ERROR = 0,
  LOG_WARN = 1,
  LOG_INFO = 2,
  LOG_DEBUG = 3,
  LOG_VERBOSE = 4,
} LogPriority;

extern LogPriority g_cutoff;

// Enable/disable verbose logging (LOGV).
// This function is supplied for cases where the system layer does not
// initialize logging.  This is also needed to initialize logging in
// unit tests.
CORE_UTIL_EXPORT void InitLogging();

CORE_UTIL_EXPORT void Log(
    const char* file, const char* function, int line, LogPriority level,
    const char* fmt, ...);

// Log APIs
#ifndef LOGE
#define LOGE(...) Log(__FILE__, __func__, __LINE__, \
                      wvcdm::LOG_ERROR, __VA_ARGS__)
#define LOGW(...) Log(__FILE__, __func__, __LINE__, \
                      wvcdm::LOG_WARN, __VA_ARGS__)
#define LOGI(...) Log(__FILE__, __func__, __LINE__, \
                      wvcdm::LOG_INFO, __VA_ARGS__)
#define LOGD(...) Log(__FILE__, __func__, __LINE__, \
                      wvcdm::LOG_DEBUG, __VA_ARGS__)
#define LOGV(...) Log(__FILE__, __func__, __LINE__, \
                      wvcdm::LOG_VERBOSE, __VA_ARGS__)
#endif
}  // namespace wvcdm

#endif  // WVCDM_UTIL_LOG_H_
