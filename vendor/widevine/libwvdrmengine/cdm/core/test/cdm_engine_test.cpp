// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
// These tests are for the cdm engine, and code below it in the stack.  In
// particular, we assume that the OEMCrypo layer works, and has a valid keybox.
// This is because we need a valid RSA certificate, and will attempt to connect
// to the provisioning server to request one if we don't.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "cdm_engine.h"
#include "config_test_env.h"
#include "device_files.h"
#include "initialization_data.h"
#include "file_store.h"
#include "license_request.h"
#include "log.h"
#include "metrics.pb.h"
#include "OEMCryptoCENC.h"
#include "properties.h"
#include "string_conversions.h"
#include "test_base.h"
#include "test_printers.h"
#include "url_request.h"
#include "wv_cdm_constants.h"
#include "wv_cdm_types.h"

namespace wvcdm {

using drm_metrics::DistributionMetric;
using drm_metrics::WvCdmMetrics;
using metrics::EngineMetrics;

namespace {

// Http OK response code.
const int kHttpOk = 200;

const std::string kCencMimeType = "video/mp4";
const std::string kWebmMimeType = "video/webm";
const std::string kEmptyString;
const std::string kComma = ",";

}  // namespace

class WvCdmEnginePreProvTest : public WvCdmTestBase {
 public:
  WvCdmEnginePreProvTest()
      : dummy_engine_metrics_(new EngineMetrics),
        cdm_engine_(&file_system_, dummy_engine_metrics_),
        session_opened_(false) {}

  ~WvCdmEnginePreProvTest() override {}

  void SetUp() override {
    WvCdmTestBase::SetUp();
    session_opened_ = false;
  }

  virtual void OpenSession() {
    CdmResponseType status =
        cdm_engine_.OpenSession(config_.key_system(), NULL, NULL, &session_id_);
    if (status == NEED_PROVISIONING) {
      Provision();
      status = cdm_engine_.OpenSession(config_.key_system(), NULL, NULL,
                                       &session_id_);
    }
    ASSERT_EQ(status, NO_ERROR);
    ASSERT_NE("", session_id_) << "Could not open CDM session.";
    ASSERT_TRUE(cdm_engine_.IsOpenSession(session_id_));
    session_opened_ = true;
  }

  void TearDown() override {
    if (cdm_engine_.IsProvisioned(kSecurityLevelL1)) {
      cdm_engine_.Unprovision(kSecurityLevelL1);
    }
    if (cdm_engine_.IsProvisioned(kSecurityLevelL3)) {
      cdm_engine_.Unprovision(kSecurityLevelL3);
    }
    if (session_opened_) {
      cdm_engine_.CloseSession(session_id_);
      session_opened_ = false;
    }
  }

 protected:
  // Trade request for response via the license server.
  virtual bool LicenseServerRequestResponse(const std::string& request,
                                            std::string* response) {
    LOGV("LicenseServerRequestResponse: server url: %s",
         config_.license_server().c_str());
    UrlRequest url_request(config_.license_server() + config_.client_auth());
    url_request.PostRequest(request);

    std::string http_response;
    if (!url_request.GetResponse(&http_response)) {
      return false;
    }

    LOGV("http_response:\n%s\n", http_response.c_str());

    // Separate message from HTTP headers.
    LicenseRequest license_request;
    license_request.GetDrmMessage(http_response, *response);

    LOGV("response: size=%u, string:\n%s\n", response->size(),
         Base64SafeEncode(
             std::vector<uint8_t>(response->begin(), response->end()))
             .c_str());
    return true;
  }

  FileSystem file_system_;
  shared_ptr<metrics::EngineMetrics> dummy_engine_metrics_;
  CdmEngine cdm_engine_;
  bool session_opened_;
  std::string key_msg_;
  std::string session_id_;
};

class WvCdmEnginePreProvTestStaging : public WvCdmEnginePreProvTest {
 public:
  WvCdmEnginePreProvTestStaging() {
    config_ = ConfigTestEnv(kContentProtectionStagingServer);
  }
};

class WvCdmEnginePreProvTestProd : public WvCdmEnginePreProvTest {
 public:
  WvCdmEnginePreProvTestProd() {
    config_ = ConfigTestEnv(kContentProtectionProductionServer);
  }
};

class WvCdmEnginePreProvTestUat : public WvCdmEnginePreProvTest {
 public:
  WvCdmEnginePreProvTestUat() {
    config_ = ConfigTestEnv(kContentProtectionUatServer);
  }
};

class WvCdmEnginePreProvTestUatBinary : public WvCdmEnginePreProvTest {
 public:
  WvCdmEnginePreProvTestUatBinary() {
    config_ = ConfigTestEnv(kContentProtectionUatServer);
    // Override default setting of provisioning_messages_are_binary property
    binary_provisioning_ = true;
  }
};

class WvCdmEngineTest : public WvCdmEnginePreProvTest {
 public:
  WvCdmEngineTest() {}

  void SetUp() override {
    WvCdmTestBase::SetUp();
    session_opened_ = false;
    WvCdmEnginePreProvTest::OpenSession();
  }

 protected:
  void GenerateKeyRequest(const std::string& key_id,
                          const std::string& init_data_type_string) {
    GenerateKeyRequest(key_id, init_data_type_string, kLicenseTypeStreaming);
  }

  void GenerateKeyRequest(const std::string& key_id,
                          const std::string& init_data_type_string,
                          CdmLicenseType license_type) {
    CdmAppParameterMap app_parameters;
    CdmKeySetId key_set_id;

    InitializationData init_data(init_data_type_string, key_id);

    CdmKeyRequest key_request;

    CdmResponseType result = cdm_engine_.GenerateKeyRequest(
        session_id_, key_set_id, init_data, license_type,
        app_parameters, &key_request);
    EXPECT_EQ(KEY_MESSAGE, result);

    key_msg_ = key_request.message;
    EXPECT_EQ(kKeyRequestTypeInitial, key_request.type);
  }

  void GenerateRenewalRequest() {
    CdmKeyRequest request;
    CdmResponseType result =
        cdm_engine_.GenerateRenewalRequest(session_id_, &request);
    EXPECT_EQ(KEY_MESSAGE, result);

    key_msg_ = request.message;
    server_url_ = request.url;
  }

  std::string GetKeyRequestResponse(const std::string& server_url,
                                    const std::string& client_auth) {
    return GetKeyRequestResponse(server_url, client_auth, true);
  }

  std::string FailToGetKeyRequestResponse(const std::string& server_url,
                                          const std::string& client_auth) {
    return GetKeyRequestResponse(server_url, client_auth, false);
  }

  // posts a request and extracts the drm message from the response
  std::string GetKeyRequestResponse(const std::string& server_url,
                                    const std::string& client_auth,
                                    bool expect_success) {
    // Use secure connection and chunk transfer coding.

    LOGV("GetKeyRequestResponse: server_url: %s", server_url.c_str());

    UrlRequest url_request(server_url + client_auth);
    if (!url_request.is_connected()) {
      return "";
    }

    url_request.PostRequest(key_msg_);
    std::string response;
    bool ok = url_request.GetResponse(&response);
    LOGV("response: %s\n", response.c_str());
    EXPECT_TRUE(ok);

    int status_code = url_request.GetStatusCode(response);
    if (expect_success)
      EXPECT_EQ(kHttpOk, status_code) << "Error response: " << response;

    if (status_code != kHttpOk) {
      return "";
    } else {
      std::string drm_msg;
      LicenseRequest lic_request;
      lic_request.GetDrmMessage(response, drm_msg);
      LOGV("drm msg: %u bytes\r\n%s", drm_msg.size(),
           HexEncode(reinterpret_cast<const uint8_t*>(drm_msg.data()),
                     drm_msg.size())
               .c_str());
      return drm_msg;
    }
  }

  void VerifyNewKeyResponse(const std::string& server_url,
                            const std::string& client_auth) {
    VerifyNewKeyResponse(server_url, client_auth, kLicenseTypeStreaming);
  }
  void VerifyNewKeyResponse(const std::string& server_url,
                            const std::string& client_auth,
                            CdmLicenseType expected_license_type) {
    std::string resp = GetKeyRequestResponse(server_url, client_auth);
    CdmKeySetId key_set_id;
    CdmLicenseType license_type;
    CdmResponseType status =
        cdm_engine_.AddKey(session_id_, resp, &license_type, &key_set_id);

    EXPECT_EQ(KEY_ADDED, status);
    EXPECT_EQ(expected_license_type, license_type);
    VerifyLicenseRequestLatency(kKeyRequestTypeInitial,
                                *dummy_engine_metrics_);
  }

  void VerifyRenewalKeyResponse(const std::string& server_url,
                                const std::string& client_auth) {
    std::string resp = GetKeyRequestResponse(server_url, client_auth);
    EXPECT_EQ(KEY_ADDED, cdm_engine_.RenewKey(session_id_, resp));
    VerifyLicenseRequestLatency(kKeyRequestTypeRenewal,
                                *dummy_engine_metrics_);
  }

  void VerifyLicenseRequestLatency(
      CdmKeyRequestType key_request_type,
      const metrics::EngineMetrics& engine_metrics) {
    WvCdmMetrics metrics_proto;
    engine_metrics.Serialize(&metrics_proto);
    bool has_request_type = false;
    for (int i = 0; i < metrics_proto.session_metrics_size(); i++) {
      WvCdmMetrics::SessionMetrics session_metrics =
          metrics_proto.session_metrics(i);
      for (int j = 0;
           j < session_metrics.cdm_session_license_request_latency_ms_size();
           j++) {
        DistributionMetric latency_distribution =
            session_metrics.cdm_session_license_request_latency_ms(j);
        if (latency_distribution.attributes().key_request_type() ==
                key_request_type &&
            latency_distribution.operation_count() > 0) {
          has_request_type = true;
        }
      }
    }
    std::string serialized_metrics;
    ASSERT_TRUE(metrics_proto.SerializeToString(&serialized_metrics));
    EXPECT_TRUE(has_request_type)
        << "Expected request type " << key_request_type << " was not found. "
        << "metrics: " << wvcdm::b2a_hex(serialized_metrics);
  }

  std::string server_url_;
};

// Tests to validate service certificate
TEST_F(WvCdmEnginePreProvTestUat, ProvisioningServiceCertificateValidTest) {
  ASSERT_EQ(cdm_engine_.ValidateServiceCertificate(
                config_.provisioning_service_certificate()),
            NO_ERROR);
};

TEST_F(WvCdmEnginePreProvTestUat, ProvisioningServiceCertificateInvalidTest) {
  std::string certificate = config_.provisioning_service_certificate();
  // Add four nulls to the beginning of the cert to invalidate it
  certificate.insert(0, 4, '\0');

  ASSERT_NE(cdm_engine_.ValidateServiceCertificate(certificate), NO_ERROR);
};

TEST_F(WvCdmEnginePreProvTestStaging, ProvisioningTest) { Provision(); }

TEST_F(WvCdmEnginePreProvTestUatBinary, ProvisioningTest) {
  Provision();
}

// Test that provisioning works.
TEST_F(WvCdmEngineTest, ProvisioningTest) {
  Provision();
}

// Test that provisioning works, even if device is already provisioned.
TEST_F(WvCdmEngineTest, ReprovisioningTest) {
  // Provision once.
  Provision();
  // Verify that we can provision a second time, even though we already
  // provisioned once.
  Provision();
}

TEST_F(WvCdmEngineTest, BaseIsoBmffMessageTest) {
  GenerateKeyRequest(binary_key_id(), kCencMimeType);
  GetKeyRequestResponse(config_.license_server(), config_.client_auth());
}

// TODO(juce): Set up with correct test data.
TEST_F(WvCdmEngineTest, DISABLED_BaseWebmMessageTest) {
  // Extract the key ID from the PSSH box.
  InitializationData extractor(CENC_INIT_DATA_FORMAT, binary_key_id());
  KeyId key_id_unwrapped = extractor.data();
  GenerateKeyRequest(key_id_unwrapped, kWebmMimeType);
  GetKeyRequestResponse(config_.license_server(), config_.client_auth());
}

TEST_F(WvCdmEngineTest, NormalDecryptionIsoBmff) {
  GenerateKeyRequest(binary_key_id(), kCencMimeType);
  VerifyNewKeyResponse(config_.license_server(), config_.client_auth());
}

// TODO(blueeyes): Add tests for different license types.
TEST_F(WvCdmEngineTest, ReturnsLicenseTypeDetailStreaming) {
  GenerateKeyRequest(binary_key_id(), kCencMimeType, kLicenseTypeStreaming);
  VerifyNewKeyResponse(config_.license_server(), config_.client_auth(),
                       kLicenseTypeStreaming);
}

// TODO(juce): Set up with correct test data.
TEST_F(WvCdmEngineTest, DISABLED_NormalDecryptionWebm) {
  // Extract the key ID from the PSSH box.
  InitializationData extractor(CENC_INIT_DATA_FORMAT, binary_key_id());
  KeyId key_id_unwrapped = extractor.data();
  GenerateKeyRequest(key_id_unwrapped, kWebmMimeType);
  VerifyNewKeyResponse(config_.license_server(), config_.client_auth());
}

TEST_F(WvCdmEngineTest, LoadKey) {
  EnsureProvisioned();
  TestLicenseHolder holder(&cdm_engine_);
  holder.OpenSession(config_.key_system());
  holder.GenerateKeyRequest(binary_key_id(), ISO_BMFF_VIDEO_MIME_TYPE);
  holder.CreateDefaultLicense();
  std::vector<uint8_t> key_data(CONTENT_KEY_SIZE, '1');
  wvoec::KeyControlBlock block = {};
  holder.AddKey("key_one", key_data, block);
  holder.SignAndLoadLicense();
}

TEST_F(WvCdmEngineTest, LicenseRenewal) {
  GenerateKeyRequest(binary_key_id(), kCencMimeType);
  VerifyNewKeyResponse(config_.license_server(), config_.client_auth());

  GenerateRenewalRequest();
  VerifyRenewalKeyResponse(
      server_url_.empty() ? config_.license_server() : server_url_,
      config_.client_auth());
}

TEST_F(WvCdmEngineTest, ParseDecryptHashStringTest) {

  CdmSessionId session_id;
  uint32_t frame_number;
  std::string hash;

  const std::string test_session_id = "sample session id";
  uint32_t test_frame_number = 5;
  const std::string test_frame_number_string =
      std::to_string(test_frame_number);
  const std::string test_invalid_hash = "an invalid hash";
  std::vector<uint8_t> binary_hash{ 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0xFF };
  const std::string test_valid_decoded_hash(binary_hash.begin(),
                                            binary_hash.end());
  const std::string test_valid_hash = b2a_hex(binary_hash);
  const std::string test_invalid_hash_string = "sample hash string";
  const std::string test_valid_hash_string = test_session_id + kComma +
      test_frame_number_string + kComma + test_valid_hash;

  // invalid parameters
  EXPECT_EQ(PARAMETER_NULL,
            CdmEngine::ParseDecryptHashString(test_valid_hash_string, nullptr,
                                              &frame_number, &hash));
  EXPECT_EQ(PARAMETER_NULL,
            CdmEngine::ParseDecryptHashString(test_valid_hash_string,
                                              &session_id, nullptr, &hash));
  EXPECT_EQ(PARAMETER_NULL,
            CdmEngine::ParseDecryptHashString(test_valid_hash_string,
                                              &session_id, &frame_number,
                                              nullptr));

  // Too few or too many parameters
  EXPECT_EQ(INVALID_DECRYPT_HASH_FORMAT,
            CdmEngine::ParseDecryptHashString(kEmptyString, &session_id,
                                              &frame_number, &hash));
  std::string hash_data = test_session_id;
  EXPECT_EQ(INVALID_DECRYPT_HASH_FORMAT,
            CdmEngine::ParseDecryptHashString(hash_data, &session_id,
                                              &frame_number, &hash));
  hash_data += kComma;
  EXPECT_EQ(INVALID_DECRYPT_HASH_FORMAT,
            CdmEngine::ParseDecryptHashString(hash_data, &session_id,
                                              &frame_number, &hash));
  hash_data += test_frame_number_string;
  EXPECT_EQ(INVALID_DECRYPT_HASH_FORMAT,
            CdmEngine::ParseDecryptHashString(hash_data, &session_id,
                                              &frame_number, &hash));
  hash_data += kComma;
  EXPECT_EQ(INVALID_DECRYPT_HASH_FORMAT,
            CdmEngine::ParseDecryptHashString(hash_data, &session_id,
                                              &frame_number, &hash));
  hash_data += test_valid_hash + kComma + test_valid_hash;
  EXPECT_EQ(INVALID_DECRYPT_HASH_FORMAT,
            CdmEngine::ParseDecryptHashString(hash_data, &session_id,
                                              &frame_number, &hash));

  // No parameters
  hash_data = kComma;
  EXPECT_EQ(INVALID_DECRYPT_HASH_FORMAT,
            CdmEngine::ParseDecryptHashString(hash_data, &session_id,
                                              &frame_number, &hash));
  hash_data = kComma + kComma;
  EXPECT_EQ(INVALID_DECRYPT_HASH_FORMAT,
            CdmEngine::ParseDecryptHashString(hash_data, &session_id,
                                              &frame_number, &hash));
  hash_data = kComma + kComma + kComma;
  EXPECT_EQ(INVALID_DECRYPT_HASH_FORMAT,
            CdmEngine::ParseDecryptHashString(hash_data, &session_id,
                                              &frame_number, &hash));

  // Missing parameters
  hash_data = kComma + test_frame_number_string + kComma + test_valid_hash;
  EXPECT_EQ(INVALID_DECRYPT_HASH_FORMAT,
            CdmEngine::ParseDecryptHashString(hash_data, &session_id,
                                              &frame_number, &hash));
  hash_data = test_session_id + kComma + kComma + test_valid_hash;
  EXPECT_EQ(INVALID_DECRYPT_HASH_FORMAT,
            CdmEngine::ParseDecryptHashString(hash_data, &session_id,
                                              &frame_number, &hash));
  hash_data = test_session_id + kComma + test_frame_number_string + kComma;
  EXPECT_EQ(INVALID_DECRYPT_HASH_FORMAT,
            CdmEngine::ParseDecryptHashString(hash_data, &session_id,
                                              &frame_number, &hash));
  hash_data = test_session_id + kComma + test_frame_number_string + kComma +
      kComma;
  EXPECT_EQ(INVALID_DECRYPT_HASH_FORMAT,
            CdmEngine::ParseDecryptHashString(hash_data, &session_id,
                                              &frame_number, &hash));

  // Invalid parameters (frame number, hash)
  hash_data = test_session_id + kComma + test_session_id + kComma +
      test_valid_hash;
  EXPECT_EQ(INVALID_DECRYPT_HASH_FORMAT,
            CdmEngine::ParseDecryptHashString(hash_data, &session_id,
                                              &frame_number, &hash));
  hash_data = test_session_id + kComma + test_frame_number_string + kComma +
      test_invalid_hash;
  EXPECT_EQ(INVALID_DECRYPT_HASH_FORMAT,
            CdmEngine::ParseDecryptHashString(hash_data, &session_id,
                                              &frame_number, &hash));

  // valid data
  EXPECT_EQ(NO_ERROR,
            CdmEngine::ParseDecryptHashString(test_valid_hash_string,
                                              &session_id, &frame_number,
                                              &hash));
  EXPECT_EQ(test_session_id, session_id);
  EXPECT_EQ(test_frame_number, frame_number);
  EXPECT_EQ(test_valid_decoded_hash, hash);
}


}  // namespace wvcdm
