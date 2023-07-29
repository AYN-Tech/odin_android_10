// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef CDM_AMI_ADAPTER_H_
#define CDM_AMI_ADAPTER_H_

#include <stdint.h>
#include <string>

#include <media/MediaAnalyticsItem.h>

namespace wvcdm {

class AmiAdapter {

 public:
  AmiAdapter();
  AmiAdapter(int64_t parent);
  ~AmiAdapter();

  void UpdateString(const std::string& metric_id, const std::string& value);
  void UpdateInt32(const std::string& metric_id, int32_t value);
  void UpdateInt64(const std::string& metric_id, int64_t value);
  void UpdateDouble(const std::string& metric_id, double value);

 private:
  android::MediaAnalyticsItem analytics_item_;

};

}  // namespace wvcdm

#endif
