// Copyright 2018 Google Inc. All Rights Reserved.
//
// This file contains the declarations for the EventMetric class and related
// types.
#ifndef WVCDM_METRICS_ATTRIBUTE_HANDLER_H_
#define WVCDM_METRICS_ATTRIBUTE_HANDLER_H_

#include "OEMCryptoCENC.h"
#include "field_tuples.h"
#include "log.h"
#include "metrics.pb.h"
#include "pow2bucket.h"
#include "value_metric.h"
#include "wv_cdm_types.h"

namespace wvcdm {
namespace metrics {

// This method is used to set the value of a single proto field.
// Specializations handle setting each value.
template <int I, typename F>
void SetAttributeField(const F& value, drm_metrics::Attributes* attributes);

// This class handles serializing the attributes for metric types that breakdown
// into multiple buckets.  The template parameters should be specified in pairs.
// E.g. I1 and F1 should match.  I1 should specify the field number in the proto
// and F1 should specify the type of the field to be set.
template <int I1, typename F1, int I2, typename F2, int I3, typename F3, int I4,
          typename F4>
class AttributeHandler {
 public:
  // This method returns the serialized attribute proto for the given field
  // values. The order of the field values must match the order in the
  // template.
  std::string GetSerializedAttributes(F1 field1, F2 field2, F3 field3,
                                      F4 field4) const {
    drm_metrics::Attributes attributes;
    SetAttributeField<I1, F1>(field1, &attributes);
    SetAttributeField<I2, F2>(field2, &attributes);
    SetAttributeField<I3, F3>(field3, &attributes);
    SetAttributeField<I4, F4>(field4, &attributes);
    std::string serialized_attributes;
    if (!attributes.SerializeToString(&serialized_attributes)) {
      LOGE("Failed to serialize attribute proto.");
      return "";
    }
    return serialized_attributes;
  }
};

}  // namespace metrics
}  // namespace wvcdm
#endif  // WVCDM_METRICS_ATTRIBUTE_HANDLER_H_
