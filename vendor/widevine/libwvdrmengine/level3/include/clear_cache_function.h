#include <stdio.h>
#include <stdlib.h>

#if __mips__
#include <sys/syscall.h>
#include <asm/cachectl.h>
#endif

namespace wvoec3 {

// The Cache Flush function is very processor dependent, and is used by the
// Level3 code using platform-specific flags.
// Linked in separately from the rest of the build, since this code isn't
// obfuscated and relies on system flags to function properly.
void clear_cache_function(void *page, size_t len) {
  // Note on cross platform support.  If __has_builtin is not defined as a
  // preprocessor function, we cannot use
  // "#if defined(__has_builtin) && __has_builtin(..)".
  // So, instead, we will define USED_BUILTIN_CLEAR_CACHE if both conditions
  // are true, and use "#ifndef  USED_BUILTIN_CLEAR_CACHE" instead of #else.
#ifdef __has_builtin
#if __has_builtin(__builtin___clear_cache)
#pragma message "(info): clear_cache_function is using __builtin___clear_cache."
#define USED_BUILTIN_CLEAR_CACHE
  char *begin = static_cast<char *>(page);
  char *end = begin + len;
  __builtin___clear_cache(begin, end);
#endif
#endif
#ifndef USED_BUILTIN_CLEAR_CACHE
#if __arm__
#pragma message "(info): clear_cache_function is using arm asm."
  // ARM Cache Flush System Call:
  char *begin = static_cast<char *>(page);
  char *end = begin + len;
  const int syscall = 0xf0002;
  __asm __volatile(
      "push      {r0, r1, r2, r7}\n"
      "mov       r0, %0\n"
      "mov       r1, %1\n"
      "mov       r7, %2\n"
      "mov       r2, #0x0\n"
      "svc       0x00000000\n"
      "pop       {r0, r1, r2, r7}\n"
      :
      : "r"(begin), "r"(end), "r"(syscall)
      : "r0", "r1", "r7");
#elif __mips__
#pragma message "(info): clear_cache_function is using mips asm."
  int result = syscall(__NR_cacheflush, page, len, ICACHE);
  if (result) {
    fprintf(stderr, "cacheflush failed!: errno=%d %s\n", errno,
            strerror(errno));
    exit(-1);  // TODO(fredgc): figure out more graceful error handling.
  }
#else
#pragma message "(info): clear_cache_function is not doing anything."
  // TODO(fredgc): silence warning about unused variables.
#endif
#endif
}

}  // namespace wvoec3