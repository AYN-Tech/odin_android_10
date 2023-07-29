// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// Log - implemented using the standard Android logging mechanism

/*
 * Qutoing from system/core/include/log/log.h:
 * Normally we strip ALOGV (VERBOSE messages) from release builds.
 * You can modify this (for example with "#define LOG_NDEBUG 0"
 * at the top of your source file) to change that behavior.
 */
#ifndef LOG_NDEBUG
#ifdef NDEBUG
#define LOG_NDEBUG 1
#else
#define LOG_NDEBUG 0
#endif
#endif

#define LOG_TAG "WVCdm"
#define LOG_BUF_SIZE 1024

#include "log.h"
#include <utils/Log.h>

#include <string>

#include <stdio.h>
#include <stdarg.h>

/*
 * Uncomment the line below if you want to have the LOGV messages to print
 * IMPORTANT : this will affect all of CDM
 */

// #define LOG_NDEBUG 0

namespace wvcdm {

LogPriority g_cutoff = LOG_VERBOSE;

void InitLogging() {}

void Log(const char* file, const char* function, int line, LogPriority level,
         const char* format, ...) {
  const char* filename = strrchr(file, '/');
  filename = filename == nullptr ? file : filename + 1;

  char buf[LOG_BUF_SIZE];
  int len = snprintf(buf, LOG_BUF_SIZE, "[%s(%d):%s] ", filename, line,
                     function);
  if (len < 0) len = 0;
  if (static_cast<unsigned int>(len) < sizeof(buf)) {
    va_list ap;
    va_start(ap, format);
    vsnprintf(buf+len, LOG_BUF_SIZE-len, format, ap);
    va_end(ap);
  }

  android_LogPriority prio = ANDROID_LOG_VERBOSE;

  switch(level) {
    case LOG_SILENT: return;  // It is nonsensical to pass LOG_SILENT.
    case LOG_ERROR: prio = ANDROID_LOG_ERROR; break;
    case LOG_WARN: prio = ANDROID_LOG_WARN; break;
    case LOG_INFO: prio = ANDROID_LOG_INFO; break;
    case LOG_DEBUG: prio = ANDROID_LOG_DEBUG; break;
#if LOG_NDEBUG
    case LOG_VERBOSE: return;
#else
    case LOG_VERBOSE: prio = ANDROID_LOG_VERBOSE; break;
#endif
  }

  __android_log_write(prio, LOG_TAG, buf);
}

}  // namespace wvcdm
