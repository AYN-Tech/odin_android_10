// Copyright 2017 Google Inc. All Rights Reserved.
//
// This file contains the specializations for helper methods for the
// ValueMetric class.

#include <stdint.h>
#include <string>

#include "value_metric.h"

#include "OEMCryptoCENC.h"
#include "metrics_collections.h"
#include "wv_cdm_types.h"

namespace wvcdm {
namespace metrics {

namespace impl{

template <>
void SetValue<int>(drm_metrics::ValueMetric *value_proto,
                         const int &value) {
  value_proto->set_int_value(value);
}

template <>
void SetValue<long>(drm_metrics::ValueMetric *value_proto,
                          const long &value) {
  value_proto->set_int_value(value);
}

template <>
void SetValue<long long>(drm_metrics::ValueMetric *value_proto,
                               const long long &value) {
  value_proto->set_int_value(value);
}

template <>
void SetValue<unsigned int>(drm_metrics::ValueMetric *value_proto,
                                  const unsigned int &value) {
  value_proto->set_int_value((int64_t)value);
}

template <>
void SetValue<unsigned short>(drm_metrics::ValueMetric *value_proto,
                                    const unsigned short &value) {
  value_proto->set_int_value((int64_t)value);
}

template <>
void SetValue<unsigned long>(drm_metrics::ValueMetric *value_proto,
                                   const unsigned long &value) {
  value_proto->set_int_value((int64_t)value);
}

template <>
void SetValue<unsigned long long>(drm_metrics::ValueMetric *value_proto,
                                  const unsigned long long &value) {
  value_proto->set_int_value((int64_t)value);
}

template <>
void SetValue<bool>(drm_metrics::ValueMetric *value_proto,
                          const bool &value) {
  value_proto->set_int_value(value);
}

template <>
void SetValue<OEMCrypto_HDCP_Capability>(
    drm_metrics::ValueMetric *value_proto,
    const OEMCrypto_HDCP_Capability &value) {
  value_proto->set_int_value(value);
}

template <>
void SetValue<OEMCrypto_ProvisioningMethod>(
    drm_metrics::ValueMetric *value_proto,
    const OEMCrypto_ProvisioningMethod &value) {
  value_proto->set_int_value(value);
}

template <>
void SetValue<OEMCryptoInitializationMode>(
    drm_metrics::ValueMetric *value_proto,
    const OEMCryptoInitializationMode &value) {
  value_proto->set_int_value(value);
}

template <>
void SetValue<CdmSecurityLevel>(drm_metrics::ValueMetric *value_proto,
                                      const CdmSecurityLevel &value) {
  value_proto->set_int_value(value);
}

template <>
void SetValue<CdmUsageSupportType>(drm_metrics::ValueMetric *value_proto,
                                         const CdmUsageSupportType &value) {
  value_proto->set_int_value(value);
}

template <>
void SetValue<double>(drm_metrics::ValueMetric *value_proto,
                            const double &value) {
  value_proto->set_double_value(value);
}

template <>
void SetValue<std::string>(drm_metrics::ValueMetric *value_proto,
                                 const std::string &value) {
  value_proto->set_string_value(value);
}

}  // namespace impl
}  // namespace metrics
}  // namespace wvcdm
