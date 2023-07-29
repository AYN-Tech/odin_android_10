// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef CDM_TEST_CONFIG_TEST_ENV_H_
#define CDM_TEST_CONFIG_TEST_ENV_H_

#include <string>
#include "disallow_copy_and_assign.h"
#include "wv_cdm_types.h"

// Declare class ConfigTestEnv - holds the configuration settings needed
// to talk to the various provisioning and license servers.
//
// License Servers
//   QA  - early test server (corporate access only, not generally usable).
//   UAT  - test server with non-production data.
//   Staging  - test server with access to production data.
//   Production  - live, production server.
//   Google Play  - Allows testing on Google Play servers (very stale).
//
// Provisioning Servers
//   UAT  - early access provisioning server.
//   Staging  - early access to features.
//   Production  - live production server.

// Useful configurations
namespace wvcdm {
typedef enum {
  kGooglePlayServer,   // not tested recently
  kContentProtectionUatServer,
  kContentProtectionStagingServer,
  kContentProtectionProductionServer,
} ServerConfigurationId;

// Identifies content used in tests. Specify Prod/Uat/Staging if content
// has been registered across license services.
enum ContentId {
  kContentIdStreaming,
  kContentIdOffline,
  kContentIdStagingSrmOuputProtectionRequested,
  kContentIdStagingSrmOuputProtectionRequired,
};

// Configures default test environment.
class ConfigTestEnv {
 public:

  typedef struct {
    ServerConfigurationId id;
    std::string license_server_url;
    std::string license_service_certificate;  //  Binary (not hex).
    std::string client_tag;
    std::string key_id;  // Hex.
    std::string offline_key_id;
    std::string provisioning_server_url;
    std::string provisioning_service_certificate;  // Binary (not hex).
  } LicenseServerConfiguration;

  explicit ConfigTestEnv(ServerConfigurationId server_id);
  ConfigTestEnv(ServerConfigurationId server_id, bool streaming);
  ConfigTestEnv(ServerConfigurationId server_id, bool streaming, bool renew,
                bool release);
  // Allow copy and assign.  Performance is not an issue in test initialization.
  ConfigTestEnv(const ConfigTestEnv &other) { *this = other; };
  ConfigTestEnv& operator=(const ConfigTestEnv &other);

  ~ConfigTestEnv() {};

  ServerConfigurationId server_id() { return server_id_; }
  const std::string& client_auth() const { return client_auth_; }
  const KeyId& key_id() const { return key_id_; }
  const CdmKeySystem& key_system() const { return key_system_; }
  const std::string& license_server() const { return license_server_; }
  const std::string& provisioning_server() const {
    return provisioning_server_;
  }
  const std::string& license_service_certificate() const {
    return license_service_certificate_;
  }
  const std::string& provisioning_service_certificate() const {
    return provisioning_service_certificate_;
  }

  static const CdmInitData GetInitData(ContentId content_id);
  static const std::string& GetLicenseServerUrl(
      ServerConfigurationId server_configuration_id);

  void set_key_id(const KeyId& key_id) { key_id_.assign(key_id); }
  void set_key_system(const CdmKeySystem& key_system) {
    key_system_.assign(key_system);
  }
  void set_license_server(const std::string& license_server) {
    license_server_.assign(license_server);
  }
  void set_license_service_certificate(
      const std::string& license_service_certificate) {
    license_service_certificate_.assign(license_service_certificate);
  }
  void set_provisioning_server(const std::string& provisioning_server) {
    provisioning_server_.assign(provisioning_server);
  }
  void set_provisioning_service_certificate(
      const std::string& provisioning_service_certificate) {
    provisioning_service_certificate_.assign(provisioning_service_certificate);
  }

 private:
  void Init(ServerConfigurationId server_id);

  ServerConfigurationId server_id_;
  std::string client_auth_;
  KeyId key_id_;
  CdmKeySystem key_system_;
  std::string license_server_;
  std::string provisioning_server_;
  std::string license_service_certificate_;
  std::string provisioning_service_certificate_;
};

}  // namespace wvcdm

#endif  // CDM_TEST_CONFIG_TEST_ENV_H_
