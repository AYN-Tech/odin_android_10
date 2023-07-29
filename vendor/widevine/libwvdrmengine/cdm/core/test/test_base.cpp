// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
// This file adds some print methods so that when unit tests fail, the
// will print the name of an enumeration instead of the numeric value.

#include "test_base.h"

#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/cmac.h>
#include <stdlib.h>

#include <sstream>
#include <string>
#include <vector>

#include "cdm_engine.h"
#include "crypto_session.h"
#include "file_store.h"
#include "license.h"
#include "log.h"
#include "oec_device_features.h"
#include "oec_test_data.h"
#include "platform.h"
#include "properties.h"
#include "test_printers.h"
#include "url_request.h"

using wvcdm::metrics::EngineMetrics;

namespace wvcdm {
namespace {
void show_menu(char* prog_name) {
  std::cout << std::endl;
  std::cout << "usage: " << prog_name << " [options]" << std::endl << std::endl;
  std::cout << "  enclose multiple arguments in '' when using adb shell"
            << std::endl;
  std::cout << "  e.g. adb shell '" << prog_name << " --server=\"url\"'"
            << std::endl;

  std::cout << "  --verbose" << std::endl;
  std::cout << "    increase logging verbosity (may be repeated)" << std::endl
            << std::endl;

  std::cout << "  --no_filter" << std::endl;
  std::cout << "    Do not filter out inappropriate tests" << std::endl
            << std::endl;

  std::cout << "  --cast" << std::endl;
  std::cout << "    Run tests appropriate for a Cast Receiver" << std::endl
            << std::endl;

  std::cout << "  --license_server_id=<gp/cp/st>" << std::endl;
  std::cout << "    specifies which default server settings to use: "
            << std::endl;
  std::cout << "      gp for GooglePlay server" << std::endl;
  std::cout << "      cp for Content Protection UAT server" << std::endl;
  std::cout << "      st for Content Protection Staging server" << std::endl
            << std::endl;

  std::cout << "  --keyid=<key_id>" << std::endl;
  std::cout << "    configure the key id or pssh, in hex format" << std::endl
            << std::endl;

  std::cout << "  --service_certificate=<cert>" << std::endl;
  std::cout << "    configure the signed license service certificate" << std::endl;
  std::cout << "    Specify the SignedDeviceCertificate (from "
            << "device_certificate.proto) " << std::endl;
  std::cout << "    in hex format." << std::endl;
  std::cout << "    Due to the length of the argument use, " << std::endl;
  std::cout << "      echo \"/system/bin/request_license_test -s \\\""
            << "0ABF02...A29914\\\"\" \\" << std::endl;
  std::cout << "          > run_request_license_test.sh" << std::endl;
  std::cout << "      chmod +x run_request_license_test.sh" << std::endl;
  std::cout << "      adb push run_request_license_test.sh /system/bin"
            << std::endl;
  std::cout << "      adb shell sh /system/bin/run_request_license_test.sh"
            << std::endl << std::endl;

  std::cout << "  --provisioning_certificate=<cert>" << std::endl;
  std::cout << "    configure the signed provisioning service certificate" << std::endl
            << "    in hex" << std::endl << std::endl;

  std::cout << "  --license_server_url=<url>" << std::endl;
  std::cout << "    configure the license server url, please include http[s]"
            << "    in the url" << std::endl
            << std::endl;

  std::cout << "  --provisioning_server_url=<url>" << std::endl;
  std::cout << "    configure the provisioning server url, please include http[s]"
            << "    in the url" << std::endl
            << std::endl;
}

/*
 * Locate the portion of the server's response message that is between
 * the strings jason_start_substr and json_end_substr. Returns the string
 * through *result. If the start substring match fails, assume the entire
 * string represents a serialized protobuf mesaage and return true with
 * the entire string. If the end_substring match fails, return false with
 * an empty *result.
 */
bool ExtractSignedMessage(const std::string& response,
                          const std::string& json_start_substr,
                          const std::string& json_end_substr,
                          std::string* result) {
  std::string response_string;
  size_t start = response.find(json_start_substr);

  if (start == response.npos) {
    // Assume serialized protobuf message.
    result->assign(response);
  } else {
    // Assume JSON-wrapped protobuf.
    size_t end =
        response.find(json_end_substr, start + json_start_substr.length());
    if (end == response.npos) {
      LOGE("ExtractSignedMessage cannot locate end substring");
      result->clear();
      return false;
    }
    size_t result_string_size = end - start - json_start_substr.length();
    result->assign(response, start + json_start_substr.length(),
                   result_string_size);
  }

  if (result->empty()) {
    LOGE("ExtractSignedMessage: Response message is empty");
    return false;
  }
  return true;
}

}  // namespace

ConfigTestEnv WvCdmTestBase::default_config_(kContentProtectionUatServer);

void WvCdmTestBase::StripeBuffer(std::vector<uint8_t>* buffer, size_t size,
                                 uint8_t init) {
  buffer->assign(size, 0);
  for (size_t i = 0; i < size; i++) {
    (*buffer)[i] = init + i % 250;
  }
}

std::string WvCdmTestBase::Aes128CbcEncrypt(std::vector<uint8_t> key,
                                            const std::vector<uint8_t>& clear,
                                            const std::vector<uint8_t> iv) {
  std::vector<uint8_t> encrypted(clear.size());
  std::vector<uint8_t> iv_mod(iv.begin(), iv.end());
  AES_KEY aes_key;
  AES_set_encrypt_key(&key[0], 128, &aes_key);
  AES_cbc_encrypt(&clear[0], &encrypted[0], clear.size(), &aes_key, &iv_mod[0],
                  AES_ENCRYPT);
  return std::string(encrypted.begin(), encrypted.end());
}

std::string WvCdmTestBase::Aes128CbcDecrypt(std::vector<uint8_t> key,
                                            const std::vector<uint8_t>& clear,
                                            const std::vector<uint8_t> iv) {
  std::vector<uint8_t> encrypted(clear.size());
  std::vector<uint8_t> iv_mod(iv.begin(), iv.end());
  AES_KEY aes_key;
  AES_set_decrypt_key(&key[0], 128, &aes_key);
  AES_cbc_encrypt(&clear[0], &encrypted[0], clear.size(), &aes_key, &iv_mod[0],
                  AES_DECRYPT);
  return std::string(encrypted.begin(), encrypted.end());
}

std::string WvCdmTestBase::SignHMAC(const std::string& message,
                                    const std::vector<uint8_t>& key) {
  uint8_t signature[SHA256_DIGEST_LENGTH];
  unsigned int md_len = SHA256_DIGEST_LENGTH;
  HMAC(EVP_sha256(), &key[0], key.size(),
       reinterpret_cast<const uint8_t*>(message.data()), message.size(),
       signature, &md_len);
  std::string result(signature, signature + SHA256_DIGEST_LENGTH);
  return result;
}

TestCryptoSession::TestCryptoSession(metrics::CryptoMetrics* crypto_metrics)
    : CryptoSession(crypto_metrics) {
  // The first CryptoSession should have initialized OEMCrypto.  This is right
  // after that, so should tell oemcrypto to use a test keybox.
  if (session_count() == 1) {
    WvCdmTestBase::InstallTestRootOfTrust();
  }
}

CdmResponseType TestCryptoSession::GenerateNonce(uint32_t* nonce) {
  CdmResponseType status = CryptoSession::GenerateNonce(nonce);
  for (int i = 0; status != NO_ERROR; i++) {
    LOGV("Recovering from nonce flood.");
    if (i > 2) return status;
    sleep(1);
    status = CryptoSession::GenerateNonce(nonce);
  }
  return NO_ERROR;
}

class TestCryptoSessionFactory : public CryptoSessionFactory {
  CryptoSession* MakeCryptoSession(metrics::CryptoMetrics* crypto_metrics) {
    // We need to add extra locking here because we need to make sure that there
    // are no other OEMCrypto calls between OEMCrypto_Initialize and
    // InstallTestRootOfTrust.  OEMCrypto_Initialize is called in the production
    // CryptoSession::Init and is wrapped in crypto_lock_, but
    // InstallTestRootOfTrust is only called in the constructor of the
    // TestCryptoSession, above.
    std::unique_lock<std::mutex> auto_lock(init_lock_);
    return new TestCryptoSession(crypto_metrics);
  }
  std::mutex init_lock_;
};

void WvCdmTestBase::SetUp() {
  ::testing::Test::SetUp();
  Properties::Init();
  Properties::set_provisioning_messages_are_binary(binary_provisioning_);
  // Log the current test name, to help with debugging when the log and stdout
  // are not the same.
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  LOGD("Running test %s.%s", test_info->test_case_name(), test_info->name());
  // Some test environments allow the model name of the device to be set
  // dynamically using an environment variable.  The model name will show up in
  // the license server logs as the part of the device idenfication as
  // "model_name".
  std::stringstream ss;
  ss << test_info->test_case_name() << "." << test_info->name();
  int overwrite = 1;  // Set value even if already set.
  setenv("MODEL_NAME", ss.str().c_str(), overwrite);
  CryptoSession::SetCryptoSessionFactory(new TestCryptoSessionFactory());
  // TODO(fredgc): Add a test version of DeviceFiles.
}

void WvCdmTestBase::InstallTestRootOfTrust() {
  switch (wvoec::global_features.derive_key_method) {
    case wvoec::DeviceFeatures::LOAD_TEST_KEYBOX:
      // TODO(fredgc, b/119316243): REMOVE THIS! (and the lines below)
      if (wvoec::global_features.api_version < 14) {
        // This should work with a production android device, but will fail with
        // the keyboxless ce cdm, as shipped.  We are including this bit of code
        // so we can develop on Android, but plan to remove it when we have a
        // few more android test devices with v14 or v15 oemcrypto.
        LOGE("Attempting tests without test keybox.");
      } else { // TODO(fredgc, b/119316243): END OF REMOVE THIS!
        ASSERT_EQ(OEMCrypto_SUCCESS,
                  OEMCrypto_LoadTestKeybox(
                      reinterpret_cast<const uint8_t*>(&wvoec::kTestKeybox),
                      sizeof(wvoec::kTestKeybox)));
      }  // TODO(fredgc, b/119316243): yeah, yeah... remove this line, too.
      break;
    case wvoec::DeviceFeatures::LOAD_TEST_RSA_KEY:
      // Rare case: used by devices with baked in DRM cert.
      ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_LoadTestRSAKey());
      break;
    case wvoec::DeviceFeatures::TEST_PROVISION_30:
      // Can use oem certificate to install test rsa key.
      break;
    default:
      FAIL() << "Cannot run test without test keybox or RSA key installed.";
  }
}

void WvCdmTestBase::Provision() {
  CdmProvisioningRequest prov_request;
  CdmProvisioningRequest binary_prov_request;
  std::string provisioning_server_url;
  CdmCertificateType cert_type = kCertificateWidevine;
  std::string cert_authority;
  std::string cert, wrapped_key;

  CdmSessionId session_id;
  FileSystem file_system;

  // TODO(fredgc): provision for different SPOIDs.
  CdmEngine cdm_engine(&file_system,
                       std::shared_ptr<EngineMetrics>(new EngineMetrics));

  CdmResponseType result = cdm_engine.GetProvisioningRequest(
      cert_type, cert_authority, config_.provisioning_service_certificate(),
      &prov_request, &provisioning_server_url);
  ASSERT_EQ(NO_ERROR, result);

  if (binary_provisioning_) {
    binary_prov_request = prov_request;
    prov_request = std::string(Base64SafeEncodeNoPad(std::vector<uint8_t>(
        binary_prov_request.begin(), binary_prov_request.end())));
  }

  LOGV("WvCdmTestBase::Provision: req=%s", prov_request.c_str());

  // Ignore URL provided by CdmEngine.  Use ours, as configured
  // for test vs. production server.
  provisioning_server_url.assign(config_.provisioning_server());
  UrlRequest url_request(provisioning_server_url);
  EXPECT_TRUE(url_request.is_connected());
  url_request.PostCertRequestInQueryString(prov_request);
  std::string http_message;
  bool ok = url_request.GetResponse(&http_message);
  EXPECT_TRUE(ok) << http_message;

  LOGV("WvCdmTestBase::Provision: http_message: \n%s\n", http_message.c_str());

  if (binary_provisioning_) {
    // extract provisioning response from received message
    // Extracts signed response from JSON string, result is serialized protobuf.
    const std::string kMessageStart = "\"signedResponse\": \"";
    const std::string kMessageEnd = "\"";
    std::string protobuf_response;
    EXPECT_TRUE(ExtractSignedMessage(http_message, kMessageStart, kMessageEnd,
                                     &protobuf_response))
        << "Failed to extract signed serialized response from JSON response";

    LOGV("WvCdmEnginePreProvTest::Provision: extracted response message: \n"
         "%s\n", protobuf_response.c_str());

    // base64 decode response to yield binary protobuf
    std::vector<uint8_t> response_vec(Base64SafeDecode(
        std::string(protobuf_response.begin(), protobuf_response.end())));
    std::string binary_protobuf_response(response_vec.begin(),
                                         response_vec.end());
    ASSERT_EQ(NO_ERROR, cdm_engine.HandleProvisioningResponse(
                            binary_protobuf_response, &cert, &wrapped_key))
        << "message = " << http_message;
  } else {
    ASSERT_EQ(NO_ERROR, cdm_engine.HandleProvisioningResponse(
                            http_message, &cert, &wrapped_key))
        << "message = " << http_message;
  }
}

// TODO(fredgc): Replace this with a pre-defined DRM certificate.  We could do
// that because either the device is using a known test keybox with a known
// device key, or the device is using an OEM certificate, and we can extract
// that certificate from the provisioning request.
void WvCdmTestBase::EnsureProvisioned() {
  CdmSessionId session_id;
  FileSystem file_system;
  CdmEngine cdm_engine(&file_system,
                       std::shared_ptr<EngineMetrics>(new EngineMetrics));
  CdmResponseType status =
      cdm_engine.OpenSession(config_.key_system(), NULL, NULL, &session_id);
  if (status == NEED_PROVISIONING) {
    Provision();
    status =
        cdm_engine.OpenSession(config_.key_system(), NULL, NULL, &session_id);
  }
  ASSERT_EQ(NO_ERROR, status);
  ASSERT_NE("", session_id) << "Could not open CDM session.";
  ASSERT_TRUE(cdm_engine.IsOpenSession(session_id));
  ASSERT_EQ(NO_ERROR, cdm_engine.CloseSession(session_id));
}

bool WvCdmTestBase::Initialize(int argc, char **argv) {
  Properties::Init();
  bool is_cast_receiver = false;
  bool force_load_test_keybox = false;  // TODO(fredgc): obsolete. remove.
  bool filter_tests = true;
  bool show_usage = false;
  int verbosity = 0;

  // Skip the first element, which is the program name.
  const std::vector<std::string> args(argv + 1, argv + argc);
  for (const std::string& arg : args) {
    if (arg == "--verbose") {
      ++verbosity;
    } else if (arg == "--no_filter") {
      filter_tests = false;
    } else if (arg == "--cast") {
      is_cast_receiver = true;
    } else {
      const auto index = arg.find('=');
      if (index == std::string::npos) {
        std::cerr << "Argument values need to be specified using --arg=foo" << std::endl;
        show_usage = true;
        break;
      }

      const std::string arg_prefix = arg.substr(0, index);
      const std::string arg_value = arg.substr(index + 1);
      if (arg_prefix == "--license_server_id") {
        if (arg_value == "gp") {
          default_config_ = ConfigTestEnv(kGooglePlayServer);
        } else if (arg_value == "cp") {
          default_config_ = ConfigTestEnv(kContentProtectionUatServer);
        } else if (arg_value == "st") {
          default_config_ = ConfigTestEnv(kContentProtectionStagingServer);
        } else {
          std::cerr << "Invalid license server id: " << arg_value << std::endl;
          show_usage = true;
          break;
        }
      } else if (arg_prefix == "--keyid") {
        default_config_.set_key_id(arg_value);
      } else if (arg_prefix == "--service_certificate") {
        const std::string certificate(a2bs_hex(arg_value));
        default_config_.set_license_service_certificate(certificate);
      } else if (arg_prefix == "--provisioning_certificate") {
        const std::string certificate(a2bs_hex(arg_value));
        default_config_.set_provisioning_service_certificate(certificate);
      } else if (arg_prefix == "--license_server_url") {
        default_config_.set_license_server(arg_value);
      } else if (arg_prefix == "--provisioning_server_url") {
        default_config_.set_provisioning_server(arg_value);
      } else {
        std::cerr << "Unknown argument " << arg_prefix << std::endl;
        show_usage = true;
        break;
      }
    }
  }

  if (show_usage) {
    show_menu(argv[0]);
    return false;
  }

  g_cutoff = static_cast<LogPriority>(verbosity);

  // Displays server url, port and key Id being used
  std::cout << std::endl;
  std::cout << "Default Server: " << default_config_.license_server()
            << std::endl;
  std::cout << "Default KeyID: " << default_config_.key_id() << std::endl
            << std::endl;

  // Figure out which tests are appropriate for OEMCrypto, based on features
  // supported.
  wvoec::global_features.Initialize(is_cast_receiver, force_load_test_keybox);
  // If the user requests --no_filter, we don't change the filter, otherwise, we
  // filter out features that are not supported.
  if (filter_tests) {
    ::testing::GTEST_FLAG(filter) =
        wvoec::global_features.RestrictFilter(::testing::GTEST_FLAG(filter));
  }
  return true;
}

TestLicenseHolder::TestLicenseHolder(CdmEngine* cdm_engine)
    : cdm_engine_(cdm_engine),
      session_opened_(false),
      // Keys are initialized with simple values, and the correct size:
      derived_mac_key_server_(MAC_KEY_SIZE, 'a'),
      derived_mac_key_client_(MAC_KEY_SIZE, 'b'),
      mac_key_server_(MAC_KEY_SIZE, 'c'),
      mac_key_client_(MAC_KEY_SIZE, 'd'),
      enc_key_(CONTENT_KEY_SIZE, 'e'),
      session_key_(CONTENT_KEY_SIZE, 'f') {}

TestLicenseHolder::~TestLicenseHolder() {
  CloseSession();
}

void TestLicenseHolder::OpenSession(const std::string& key_system) {
  CdmResponseType status =
      cdm_engine_->OpenSession(key_system, NULL, NULL, &session_id_);
  ASSERT_EQ(status, NO_ERROR);
  ASSERT_NE("", session_id_) << "Could not open CDM session.";
  ASSERT_TRUE(cdm_engine_->IsOpenSession(session_id_));
  session_opened_ = true;
}

void TestLicenseHolder::CloseSession() {
  if (session_opened_) {
    cdm_engine_->CloseSession(session_id_);
    session_opened_ = false;
  }
}

void TestLicenseHolder::GenerateKeyRequest(
    const std::string& key_id, const std::string& init_data_type_string) {
  ASSERT_TRUE(session_opened_);
  CdmAppParameterMap app_parameters;
  CdmKeySetId key_set_id;
  InitializationData init_data(init_data_type_string, key_id);
  CdmKeyRequest key_request;
  CdmResponseType result = cdm_engine_->GenerateKeyRequest(
      session_id_, key_set_id, init_data, kLicenseTypeStreaming, app_parameters,
      &key_request);
  EXPECT_EQ(KEY_MESSAGE, result);
  signed_license_request_data_ = key_request.message;
  EXPECT_EQ(kKeyRequestTypeInitial, key_request.type);
}

void TestLicenseHolder::CreateDefaultLicense() {
  video_widevine::SignedMessage signed_message;
  EXPECT_TRUE(signed_message.ParseFromString(signed_license_request_data_));
  license_request_data_ = signed_message.msg();
  video_widevine::LicenseRequest license_request;
  EXPECT_TRUE(license_request.ParseFromString(license_request_data_));
  video_widevine::ClientIdentification client_id = license_request.client_id();

  EXPECT_EQ(
      video_widevine::ClientIdentification_TokenType_DRM_DEVICE_CERTIFICATE,
      client_id.type());

  // Extract the RSA key from the DRM certificate.
  std::string token = client_id.token();
  video_widevine::SignedDrmDeviceCertificate signed_drm_cert;
  EXPECT_TRUE(signed_drm_cert.ParseFromString(token));
  video_widevine::DrmDeviceCertificate drm_cert;
  EXPECT_TRUE(drm_cert.ParseFromString(signed_drm_cert.drm_certificate()));
  EXPECT_TRUE(rsa_key_.Init(drm_cert.public_key()));
  EXPECT_TRUE(rsa_key_.VerifySignature(signed_message.msg(),
                                       signed_message.signature()));

  DeriveKeysFromSessionKey();

  video_widevine::LicenseIdentification* license_id = license()->mutable_id();
  license_id->set_request_id("TestCase");
  license_id->set_session_id(session_id_);
  license_id->set_type(video_widevine::STREAMING);
  license_id->set_version(0);

  ::video_widevine::License_Policy* policy = license()->mutable_policy();
  policy->set_can_play(true);
  policy->set_can_persist(false);
  policy->set_can_renew(false);
  policy->set_playback_duration_seconds(0);
  policy->set_license_duration_seconds(0);

  AddMacKey();
}

void TestLicenseHolder::AddMacKey() {
  video_widevine::License_KeyContainer* key_container = license()->add_key();
  std::vector<uint8_t> iv(KEY_IV_SIZE, 'v');
  std::string iv_s(iv.begin(), iv.end());
  key_container->set_iv(iv_s);
  key_container->set_type(video_widevine::License_KeyContainer_KeyType_SIGNING);

  // Combine server and client mac keys.
  std::vector<uint8_t> keys(mac_key_server_);
  keys.insert(keys.end(), mac_key_client_.begin(), mac_key_client_.end());
  std::string encrypted_keys =
      WvCdmTestBase::Aes128CbcEncrypt(enc_key_, keys, iv);
  key_container->set_key(encrypted_keys);
}

video_widevine::License_KeyContainer* TestLicenseHolder::AddKey(
    const KeyId& key_id, const std::vector<uint8_t>& key_data,
    const wvoec::KeyControlBlock& block_in) {
  video_widevine::License_KeyContainer* key_container = license()->add_key();
  wvoec::KeyControlBlock block = block_in;
  if (block.verification[0] == 0) {
    block.verification[0] = 'k';
    block.verification[1] = 'c';
    block.verification[2] = '1';
    // This will work until oemcrypto api 20.
    block.verification[3] = '0' + wvoec::global_features.api_version - 10;
  }
  key_container->set_id(key_id);
  key_container->set_type(video_widevine::License_KeyContainer_KeyType_CONTENT);
  key_container->set_level(
      video_widevine::License_KeyContainer_SecurityLevel_SW_SECURE_CRYPTO);

  std::vector<uint8_t> iv(KEY_IV_SIZE, 'v');
  std::string iv_s(iv.begin(), iv.end());
  key_container->set_iv(iv_s);

  std::string encrypted_key_data =
      WvCdmTestBase::Aes128CbcEncrypt(enc_key_, key_data, iv);
  // TODO(b/111069024): remove this!
  std::string padding(CONTENT_KEY_SIZE, '-');
  key_container->set_key(encrypted_key_data + padding);

  std::vector<uint8_t> block_v(
      reinterpret_cast<const uint8_t*>(&block),
      reinterpret_cast<const uint8_t*>(&block) + sizeof(block));
  std::vector<uint8_t> block_iv(KEY_IV_SIZE, 'w');
  std::string block_iv_s(block_iv.begin(), block_iv.end());
  std::string encrypted_block =
      WvCdmTestBase::Aes128CbcEncrypt(key_data, block_v, block_iv);
  key_container->mutable_key_control()->set_iv(block_iv_s);
  key_container->mutable_key_control()->set_key_control_block(encrypted_block);
  return key_container;
}

void TestLicenseHolder::SignAndLoadLicense() {
#if 0  // Need to turn off protobuf_lite to use this.
  LOGV("License = %s\n", license_.DebugString().c_str());
#endif
  std::string license_data;
  license_.SerializeToString(&license_data);

  std::string signature =
      WvCdmTestBase::SignHMAC(license_data, derived_mac_key_server_);

  std::string session_key_s(session_key_.begin(), session_key_.end());
  std::string encrypted_session_key;
  EXPECT_TRUE(rsa_key_.Encrypt(session_key_s, &encrypted_session_key));
  video_widevine::SignedMessage signed_response;
  signed_response.set_msg(license_data);
  signed_response.set_type(video_widevine::SignedMessage_MessageType_LICENSE);
  signed_response.set_session_key(encrypted_session_key);
  signed_response.set_signature(signature);

  std::string response_data;
  signed_response.SerializeToString(&response_data);

  CdmKeySetId key_set_id;
  CdmLicenseType license_type;  // Required for AddKey. Result value ignored.
  EXPECT_EQ(KEY_ADDED,
            cdm_engine_->AddKey(session_id_, response_data,
                                &license_type, &key_set_id));
}

void TestLicenseHolder::DeriveKeysFromSessionKey() {
  std::string context;
  GenerateMacContext(license_request_data_, &context);
  std::vector<uint8_t> mac_key_context(context.begin(), context.end());
  GenerateEncryptContext(license_request_data_, &context);
  std::vector<uint8_t> enc_key_context(context.begin(), context.end());

  ASSERT_TRUE(
      DeriveKey(session_key_, mac_key_context, 1, &derived_mac_key_server_));
  std::vector<uint8_t> mac_key_part2;
  ASSERT_TRUE(DeriveKey(session_key_, mac_key_context, 2, &mac_key_part2));
  derived_mac_key_server_.insert(derived_mac_key_server_.end(),
                                 mac_key_part2.begin(), mac_key_part2.end());

  ASSERT_TRUE(
      DeriveKey(session_key_, mac_key_context, 3, &derived_mac_key_client_));
  ASSERT_TRUE(DeriveKey(session_key_, mac_key_context, 4, &mac_key_part2));
  derived_mac_key_client_.insert(derived_mac_key_client_.end(),
                                 mac_key_part2.begin(), mac_key_part2.end());

  std::vector<uint8_t> enc_key;
  ASSERT_TRUE(DeriveKey(session_key_, enc_key_context, 1, &enc_key_));
}

bool TestLicenseHolder::DeriveKey(const std::vector<uint8_t>& key,
                                  const std::vector<uint8_t>& context,
                                  int counter, std::vector<uint8_t>* out) {
  if (key.empty() || counter > 4 || context.empty() || out == NULL) {
    LOGE("DeriveKey(): bad context");
    return false;
  }
  const EVP_CIPHER* cipher = EVP_aes_128_cbc();
  CMAC_CTX* cmac_ctx = CMAC_CTX_new();

  if (!cmac_ctx) {
    LOGE("DeriveKey(): cmac failure");
    return false;
  }

  if (!CMAC_Init(cmac_ctx, &key[0], key.size(), cipher, 0)) {
    LOGE("DeriveKey(): CMAC_Init");
    CMAC_CTX_free(cmac_ctx);
    return false;
  }

  std::vector<uint8_t> message;
  message.push_back(counter);
  message.insert(message.end(), context.begin(), context.end());

  if (!CMAC_Update(cmac_ctx, &message[0], message.size())) {
    LOGE("DeriveKey(): CMAC_Update");
    CMAC_CTX_free(cmac_ctx);
    return false;
  }

  size_t reslen;
  uint8_t res[128];
  if (!CMAC_Final(cmac_ctx, res, &reslen)) {
    LOGE("DeriveKey(): CMAC_Final");
    CMAC_CTX_free(cmac_ctx);
    return false;
  }
  out->assign(res, res + reslen);
  CMAC_CTX_free(cmac_ctx);
  return true;
}

}  // namespace wvcdm
