// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "ami_adapter.h"

#include <log.h>

namespace wvcdm {

AmiAdapter::AmiAdapter() :
    analytics_item_("widevine") {
  analytics_item_.generateSessionID();
}

AmiAdapter::AmiAdapter(int64_t parent) :
    analytics_item_("widevine") {
  analytics_item_.generateSessionID();
  analytics_item_.setInt64("/drm/widevine/parent/external", parent);
}

AmiAdapter::~AmiAdapter() {
  analytics_item_.selfrecord();
}

void AmiAdapter::UpdateString(const std::string& metric_id,
                              const std::string& value) {
  analytics_item_.setCString(metric_id.c_str(), value.c_str());
  LOGV(
      "AmiAdapter (%lld) %s : %s",
      analytics_item_.getSessionID(),
      metric_id.c_str(),
      value.c_str());
}

void AmiAdapter::UpdateInt32(const std::string& metric_id,
                             int32_t value) {
  analytics_item_.setInt32(metric_id.c_str(), value);
  LOGV(
      "AmiAdapter (%lld) %s : %ld",
      analytics_item_.getSessionID(),
      metric_id.c_str(),
      value);
}

void AmiAdapter::UpdateInt64(const std::string& metric_id,
                             int64_t value) {
  analytics_item_.setInt64(metric_id.c_str(), value);
  LOGV(
      "AmiAdapter (%lld) %s : %lld",
      analytics_item_.getSessionID(),
      metric_id.c_str(),
      value);
}

void AmiAdapter::UpdateDouble(const std::string& metric_id,
                              double value) {
  analytics_item_.setDouble(metric_id.c_str(), value);
  LOGV(
      "AmiAdapter (%lld) %s : %f",
      analytics_item_.getSessionID(),
      metric_id.c_str(),
      value);
}

}  // namespace wvcdm
