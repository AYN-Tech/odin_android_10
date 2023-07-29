#include "src/utils/logging.h"

#include <cstdarg>
#include <cstdio>
#include <sstream>
#include <thread>  // NOLINT (unapproved c++11 header)

#if !defined(LIBGAV1_LOG_LEVEL)
#define LIBGAV1_LOG_LEVEL (1 << 30)
#endif

namespace libgav1 {
namespace internal {
#if LIBGAV1_ENABLE_LOGGING
namespace {

const char* LogSeverityName(LogSeverity severity) {
  switch (severity) {
    case LogSeverity::kInfo:
      return "INFO";
    case LogSeverity::kError:
      return "ERROR";
    case LogSeverity::kWarning:
      return "WARNING";
  }
  return "UNKNOWN";
}

}  // namespace

void Log(LogSeverity severity, const char* file, int line, const char* format,
         ...) {
  if (LIBGAV1_LOG_LEVEL < static_cast<int>(severity)) return;
  std::ostringstream ss;
  ss << std::hex << std::this_thread::get_id();
  fprintf(stderr, "%s %s %s:%d] ", LogSeverityName(severity), ss.str().c_str(),
          file, line);

  va_list ap;
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fprintf(stderr, "\n");
}
#else   // !LIBGAV1_ENABLE_LOGGING
void Log(LogSeverity /*severity*/, const char* /*file*/, int /*line*/,
         const char* /*format*/, ...) {}
#endif  // LIBGAV1_ENABLE_LOGGING

}  // namespace internal
}  // namespace libgav1
