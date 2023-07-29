// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "cdm_engine_factory.h"

#include <memory>
#include <string>
#include "cdm_engine.h"
#include "cdm_engine_metrics_decorator.h"
#include "clock.h"
#include "file_store.h"

namespace wvcdm {

CdmEngine* CdmEngineFactory::CreateCdmEngine(FileSystem* file_system) {
  std::unique_ptr<metrics::EngineMetrics> engine_metrics(
      new metrics::EngineMetrics());

  return new CdmEngineMetricsImpl<CdmEngine>(file_system,
                                             std::move(engine_metrics));
}

}  // namespace wvcdm
