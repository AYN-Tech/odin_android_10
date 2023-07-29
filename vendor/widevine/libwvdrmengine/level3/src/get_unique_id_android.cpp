#include "level3.h"

#include <string.h>

#include <string>

// The function property_get is defined differently if it comes from the IN_APP
// version or from the Android OS.
#if defined(IN_APP_FASTBALL)
#include "properties_fastball.h"
#else
#include <android-base/properties.h>
#endif

namespace wvoec3 {

const char *getUniqueID(size_t *len) {
  static std::string unique_id;
  unique_id = android::base::GetProperty("ro.serialno", "");
  if (unique_id.empty()) {
    unique_id = android::base::GetProperty("net.hostname", "0123456789abc");
  }
#if defined(IN_APP_FASTBALL) || defined(IN_APP_MOVIES)
  unique_id += android::base::GetProperty("package.name", "com.google.inapp");
#endif
  *len = unique_id.size();
  return unique_id.c_str();
}

}  // namespace wvoec3
