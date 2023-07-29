#include "level3.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "log.h"

namespace wvoec3 {

uint64_t generate_entropy() {
  FILE* urandom_file = fopen("/dev/urandom", "r");
  uint64_t value = 0;
  if (urandom_file) {
    if (fread(&value, 8, 1, urandom_file) != 1) {
      LOGE("Could not read from file /dev/urandom. errno=%s", strerror(errno));
    }
    if (fclose(urandom_file) != 0) {
      LOGE("Could not close file /dev/urandom. errno=%s", strerror(errno));
    }
  } else {
    LOGE("Could not open file /dev/urandom. errno=%s", strerror(errno));
  }
  return value;
}

}  // namespace wvoec3