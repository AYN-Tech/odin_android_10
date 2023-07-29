// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

// These tests validate features supported by the CDM that may not be
// supported by the OEM supplied L1 OEMCrypto implementation. These test use
// the modifiable/mock OEMCrypto. OEMCrypto configuration flags
// are enabled/disabled when validating features in tests.
//
// Follow instructions to build and use the modifiable OEMCrypto
//
// Use the install script to backup the OEM supplied L1 OEMCrypto and
// replace with the modifiable/mock OEMCrypto. Use default configuration
// amd modify flags as mentioned by the tests.
//
// NOTE: These tests are intended for use by Google to validate
// CDM functionality across a broad range of OEMCrypto implementations
// and are not intended to be run by partners.

#include <errno.h>
#include <getopt.h>
#include <sstream>

#include <cutils/properties.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "OEMCryptoCENC.h"
#include "config_test_env.h"
#include "license_protocol.pb.h"
#include "license_request.h"
#include "log.h"
#include "oemcrypto_adapter.h"
#include "string_conversions.h"
#include "test_base.h"
#include "test_printers.h"
#include "url_request.h"
#include "wv_cdm_constants.h"
#include "wv_cdm_event_listener.h"
#include "wv_cdm_types.h"
#include "wv_content_decryption_module.h"

using ::testing::_;
using ::testing::Each;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;
using ::testing::StrictMock;

namespace {

#define N_ELEM(a) (sizeof(a) / sizeof(a[0]))

// HTTP response codes.
const int kHttpOk = 200;
// The following two responses are unused, but left here for human debuggers.
// const int kHttpBadRequest = 400;
// const int kHttpInternalServerError = 500;
const std::string kEmptyServiceCertificate;

wvcdm::KeyId kSrmHdKeyId1 = wvcdm::a2bs_hex("30303030303030303030303030303032");
wvcdm::KeyId kSrmHdKeyId2 = wvcdm::a2bs_hex("30303030303030303030303030303033");
wvcdm::KeyId kSrmHdKeyId3 = wvcdm::a2bs_hex("30303030303030303030303030303037");
wvcdm::KeyId kSrmSdKeyId1 = wvcdm::a2bs_hex("30303030303030303030303030303030");
wvcdm::KeyId kSrmSdKeyId2 = wvcdm::a2bs_hex("30303030303030303030303030303031");
wvcdm::KeyId kSrmSdKeyId3 = wvcdm::a2bs_hex("30303030303030303030303030303034");
wvcdm::KeyId kSrmSdKeyId4 = wvcdm::a2bs_hex("30303030303030303030303030303035");
wvcdm::KeyId kSrmSdKeyId5 = wvcdm::a2bs_hex("30303030303030303030303030303036");

std::set<wvcdm::KeyId> kSrmExpectedAllKeyIds = {
    kSrmHdKeyId1, kSrmHdKeyId2, kSrmHdKeyId3, kSrmSdKeyId1,
    kSrmSdKeyId2, kSrmSdKeyId3, kSrmSdKeyId4, kSrmSdKeyId5,
};

std::set<wvcdm::KeyId> kSrmExpectedSdKeyIds = {
    kSrmSdKeyId1, kSrmSdKeyId2, kSrmSdKeyId3, kSrmSdKeyId4, kSrmSdKeyId5,
};

std::vector<uint8_t> kEncryptData = wvcdm::a2b_hex(
    "64ab17b3e3dfab47245c7cce4543d4fc7a26dcf248f19f9b59f3c92601440b36"
    "17c8ed0c96c656549e461f38708cd47a434066f8df28ccc28b79252eee3f9c2d"
    "7f6c68ebe40141fe818fe082ca523c03d69ddaf183a93c022327fedc5582c5ab"
    "ca9d342b71263a67f9cb2336f12108aaaef464f17177e44e9b0c4e56e61da53c"
    "2150b4405cc82d994dfd9bf4087c761956d6688a9705db4cf350381085f383c4"
    "9666d4aed135c519c1f0b5cba06e287feea96ea367bf54e7368dcf998276c6e4"
    "6497e0c50e20fef74e42cb518fe7f22ef27202428688f86404e8278587017012"
    "c1d65537c6cbd7dde04aae338d68115a9f430afc100ab83cdadf45dca39db685");

std::vector<uint8_t> kEncryptDataIV =
    wvcdm::a2b_hex("f6f4b1e600a5b67813ed2bded913ba9f");

uint32_t kSrmOutdatedVersion = 1;
uint32_t kSrmCurrentVersion = 3;

struct SrmTestParameters {
  const char* test_name;
  bool update_supported;
  uint32_t initial_version;
  uint32_t final_version;
  bool output_protection_required;  // otherwise requested
  bool revoked;
  bool expected_can_play_hd;
  std::set<wvcdm::KeyId> expected_key_ids;
};

SrmTestParameters kSrmTestConfiguration[] = {
    {"VersionCurrent|OPRequested|Updatable|NotRevoked|CanPlayback",
     true /* updatable */, kSrmCurrentVersion, kSrmCurrentVersion,
     false /* op requested */, false /* not revoked */, true /* can play */,
     kSrmExpectedAllKeyIds},
    {"VersionCurrent|OPRequested|NotUpdatable|NotRevoked|CanPlayback",
     false /* not updatable */, kSrmCurrentVersion, kSrmCurrentVersion,
     false /* op requested */, false /* not revoked */, true /* can play */,
     kSrmExpectedAllKeyIds},
    {"VersionCurrent|OPRequired|Updatable|NotRevoked|CanPlayback",
     true /* updatable */, kSrmCurrentVersion, kSrmCurrentVersion,
     true /* op required */, false /* not revoked */, true /* can play */,
     kSrmExpectedAllKeyIds},
    {"VersionCurrent|OPRequired|NotUpdatable|NotRevoked|CanPlayback",
     false /* not updatable */, kSrmCurrentVersion, kSrmCurrentVersion,
     true /* op required */, false /* not revoked */, true /* can play */,
     kSrmExpectedAllKeyIds},

    {"VersionOutdated|OPRequested|Updatable|NotRevoked|CanPlayback",
     true /* updatable */, kSrmOutdatedVersion, kSrmCurrentVersion,
     false /* op requested */, false /* not revoked */, true /* can play */,
     kSrmExpectedAllKeyIds},
    {"VersionOutdated|OPRequested|NotUpdatable|NotRevoked|CanPlayback",
     false /* not updatable */, kSrmOutdatedVersion, kSrmOutdatedVersion,
     false /* op requested */, false /* not revoked */, true /* can play */,
     kSrmExpectedAllKeyIds},
    {"VersionOutdated|OPRequired|Updatable|NotRevoked|CanPlayback",
     true /* updatable */, kSrmOutdatedVersion, kSrmCurrentVersion,
     true /* op required */, false /* not revoked */, true /* can play */,
     kSrmExpectedAllKeyIds},
    {"VersionOutdated|OPRequired|NotUpdatable|NotRevoked|CannotPlayback",
     false /* not updatable */, kSrmOutdatedVersion, kSrmOutdatedVersion,
     true /* op required */, false /* not revoked */, false /* cannot play */,
     kSrmExpectedSdKeyIds},

    {"VersionCurrent|OPRequested|Updatable|Revoked|CanPlayback",
     true /* updatable */, kSrmCurrentVersion, kSrmCurrentVersion,
     false /* op requested */, true /* revoked */, true /* can play */,
     kSrmExpectedAllKeyIds},
    {"VersionCurrent|OPRequested|NotUpdatable|Revoked|CanPlayback",
     false /* not updatable */, kSrmCurrentVersion, kSrmCurrentVersion,
     false /* op requested */, true /* revoked */, true /* can play */,
     kSrmExpectedAllKeyIds},
    {"VersionCurrent|OPRequired|Updatable|Revoked|CannotPlayback",
     true /* updatable */, kSrmCurrentVersion, kSrmCurrentVersion,
     true /* op required */, true /* revoked */, false /* cannot play */,
     kSrmExpectedSdKeyIds},
    {"VersionCurrent|OPRequired|NotUpdatable|Revoked|CannotPlayback",
     false /* not updatable */, kSrmCurrentVersion, kSrmCurrentVersion,
     true /* op required */, true /* revoked */, false /* cannot play */,
     kSrmExpectedSdKeyIds},

    {"VersionOutdated|OPRequested|Updatable|Revoked|CanPlayback",
     true /* updatable */, kSrmOutdatedVersion, kSrmCurrentVersion,
     false /* op requested */, true /* revoked */, true /* can play */,
     kSrmExpectedAllKeyIds},
    {"VersionOutdated|OPRequested|NotUpdatable|Revoked|CanPlayback",
     false /* not updatable */, kSrmOutdatedVersion, kSrmOutdatedVersion,
     false /* op requested */, true /* revoked */, true /* can play */,
     kSrmExpectedAllKeyIds},
    {"VersionOutdated|OPRequired|Updatable|Revoked|CannotPlayback",
     true /* updatable */, kSrmOutdatedVersion, kSrmCurrentVersion,
     true /* op required */, true /* revoked */, false /* cannot play */,
     kSrmExpectedSdKeyIds},
    {"VersionOutdated|OPRequired|NotUpdatable|Revoked|CannotPlayback",
     false /* not updatable */, kSrmOutdatedVersion, kSrmOutdatedVersion,
     true /* op required */, true /* revoked */, false /* cannot play */,
     kSrmExpectedSdKeyIds},
};

SrmTestParameters kSrmNotSupportedTestConfiguration[] = {
    {"VersionOutdated|OPRequested|NotUpdatable|NotRevoked|CanPlayback",
     false /* updatable */, kSrmOutdatedVersion, kSrmOutdatedVersion,
     false /* op requested */, false /* not revoked */, true /* can play */,
     kSrmExpectedAllKeyIds},
    {"VersionOutdated|OPRequired|NotUpdatable|NotRevoked|CannotPlayback",
     false /* updatable */, kSrmOutdatedVersion, kSrmOutdatedVersion,
     true /* op required */, false /* not revoked */, false /* cannot play */,
     kSrmExpectedSdKeyIds},
};

}  // namespace

namespace wvcdm {
// Protobuf generated classes
using video_widevine::ClientIdentification_TokenType;
using video_widevine::ClientIdentification_TokenType_KEYBOX;
using video_widevine::ClientIdentification_TokenType_OEM_DEVICE_CERTIFICATE;
using video_widevine::ProvisioningRequest;
using video_widevine::SignedProvisioningMessage;

class TestWvCdmEventListener : public WvCdmEventListener {
 public:
  TestWvCdmEventListener() : WvCdmEventListener() {}
  MOCK_METHOD1(OnSessionRenewalNeeded, void(const CdmSessionId& session_id));
  MOCK_METHOD3(OnSessionKeysChange, void(const CdmSessionId& session_id,
                                         const CdmKeyStatusMap& keys_status,
                                         bool has_new_usable_key));
  MOCK_METHOD2(OnExpirationUpdate, void(const CdmSessionId& session_id,
                                        int64_t new_expiry_time_seconds));
};

class TestKeyVerifier {
 public:
  TestKeyVerifier() {}
  TestKeyVerifier(std::set<KeyId> expected_usable_key_ids)
      : expected_usable_key_ids_(expected_usable_key_ids) {}

  void Verify(const CdmSessionId& /* session_id */,
              const CdmKeyStatusMap& status_map,
              bool /* has_new_usable_key */) {
    // LOGE("TestKeyVerifier::Verify: session_id: %s", session_id.c_str());
    // CdmKeyStatusMap key_status = status_map;
    // const CdmKeyStatusMap::iterator iter;
    uint32_t number_of_usable_keys = 0;
    for (CdmKeyStatusMap::const_iterator iter = status_map.begin();
         iter != status_map.end(); ++iter) {
      if (iter->second == kKeyStatusUsable) {
        ++number_of_usable_keys;
        EXPECT_NE(expected_usable_key_ids_.end(),
                  expected_usable_key_ids_.find(iter->first));
      }
      // LOGE("TestKeyVerifier::Verify: %s %d", b2a_hex(iter->first).c_str(),
      // iter->second);
    }
    EXPECT_EQ(expected_usable_key_ids_.size(), number_of_usable_keys);
  }

 private:
  std::set<KeyId> expected_usable_key_ids_;
};

class WvCdmFeatureTest : public WvCdmTestBase {
 public:
  WvCdmFeatureTest() {}
  ~WvCdmFeatureTest() {}

  void LogResponseError(const std::string& message, int http_status_code) {
    LOGD("HTTP Status code = %d", http_status_code);
    LOGD("HTTP response(%d): %s", message.size(), b2a_hex(message).c_str());
  }

  // Post a request and extract the signed provisioning message from
  // the HTTP response.
  std::string GetCertRequestResponse(const std::string& server_url) {
    // Use secure connection and chunk transfer coding.
    UrlRequest url_request(server_url);
    EXPECT_TRUE(url_request.is_connected())
        << "Fail to connect to " << server_url;
    url_request.PostCertRequestInQueryString(key_msg_);
    std::string message;
    EXPECT_TRUE(url_request.GetResponse(&message));

    int http_status_code = url_request.GetStatusCode(message);
    if (kHttpOk != http_status_code) {
      LogResponseError(message, http_status_code);
    }
    EXPECT_EQ(kHttpOk, http_status_code);
    return message;
  }

 protected:
  bool ExtractTokenType(const std::string& b64_provisioning_request_no_pad,
                        ClientIdentification_TokenType* token_type) {
    std::string b64_provisioning_request = b64_provisioning_request_no_pad;
    size_t binary_size = b64_provisioning_request.size() * 3 / 4;
    // base64 message with pad = 4*ceil[n/3]
    size_t pad_size =
        ((binary_size + 2) / 3) * 4 - b64_provisioning_request.size();
    if (pad_size >= 3) return false;
    b64_provisioning_request.append(pad_size, '=');
    LOGW("ExtractTokenType: pad_size: %d", pad_size);

    std::vector<uint8_t> bin_provisioning_request =
        Base64SafeDecode(b64_provisioning_request);
    if (bin_provisioning_request.empty()) return false;
    std::string prov_request(bin_provisioning_request.begin(),
                             bin_provisioning_request.end());

    SignedProvisioningMessage signed_provisioning_message;
    if (!signed_provisioning_message.ParseFromString(prov_request))
      return false;

    ProvisioningRequest provisioning_request;
    if (!provisioning_request.ParseFromString(
            signed_provisioning_message.message()))
      return false;

    *token_type = provisioning_request.client_id().type();
    LOGW("ExtractTokenType: success");
    return true;
  }

  void GenerateKeyRequest(const std::string& init_data,
                          CdmLicenseType license_type) {
    GenerateKeyRequest(init_data, license_type, NULL);
  }

  void GenerateKeyRequest(const std::string& init_data,
                          CdmLicenseType license_type,
                          CdmClientPropertySet* property_set) {
    CdmAppParameterMap app_parameters;
    GenerateKeyRequest(init_data, app_parameters, license_type, property_set);
  }

  void GenerateKeyRequest(const std::string& init_data,
                          CdmAppParameterMap& app_parameters,
                          CdmLicenseType license_type,
                          CdmClientPropertySet* property_set) {
    std::string init_data_type = "video/mp4";
    GenerateKeyRequest(wvcdm::KEY_MESSAGE, init_data_type, init_data,
                       app_parameters, license_type, property_set);
  }

  void GenerateKeyRequest(CdmResponseType expected_response,
                          const std::string& init_data_type,
                          const std::string& init_data,
                          CdmAppParameterMap& app_parameters,
                          CdmLicenseType license_type,
                          CdmClientPropertySet* property_set) {
    CdmKeyRequest key_request;
    std::string key_set_id;
    license_type_ = license_type;
    EXPECT_EQ(
        expected_response,
        decryptor_.GenerateKeyRequest(
            session_id_, key_set_id, init_data_type, init_data, license_type,
            app_parameters, property_set, kDefaultCdmIdentifier, &key_request));
    key_msg_ = key_request.message;
    EXPECT_EQ(0u, key_request.url.size());
  }

  // Post a request and extract the drm message from the response
  std::string GetKeyRequestResponse(const std::string& server_url,
                                    const std::string& client_auth) {
    // Use secure connection and chunk transfer coding.
    UrlRequest url_request(server_url + client_auth);
    EXPECT_TRUE(url_request.is_connected())
        << "Fail to connect to " << server_url << client_auth;
    url_request.PostRequest(key_msg_);
    std::string message;
    EXPECT_TRUE(url_request.GetResponse(&message));

    int http_status_code = url_request.GetStatusCode(message);
    if (kHttpOk != http_status_code) {
      LogResponseError(message, http_status_code);
    }
    EXPECT_EQ(kHttpOk, http_status_code);

    std::string drm_msg;
    if (kHttpOk == http_status_code) {
      LicenseRequest lic_request;
      lic_request.GetDrmMessage(message, drm_msg);
      LOGV("HTTP response body: (%u bytes)", drm_msg.size());
    }
    return drm_msg;
  }

  void VerifyKeyRequestResponse(const std::string& server_url,
                                const std::string& client_auth) {
    std::string response;
    VerifyKeyRequestResponse(server_url, client_auth, false);
  }

  void VerifyUsageKeyRequestResponse(const std::string& server_url,
                                     const std::string& client_auth) {
    std::string response;
    VerifyKeyRequestResponse(server_url, client_auth, true);
  }

  void VerifyKeyRequestResponse(const std::string& server_url,
                                const std::string& client_auth, bool is_usage) {
    std::string response;
    VerifyKeyRequestResponse(server_url, client_auth, is_usage, &response);
  }

  void VerifyKeyRequestResponse(const std::string& server_url,
                                const std::string& client_auth, bool is_usage,
                                std::string* response) {
    *response = GetKeyRequestResponse(server_url, client_auth);

    EXPECT_EQ(decryptor_.AddKey(session_id_, *response, &key_set_id_),
              wvcdm::KEY_ADDED);
    EXPECT_EQ(is_usage || license_type_ == kLicenseTypeOffline,
              key_set_id_.size() > 0);
  }

 protected:
  wvcdm::WvContentDecryptionModule decryptor_;
  CdmKeyMessage key_msg_;
  CdmKeySetId key_set_id_;
  CdmLicenseType license_type_;
  CdmSessionId session_id_;
};

// To run this test set options,
// * use_keybox 0
TEST_F(WvCdmFeatureTest, OEMCertificateProvisioning) {
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  std::string provisioning_server_url;
  CdmCertificateType cert_type = kCertificateWidevine;
  std::string cert_authority, cert, wrapped_key;

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.GetProvisioningRequest(cert_type, cert_authority,
                                              kDefaultCdmIdentifier,
                                              kEmptyServiceCertificate,
                                              &key_msg_,
                                              &provisioning_server_url));
  EXPECT_EQ(provisioning_server_url, config_.provisioning_server());

  ClientIdentification_TokenType token_type;
  EXPECT_TRUE(ExtractTokenType(key_msg_, &token_type));
  EXPECT_EQ(ClientIdentification_TokenType_OEM_DEVICE_CERTIFICATE, token_type);

  provisioning_server_url =
      "https://staging-www.sandbox.googleapis.com/"
      "certificateprovisioning/v1/devicecertificates/create"
      "?key=AIzaSyB-5OLKTx2iU5mko18DfdwK5611JIjbUhE";

  std::string response = GetCertRequestResponse(provisioning_server_url);
  // GetCertRequestResponse(config_.provisioning_server_url());
  EXPECT_NE(0, static_cast<int>(response.size()));
  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.HandleProvisioningResponse(
                kDefaultCdmIdentifier, response, &cert, &wrapped_key));
  EXPECT_EQ(0, static_cast<int>(cert.size()));
  EXPECT_EQ(0, static_cast<int>(wrapped_key.size()));
  decryptor_.CloseSession(session_id_);
}

// To run this test set options,
// * use_keybox 1
TEST_F(WvCdmFeatureTest, KeyboxProvisioning) {
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  std::string provisioning_server_url;
  CdmCertificateType cert_type = kCertificateWidevine;
  std::string cert_authority, cert, wrapped_key;

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.GetProvisioningRequest(cert_type, cert_authority,
                                              kDefaultCdmIdentifier,
                                              kEmptyServiceCertificate,
                                              &key_msg_,
                                              &provisioning_server_url));
  EXPECT_EQ(provisioning_server_url, config_.provisioning_server());

  ClientIdentification_TokenType token_type;
  EXPECT_TRUE(ExtractTokenType(key_msg_, &token_type));
  EXPECT_EQ(ClientIdentification_TokenType_KEYBOX, token_type);

  provisioning_server_url =
      "https://staging-www.sandbox.googleapis.com/"
      "certificateprovisioning/v1/devicecertificates/create"
      "?key=AIzaSyB-5OLKTx2iU5mko18DfdwK5611JIjbUhE";

  std::string response = GetCertRequestResponse(provisioning_server_url);

  EXPECT_NE(0, static_cast<int>(response.size()));
  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.HandleProvisioningResponse(
                kDefaultCdmIdentifier, response, &cert, &wrapped_key));
  EXPECT_EQ(0, static_cast<int>(cert.size()));
  EXPECT_EQ(0, static_cast<int>(wrapped_key.size()));
  decryptor_.CloseSession(session_id_);
}

// These parameterized tests validate SRM scenarios described in
// SRM End-to-End Test Plan doc
class WvCdmSrmTest : public WvCdmFeatureTest,
                     public ::testing::WithParamInterface<SrmTestParameters*> {
};

TEST_P(WvCdmSrmTest, Srm) {
  SrmTestParameters* config = GetParam();

  std::string str;
  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevelDefault,
                                   wvcdm::QUERY_KEY_CURRENT_SRM_VERSION, &str));

  std::istringstream ss(str);
  uint32_t value;
  ss >> value;
  EXPECT_FALSE(ss.fail());
  EXPECT_TRUE(ss.eof());
  EXPECT_EQ(config->initial_version, value);

  StrictMock<TestWvCdmEventListener> listener;
  TestKeyVerifier verify_keys_callback(config->expected_key_ids);
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         &listener, &session_id_);

  EXPECT_CALL(listener,
              OnSessionKeysChange(
                  session_id_,
                  AllOf(Each(Pair(_, kKeyStatusUsable)), Not(IsEmpty())), true))
      .WillOnce(Invoke(&verify_keys_callback, &TestKeyVerifier::Verify));
  EXPECT_CALL(listener, OnExpirationUpdate(session_id_, _));

  CdmInitData init_data =
      config->output_protection_required
          ? ConfigTestEnv::GetInitData(
                kContentIdStagingSrmOuputProtectionRequired)
          : ConfigTestEnv::GetInitData(
                kContentIdStagingSrmOuputProtectionRequested);

  GenerateKeyRequest(init_data, kLicenseTypeStreaming);
  VerifyKeyRequestResponse(
      ConfigTestEnv::GetLicenseServerUrl(kContentProtectionStagingServer),
      config_.client_auth());

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevelDefault,
                                   wvcdm::QUERY_KEY_CURRENT_SRM_VERSION, &str));

  ss.clear();
  ss.str(str);
  ss >> value;
  EXPECT_FALSE(ss.fail());
  EXPECT_TRUE(ss.eof());
  EXPECT_EQ(config->final_version, value);

  decryptor_.CloseSession(session_id_);
}

INSTANTIATE_TEST_CASE_P(
    Cdm, WvCdmSrmTest,
    ::testing::Range(&kSrmTestConfiguration[0],
                     &kSrmTestConfiguration[N_ELEM(kSrmTestConfiguration)]));

// These parameterized tests validate SRM scenarios described in
// SRM End-to-End Test Plan doc
class WvCdmSrmNotSupportedTest
    : public WvCdmFeatureTest,
      public ::testing::WithParamInterface<SrmTestParameters*> {};

TEST_P(WvCdmSrmNotSupportedTest, Srm) {
  SrmTestParameters* config = GetParam();

  std::string str;
  EXPECT_NE(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevelDefault,
                                   wvcdm::QUERY_KEY_CURRENT_SRM_VERSION, &str));

  StrictMock<TestWvCdmEventListener> listener;
  TestKeyVerifier verify_keys_callback(config->expected_key_ids);
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         &listener, &session_id_);

  EXPECT_CALL(listener,
              OnSessionKeysChange(
                  session_id_,
                  AllOf(Each(Pair(_, kKeyStatusUsable)), Not(IsEmpty())), true))
      .WillOnce(Invoke(&verify_keys_callback, &TestKeyVerifier::Verify));
  EXPECT_CALL(listener, OnExpirationUpdate(session_id_, _));

  CdmInitData init_data =
      config->output_protection_required
          ? ConfigTestEnv::GetInitData(
                kContentIdStagingSrmOuputProtectionRequired)
          : ConfigTestEnv::GetInitData(
                kContentIdStagingSrmOuputProtectionRequested);

  GenerateKeyRequest(init_data, kLicenseTypeStreaming);
  VerifyKeyRequestResponse(
      ConfigTestEnv::GetLicenseServerUrl(kContentProtectionStagingServer),
      config_.client_auth());

  EXPECT_NE(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevelDefault,
                                   wvcdm::QUERY_KEY_CURRENT_SRM_VERSION, &str));

  decryptor_.CloseSession(session_id_);
}

INSTANTIATE_TEST_CASE_P(
    Cdm, WvCdmSrmNotSupportedTest,
    ::testing::Range(&kSrmNotSupportedTestConfiguration[0],
                     &kSrmNotSupportedTestConfiguration[N_ELEM(
                         kSrmNotSupportedTestConfiguration)]));
}  // namespace wvcdm
