#include "level3.h"

#include "level3_file_system_android.h"

namespace wvoec3 {

OEMCrypto_Level3FileSystem* createLevel3FileSystem() {
  return new OEMCrypto_Level3AndroidFileSystem();
}

void deleteLevel3FileSystem(OEMCrypto_Level3FileSystem* file_system) {
  if (file_system) {
    delete file_system;
  }
}

}  // namespace wvoec3