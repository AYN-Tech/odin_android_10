// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_CORE_PROPERTIES_H_
#define WVCDM_CORE_PROPERTIES_H_

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "cdm_client_property_set.h"
#include "disallow_copy_and_assign.h"
#include "wv_cdm_types.h"

#if defined(UNIT_TEST)
#include <gtest/gtest_prod.h>
#endif

namespace wvcdm {

typedef std::map<CdmSessionId, CdmClientPropertySet*>
    CdmClientPropertySetMap;

// This class saves information about features and properties enabled
// for a given platform. At initialization it initializes properties from
// property_configuration.h. That file specifies features selected for each
// platform. Core CDM can then query enabled features though specific getter
// methods.
// Setter methods are provided but their only planned use is for testing.
class Properties {
 public:
  static void Init() {
    std::unique_lock<std::mutex> lock(init_mutex_);

    if (!is_initialized_) {
      InitOnce();
      is_initialized_ = true;
    }
  }

  static inline bool oem_crypto_use_secure_buffers() {
    return oem_crypto_use_secure_buffers_;
  }
  static inline bool oem_crypto_use_fifo() { return oem_crypto_use_fifo_; }
  static inline bool oem_crypto_use_userspace_buffers() {
    return oem_crypto_use_userspace_buffers_;
  }
  static inline bool provisioning_messages_are_binary() {
    return provisioning_messages_are_binary_;
  }
  static inline bool allow_service_certificate_requests() {
    return allow_service_certificate_requests_;
  }
  static void set_provisioning_messages_are_binary(bool flag) {
    provisioning_messages_are_binary_ = flag;
  }
  static bool GetCompanyName(std::string* company_name);
  static bool GetModelName(std::string* model_name);
  static bool GetArchitectureName(std::string* arch_name);
  static bool GetDeviceName(std::string* device_name);
  static bool GetProductName(std::string* product_name);
  static bool GetBuildInfo(std::string* build_info);
  static bool GetWVCdmVersion(std::string* version);
  static bool GetDeviceFilesBasePath(CdmSecurityLevel security_level,
                                     std::string* base_path);
  static bool GetFactoryKeyboxPath(std::string* keybox);
  static bool GetOEMCryptoPath(std::string* library_name);
  static bool GetSandboxId(std::string *sandbox_id);
  static bool AlwaysUseKeySetIds();
  static bool UseProviderIdInProvisioningRequest();

  static bool GetSecurityLevelDirectories(std::vector<std::string>* dirs);
  static bool GetApplicationId(const CdmSessionId& session_id,
                               std::string* app_id);
  static bool GetServiceCertificate(const CdmSessionId& session_id,
                                    std::string* service_certificate);
  static bool SetServiceCertificate(const CdmSessionId& session_id,
                                    const std::string& service_certificate);
  static bool UsePrivacyMode(const CdmSessionId& session_id);
  static uint32_t GetSessionSharingId(const CdmSessionId& session_id);

  static bool AddSessionPropertySet(const CdmSessionId& session_id,
                                    CdmClientPropertySet* property_set);
  static bool RemoveSessionPropertySet(const CdmSessionId& session_id);

 protected:
  // This function always runs the code in |Init()| (and subsequently
  // |InitOnce()|) even if Properties have already been initialized. This is
  // needed by certain tests that are dependent on controlling the mutable state
  // of Properties. Should not be used in general, as most tests rely on
  // Properties' normal guarantee about |Init()| being safe to call multiple
  // times without destroying mutable state.
  static void ForceReinit() {
    {
      std::unique_lock<std::mutex> lock(init_mutex_);
      is_initialized_ = false;
    }
    Init();
  }

 private:
  static CdmClientPropertySet* GetCdmClientPropertySet(
      const CdmSessionId& session_id);
  static void set_oem_crypto_use_secure_buffers(bool flag) {
    oem_crypto_use_secure_buffers_ = flag;
  }
  static void set_oem_crypto_use_fifo(bool flag) {
    oem_crypto_use_fifo_ = flag;
  }
  static void set_oem_crypto_use_userspace_buffers(bool flag) {
    oem_crypto_use_userspace_buffers_ = flag;
  }
  static void set_use_certificates_as_identification(bool flag) {
    use_certificates_as_identification_ = flag;
  }

#if defined(UNIT_TEST)
  FRIEND_TEST(CdmSessionTest, InitWithBuiltInCertificate);
  FRIEND_TEST(CdmSessionTest, InitWithCertificate);
  FRIEND_TEST(CdmSessionTest, InitWithKeybox);
  FRIEND_TEST(CdmSessionTest, ReInitFail);
  FRIEND_TEST(CdmSessionTest, InitFailCryptoError);
  FRIEND_TEST(CdmSessionTest, InitNeedsProvisioning);
  FRIEND_TEST(CdmLicenseTest, PrepareKeyRequestValidation);
#endif

  // Called at least once before any properties are used.
  static void InitOnce();

  static std::mutex init_mutex_;
  static bool is_initialized_;
  static bool oem_crypto_use_secure_buffers_;
  static bool oem_crypto_use_fifo_;
  static bool oem_crypto_use_userspace_buffers_;
  static bool use_certificates_as_identification_;
  static bool provisioning_messages_are_binary_;
  static bool allow_service_certificate_requests_;
  static std::unique_ptr<CdmClientPropertySetMap> session_property_set_;

  CORE_DISALLOW_COPY_AND_ASSIGN(Properties);
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_PROPERTIES_H_
