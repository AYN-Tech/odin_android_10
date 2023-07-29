// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_CORE_CDM_ENGINE_FACTORY_H_
#define WVCDM_CORE_CDM_ENGINE_FACTORY_H_

#include <string>

#include "cdm_engine.h"
#include "file_store.h"

namespace wvcdm {

// This factory is used to create an instance of the CdmEngine.
class CdmEngineFactory {
 public:
  // Creates a new instance of a CdmEngine. Caller retains ownership of the
  // |files_system| which cannot be null.
  static CdmEngine* CreateCdmEngine(FileSystem* file_system);

 private:
  CORE_DISALLOW_COPY_AND_ASSIGN(CdmEngineFactory);
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_CDM_ENGINE_FACTORY_H_
