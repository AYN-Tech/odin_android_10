// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include <errno.h>
#include <getopt.h>
#include <sstream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <utils/Thread.h>
#include "OEMCryptoCENC.h"
#include "cdm_identifier.h"
#include "clock.h"
#include "config_test_env.h"
#include "device_files.h"
#include "file_store.h"
#include "license_protocol.pb.h"
#include "license_request.h"
#include "log.h"
#include "oemcrypto_adapter.h"
#include "properties.h"
#include "pst_report.h"
#include "string_conversions.h"
#include "test_base.h"
#include "test_printers.h"
#include "url_request.h"
#include "wv_cdm_constants.h"
#include "wv_content_decryption_module.h"

namespace {

// HTTP response codes.
const int kHttpOk = 200;
// The following two responses are unused, but left here for human debuggers.
// const int kHttpBadRequest = 400;
// const int kHttpInternalServerError = 500;

const uint32_t kMinute = 60;
const uint32_t kClockTolerance = 10;

const uint32_t kMaxUsageTableSize = 50;
const std::string kEmptyServiceCertificate;

// TODO(rfrias): refactor to print out the decryption test names
struct SubSampleInfo {
  bool retrieve_key;
  size_t num_of_subsamples;
  bool validate_key_id;
  bool is_encrypted;
  bool is_secure;
  wvcdm::KeyId key_id;
  std::vector<uint8_t> encrypt_data;
  std::vector<uint8_t> decrypt_data;
  std::vector<uint8_t> iv;
  size_t block_offset;
  uint8_t subsample_flags;
};

// clang-format off
SubSampleInfo kEncryptedStreamingNoPstSubSample = {
    // key SD, encrypted, 256b
    true, 1, true, true, false,
    wvcdm::a2bs_hex("371EA35E1A985D75D198A7F41020DC23"),
    wvcdm::a2b_hex(
        "64ab17b3e3dfab47245c7cce4543d4fc7a26dcf248f19f9b59f3c92601440b36"
        "17c8ed0c96c656549e461f38708cd47a434066f8df28ccc28b79252eee3f9c2d"
        "7f6c68ebe40141fe818fe082ca523c03d69ddaf183a93c022327fedc5582c5ab"
        "ca9d342b71263a67f9cb2336f12108aaaef464f17177e44e9b0c4e56e61da53c"
        "2150b4405cc82d994dfd9bf4087c761956d6688a9705db4cf350381085f383c4"
        "9666d4aed135c519c1f0b5cba06e287feea96ea367bf54e7368dcf998276c6e4"
        "6497e0c50e20fef74e42cb518fe7f22ef27202428688f86404e8278587017012"
        "c1d65537c6cbd7dde04aae338d68115a9f430afc100ab83cdadf45dca39db685"),
    wvcdm::a2b_hex(
        "217ce9bde99bd91e9733a1a00b9b557ac3a433dc92633546156817fae26b6e1c"
        "942ac20a89ff79f4c2f25fba99d6a44618a8c0420b27d54e3da17b77c9d43cca"
        "595d259a1e4a8b6d7744cd98c5d3f921adc252eb7d8af6b916044b676a574747"
        "8df21fdc42f166880d97a2225cd5c9ea5e7b752f4cf81bbdbe98e542ee10e1c6"
        "ad868a6ac55c10d564fc23b8acff407daaf4ed2743520e02cda9680d9ea88e91"
        "029359c4cf5906b6ab5bf60fbb3f1a1c7c59acfc7e4fb4ad8e623c04d503a3dd"
        "4884604c8da8a53ce33db9ff8f1c5bb6bb97f37b39906bf41596555c1bcce9ed"
        "08a899cd760ff0899a1170c2f224b9c52997a0785b7fe170805fd3e8b1127659"),
    wvcdm::a2b_hex("f6f4b1e600a5b67813ed2bded913ba9f"), 0, 3};

SubSampleInfo kEncryptedStreamingClip8SubSample = {
    true, 1, true, true, false,
    wvcdm::a2bs_hex("E82DDD1D07CBB31CDD31EBAAE0894609"),
    wvcdm::a2b_hex(
        "fe8670a01c86906c056b4bf85ad278464c4eb79c60de1da8480e66e78561350e"
        "a25ae19a001f834c43aaeadf900b3c5a6745e885a4d1d1ae5bafac08dc1d60e5"
        "f3465da303909ec4b09023490471f670b615d77db844192854fdab52b7806203"
        "89b374594bbb6a2f2fcc31036d7cb8a3f80c0e27637b58a7650028fbf2470d68"
        "1bbd77934af165d215ef325c74438c9d99a20fc628792db28c05ed5deff7d9d4"
        "dba02ddb6cf11dc6e78cb5200940af9a2321c3a7c4c79be67b54a744dae1209c"
        "fa02fc250ce18d30c7da9c3a4a6c9619bf8561a42ff1e55a7b14fa3c8de69196"
        "c2b8e3ff672fc37003b479da5d567b7199917dbe5aa402890ebb066bce140b33"),
    wvcdm::a2b_hex(
        "d08733bd0ef671f467906b50ff8322091400f86fd6f016fea2b86e33923775b3"
        "ebb4c8c6f3ba8b78dd200a74d3872a40264ab99e1d422e4f819abb7f249114aa"
        "b334420b37c86ce81938615ab9d3a6b2de8db545cd88e35091031e73016fb386"
        "1b754298329b52dbe483de3a532277815e659f3e05e89257333225b933d92e15"
        "ef2deff287a192d2c8fc942a29a5f3a1d54440ac6385de7b34bb650b889e4ae9"
        "58c957b5f5ff268f445c0a6b825fcad55290cb7b5c9814bc4c72984dcf4c8fd7"
        "5f511c173b2e0a3163b18a1eac58539e5c188aeb0751b946ad4dcd08ea777a7f"
        "37326df26fa509343faa98dff667629f557873f1284903202e451227ef465a62"),
    wvcdm::a2b_hex("7362b5140c4ce0cd5f863858668d3f1a"), 0, 3};

SubSampleInfo kEncryptedStreamingClip5SubSample = {
    true, 1, true, true, false,
    wvcdm::a2bs_hex("3AE243D83B93B3311A1D777FF5FBE01A"),
    wvcdm::a2b_hex(
        "934997779aa1aeb45d6ba8845f13786575d0adf85a5e93674d9597f8d4286ed7"
        "dcce02f306e502bbd9f1cadf502f354038ca921276d158d911bdf3171d335b18"
        "0ae0f9abece16ff31ee263228354f724da2f3723b19caa38ea02bd6563b01208"
        "fb5bf57854ac0fe38d5883197ef90324b2721ff20fdcf9a53819515e6daa096e"
        "70f6f5c1d29a4a13dafd127e2e1f761ea0e28fd451607552ecbaef5da3c780bc"
        "aaf2667b4cc4f858f01d480cac9e32c3fbb5705e5d2adcceebefc2535c117208"
        "e65f604799fc3d7223e16908550f287a4bea687008cb0064cf14d3aeedb8c705"
        "09ebc5c2b8b5315f43c04d78d2f55f4b32c7d33e157114362106395cc0bb6d93"),
    wvcdm::a2b_hex(
        "2dd54eee1307753508e1f250d637044d6e8f5abf057dab73e9e95f83910e4efc"
        "191c9bac63950f13fd51833dd94a4d03f2b64fb5c721970c418fe53fa6f74ad5"
        "a6e16477a35c7aa6e28909b069cd25770ef80da20918fc30fe95fd5c87fd3522"
        "1649de17ca2c7b3dc31f936f0cbdf97c7b1c15de3a86b279dc4b4de64943914a"
        "99734556c4b7a1a0b022c1933cb0786068fc18d49fed2f2b49f3ac6d01c32d07"
        "92175ce2844eaf9064e6a3fcffade038d690cbed81659351163a22432f0d0545"
        "037e1c805d8e92a1272b4196ad0ce22f26bb80063137a8e454d3b97e2414283d"
        "ed0716cd8bceb80cf59166a217006bd147c51b04dfb183088ce3f51e9b9f759e"),
    wvcdm::a2b_hex("b358ab21ac90455bbf60490daad457e3"), 0, 3};

SubSampleInfo kEncryptedOfflineClip2SubSample = {
    true, 1, true, true, false,
    wvcdm::a2bs_hex("3260F39E12CCF653529990168A3583FF"),
    wvcdm::a2b_hex(
        "3b2cbde084973539329bd5656da22d20396249bf4a18a51c38c4743360cc9fea"
        "a1c78d53de1bd7e14dc5d256fd20a57178a98b83804258c239acd7aa38f2d7d2"
        "eca614965b3d22049e19e236fc1800e60965d8b36415677bf2f843d50a6943c4"
        "683c07c114a32f5e5fbc9939c483c3a1b2ecd3d82b554d649798866191724283"
        "f0ab082eba2da79aaca5c4eaf186f9ee9a0c568f621f705a578f30e4e2ef7b96"
        "5e14cc046ce6dbf272ee5558b098f332333e95fc879dea6c29bf34acdb649650"
        "f08201b9e649960f2493fd7677cc3abf5ae70e5445845c947ba544456b431646"
        "d95a133bff5f57614dda5e4446cd8837901d074149dadf4b775b5b07bb88ca20"),
    wvcdm::a2b_hex(
        "D3EE543581F16AB2EABFA13468133314D19CB6A14A42229BE83543828D801475"
        "FAE1C2C5D193DA8445B9C4B1598E8FCBDF42EFF1FBB54EBC6A4815E2836C2848"
        "15094DEDE76FE4658A2D6EA3E775A872CA71835CF274676C18556C665EC7F32A"
        "4DBB04C10BA988B42758E37DCEFD99D9CE3AFFB1E816C412B4013890E1A31E25"
        "64EBF2125BC54D66FECDF8A31956830121546DC183B3DAC103E223778875B590"
        "3961448C287B367C585E510DA43BF9242B8E9A27B9F6F3EC7E4B5A0A74A1742B"
        "F5CD65EA66D7D9E79C02C7E7D5CD02DB182DDD8EAC3525B0834F1F2822AD0006"
        "944B5080B998BB0FE6E566AAFAE2328B37FD189F1920A964434ECF18E11AC81E"),
    wvcdm::a2b_hex("7362b5140c4ce0cd5f863858668d3f1a"), 0, 3};

std::string kStreamingClip8PstInitData = wvcdm::a2bs_hex(
    "000000427073736800000000"                  // blob size and pssh
    "EDEF8BA979D64ACEA3C827DCD51D21ED00000022"  // Widevine system id
    "08011a0d7769646576696e655f74657374220f73"  // pssh data
    "747265616d696e675f636c697038");

std::string kOfflineClip2PstInitData = wvcdm::a2bs_hex(
    "000000427073736800000000"                  // blob size and pssh
    "EDEF8BA979D64ACEA3C827DCD51D21ED00000020"  // Widevine system id
    "08011a0d7769646576696e655f74657374220d6f"  // pssh data
    "66666c696e655f636c697032");

std::string kOfflineClip4 = wvcdm::a2bs_hex(
    "000000427073736800000000"                  // blob size and pssh
    "EDEF8BA979D64ACEA3C827DCD51D21ED00000020"  // Widevine system id
    "08011a0d7769646576696e655f74657374220d6f"  // pssh data
    "66666c696e655f636c697034");
// clang-format off

std::string kUatLicenseServer = "https://proxy.uat.widevine.com/proxy";

bool StringToInt64(const std::string& input, int64_t* output) {
  std::istringstream ss(input);
  ss >> *output;
  return !ss.fail();
}

}  // namespace

// GTest requires PrintTo to be in the same namespace as the thing it prints,
// which is std::vector in this case.
namespace std {
void PrintTo(const vector<uint8_t>& value, ostream* os) {
  *os << wvcdm::b2a_hex(value);
}
}  // namespace std

using ::testing::Contains;
using ::testing::Pair;
using ::testing::StrNe;

namespace wvcdm {
// Protobuf generated classes
using video_widevine::ClientIdentification;
using video_widevine::LicenseIdentification;
using video_widevine::LicenseRequest_ContentIdentification;
using video_widevine::LicenseRequest_ContentIdentification_CencDeprecated;
using video_widevine::SignedMessage;

class TestWvCdmClientPropertySet : public CdmClientPropertySet {
 public:
  TestWvCdmClientPropertySet()
      : use_privacy_mode_(false),
        is_session_sharing_enabled_(false),
        session_sharing_id_(0) {}
  virtual ~TestWvCdmClientPropertySet() {}

  virtual const std::string& app_id() const { return app_id_; }
  virtual const std::string& security_level() const { return security_level_; }
  virtual const std::string& service_certificate() const {
    return service_certificate_;
  }
  virtual void set_service_certificate(const std::string& cert) {
    service_certificate_ = cert;
  }
  virtual bool use_privacy_mode() const { return use_privacy_mode_; }
  virtual bool is_session_sharing_enabled() const {
    return is_session_sharing_enabled_;
  }
  virtual uint32_t session_sharing_id() const { return session_sharing_id_; }

  void set_app_id(const std::string& app_id) { app_id_ = app_id; }
  void set_security_level(const std::string& security_level) {
    if (security_level == QUERY_VALUE_SECURITY_LEVEL_L1 ||
        security_level == QUERY_VALUE_SECURITY_LEVEL_L3) {
      security_level_ = security_level;
    }
  }
  void set_use_privacy_mode(bool use_privacy_mode) {
    use_privacy_mode_ = use_privacy_mode;
  }
  void set_session_sharing_mode(bool enable) {
    is_session_sharing_enabled_ = enable;
  }
  void set_session_sharing_id(uint32_t id) { session_sharing_id_ = id; }

 private:
  std::string app_id_;
  std::string security_level_;
  std::string service_certificate_;
  bool use_privacy_mode_;
  bool is_session_sharing_enabled_;
  uint32_t session_sharing_id_;
};

class WvCdmExtendedDurationTest : public WvCdmTestBase {
 public:
  WvCdmExtendedDurationTest() {}
  ~WvCdmExtendedDurationTest() {}

 protected:
  void GetOfflineConfiguration(std::string* key_id, std::string* client_auth) {
    ConfigTestEnv config(config_.server_id(), false);
    if (binary_key_id().compare(a2bs_hex(config_.key_id())) == 0)
      key_id->assign(a2bs_hex(config.key_id()));
    else
      key_id->assign(binary_key_id());

    client_auth->assign(config.client_auth());
  }

  void GenerateKeyRequest(const std::string& init_data,
                          CdmLicenseType license_type) {
    CdmResponseType response;
    GenerateKeyRequest(init_data, license_type, &response);
    EXPECT_EQ(KEY_MESSAGE, response);
  }

  void GenerateKeyRequest(const std::string& init_data,
                          CdmLicenseType license_type,
                          CdmResponseType *response) {
    CdmAppParameterMap app_parameters;
    CdmKeyRequest key_request;

    *response = decryptor_.GenerateKeyRequest(
                    session_id_, key_set_id_, "video/mp4", init_data,
                    license_type, app_parameters, NULL,
                    kDefaultCdmIdentifier, &key_request);
    if (*response == KEY_MESSAGE) {
      EXPECT_EQ(kKeyRequestTypeInitial, key_request.type);
      key_msg_ = key_request.message;
      EXPECT_EQ(0u, key_request.url.size());
    }
  }

  void GenerateRenewalRequest(CdmLicenseType license_type,
                              std::string* server_url) {
    // TODO application makes a license request, CDM will renew the license
    // when appropriate.
    std::string init_data;
    CdmAppParameterMap app_parameters;

    CdmKeyRequest key_request;

    EXPECT_EQ(KEY_MESSAGE, decryptor_.GenerateKeyRequest(
                               session_id_, key_set_id_, "video/mp4", init_data,
                               license_type, app_parameters, NULL,
                               kDefaultCdmIdentifier, &key_request));

    *server_url = key_request.url;
    key_msg_ = key_request.message;
    EXPECT_EQ(kKeyRequestTypeRenewal, key_request.type);
    // TODO(edwinwong, rfrias): Add tests cases for when license server url
    // is empty on renewal. Need appropriate key id at the server.
    EXPECT_NE(0u, key_request.url.size());
  }

  void GenerateKeyRelease(CdmKeySetId key_set_id,
                          CdmResponseType expected_response) {
    CdmSessionId session_id;
    CdmInitData init_data;
    CdmAppParameterMap app_parameters;
    CdmKeyRequest key_request;

    EXPECT_EQ(expected_response, decryptor_.GenerateKeyRequest(
                               session_id, key_set_id, "video/mp4", init_data,
                               kLicenseTypeRelease, app_parameters, NULL,
                               kDefaultCdmIdentifier, &key_request));

    if (expected_response == KEY_MESSAGE) {
      key_msg_ = key_request.message;
      EXPECT_EQ(kKeyRequestTypeRelease, key_request.type);
    }
  }

  void GenerateKeyRelease(CdmKeySetId key_set_id) {
    GenerateKeyRelease(key_set_id, KEY_MESSAGE);
  }

  void LogResponseError(const std::string& message, int http_status_code) {
    LOGD("HTTP Status code = %d", http_status_code);
    LOGD("HTTP response(%d): %s", message.size(), b2a_hex(message).c_str());
  }

  // Post a request and extract the drm message from the response
  std::string GetKeyRequestResponse(const std::string& server_url,
                                    const std::string& client_auth) {
    // Use secure connection and chunk transfer coding.
    UrlRequest url_request(server_url + client_auth);
    EXPECT_TRUE(url_request.is_connected()) << "Fail to connect to "
                                            << server_url << client_auth;
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
    key_response_ = drm_msg;
    return drm_msg;
  }

  // Post a request and extract the signed provisioning message from
  // the HTTP response.
  std::string GetCertRequestResponse(const std::string& server_url) {
    // Use secure connection and chunk transfer coding.
    UrlRequest url_request(server_url);
    EXPECT_TRUE(url_request.is_connected()) << "Fail to connect to "
                                            << server_url;
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

  // Post a request and extract the signed provisioning message from
  // the HTTP response.
  std::string GetUsageInfoResponse(const std::string& server_url,
                                   const std::string& client_auth,
                                   const std::string& usage_info_request) {
    // Use secure connection and chunk transfer coding.
    UrlRequest url_request(server_url + client_auth);
    EXPECT_TRUE(url_request.is_connected()) << "Fail to connect to "
                                            << server_url << client_auth;
    url_request.PostRequest(usage_info_request);
    std::string message;
    EXPECT_TRUE(url_request.GetResponse(&message));

    int http_status_code = url_request.GetStatusCode(message);
    if (kHttpOk != http_status_code) {
      LogResponseError(message, http_status_code);
    }
    EXPECT_EQ(kHttpOk, http_status_code);

    std::string usage_info;
    if (kHttpOk == http_status_code) {
      LicenseRequest license;
      license.GetDrmMessage(message, usage_info);
      LOGV("HTTP response body: (%u bytes)", usage_info.size());
    }
    return usage_info;
  }

  void VerifyKeyRequestResponse(const std::string& server_url,
                                const std::string& client_auth,
                                bool is_renewal) {
    VerifyKeyRequestResponse(server_url, client_auth, is_renewal, NULL);
  }

  void VerifyKeyRequestResponse(const std::string& server_url,
                                const std::string& client_auth,
                                bool /* is_renewal */,
                                CdmResponseType* status) {
    std::string resp = GetKeyRequestResponse(server_url, client_auth);
    CdmResponseType sts =
        decryptor_.AddKey(session_id_, resp, &key_set_id_);

    if (status == NULL) {
      EXPECT_EQ(KEY_ADDED, sts);
    } else {
      *status = sts;
    }
  }

  void Unprovision() {
    EXPECT_EQ(NO_ERROR,
              decryptor_.Unprovision(kSecurityLevelL1, kDefaultCdmIdentifier));
    EXPECT_EQ(NO_ERROR,
              decryptor_.Unprovision(kSecurityLevelL3, kDefaultCdmIdentifier));
  }

  void Provision() {
    CdmResponseType status = decryptor_.OpenSession(
        config_.key_system(), NULL, kDefaultCdmIdentifier, NULL, &session_id_);
    switch (status) {
      case NO_ERROR:
        decryptor_.CloseSession(session_id_);
        return;
      case NEED_PROVISIONING:
        break;
      default:
        EXPECT_EQ(NO_ERROR, status);
        return;
    }

    std::string provisioning_server_url;
    CdmCertificateType cert_type = kCertificateWidevine;
    std::string cert_authority, cert, wrapped_key;

    status = decryptor_.GetProvisioningRequest(
        cert_type, cert_authority, kDefaultCdmIdentifier,
        kEmptyServiceCertificate, &key_msg_, &provisioning_server_url);
    EXPECT_EQ(NO_ERROR, status);
    if (NO_ERROR != status) return;
    EXPECT_EQ(provisioning_server_url, config_.provisioning_server());

    std::string response =
        GetCertRequestResponse(config_.provisioning_server());
    EXPECT_NE(0, static_cast<int>(response.size()));
    EXPECT_EQ(NO_ERROR,
        decryptor_.HandleProvisioningResponse(kDefaultCdmIdentifier, response,
                                              &cert, &wrapped_key));
    EXPECT_EQ(0, static_cast<int>(cert.size()));
    EXPECT_EQ(0, static_cast<int>(wrapped_key.size()));
    decryptor_.CloseSession(session_id_);
    return;
  }

  void ValidateResponse(video_widevine::LicenseType license_type,
                        bool has_provider_session_token) {
    // Validate signed response
    SignedMessage signed_message;
    EXPECT_TRUE(signed_message.ParseFromString(key_response_));
    EXPECT_EQ(SignedMessage::LICENSE, signed_message.type());
    EXPECT_TRUE(signed_message.has_signature());
    EXPECT_TRUE(!signed_message.msg().empty());

    // Verify license
    video_widevine::License license;
    EXPECT_TRUE(license.ParseFromString(signed_message.msg()));

    // Verify license identification
    license_id_ = license.id();
    EXPECT_EQ(license_type, license_id_.type());
    EXPECT_EQ(has_provider_session_token,
              license_id_.has_provider_session_token());
  }

  void ValidateRenewalRequest(int64_t expected_seconds_since_started,
                              int64_t expected_seconds_since_last_played,
                              bool has_provider_session_token) {
    // Validate signed renewal request
    SignedMessage signed_message;
    EXPECT_TRUE(signed_message.ParseFromString(key_msg_));
    EXPECT_EQ(SignedMessage::LICENSE_REQUEST, signed_message.type());
    EXPECT_TRUE(signed_message.has_signature());
    EXPECT_TRUE(!signed_message.msg().empty());

    // Verify license request
    video_widevine::LicenseRequest license_renewal;
    EXPECT_TRUE(license_renewal.ParseFromString(signed_message.msg()));

    // Verify Content Identification
    const LicenseRequest_ContentIdentification& content_id =
        license_renewal.content_id();
    EXPECT_FALSE(content_id.has_cenc_id_deprecated());
    EXPECT_FALSE(content_id.has_webm_id_deprecated());
    EXPECT_TRUE(content_id.has_existing_license());

    const LicenseRequest_ContentIdentification::ExistingLicense&
        existing_license = content_id.existing_license();

    const LicenseIdentification& id = existing_license.license_id();
    EXPECT_EQ(license_id_.request_id(), id.request_id());
    EXPECT_EQ(license_id_.session_id(), id.session_id());
    EXPECT_EQ(license_id_.purchase_id(), id.purchase_id());
    EXPECT_EQ(license_id_.type(), id.type());
    EXPECT_EQ(license_id_.version(), id.version());
    EXPECT_EQ(has_provider_session_token, !id.provider_session_token().empty());

    EXPECT_NEAR(existing_license.seconds_since_started(),
                expected_seconds_since_started, kClockTolerance);
    EXPECT_NEAR(existing_license.seconds_since_last_played(),
                expected_seconds_since_last_played, kClockTolerance);
    EXPECT_TRUE(existing_license.session_usage_table_entry().empty());

    EXPECT_EQ(::video_widevine::LicenseRequest_RequestType_RENEWAL,
              license_renewal.type());
    EXPECT_LT(0, license_renewal.request_time());
    EXPECT_EQ(video_widevine::VERSION_2_1, license_renewal.protocol_version());
    EXPECT_TRUE(license_renewal.has_key_control_nonce());
  }

  void ValidateReleaseRequest(std::string& usage_msg, bool license_used,
                              int64_t expected_seconds_since_license_received,
                              int64_t expected_seconds_since_first_playback,
                              int64_t expected_seconds_since_last_playback) {
    // Validate signed renewal request
    SignedMessage signed_message;
    EXPECT_TRUE(signed_message.ParseFromString(usage_msg));
    EXPECT_EQ(SignedMessage::LICENSE_REQUEST, signed_message.type());
    EXPECT_TRUE(signed_message.has_signature());
    EXPECT_TRUE(!signed_message.msg().empty());

    // Verify license request
    video_widevine::LicenseRequest license_renewal;
    EXPECT_TRUE(license_renewal.ParseFromString(signed_message.msg()));

    // Verify Content Identification
    const LicenseRequest_ContentIdentification& content_id =
        license_renewal.content_id();
    EXPECT_FALSE(content_id.has_cenc_id_deprecated());
    EXPECT_FALSE(content_id.has_webm_id_deprecated());
    EXPECT_TRUE(content_id.has_existing_license());

    const LicenseRequest_ContentIdentification::ExistingLicense&
        existing_license = content_id.existing_license();

    const LicenseIdentification& id = existing_license.license_id();
    EXPECT_EQ(license_id_.request_id(), id.request_id());
    EXPECT_EQ(license_id_.session_id(), id.session_id());
    EXPECT_EQ(license_id_.purchase_id(), id.purchase_id());
    EXPECT_EQ(license_id_.type(), id.type());
    EXPECT_EQ(license_id_.version(), id.version());
    EXPECT_TRUE(!id.provider_session_token().empty());
    EXPECT_NEAR(existing_license.seconds_since_started(),
                expected_seconds_since_first_playback, kClockTolerance);
    EXPECT_NEAR(existing_license.seconds_since_last_played(),
                expected_seconds_since_last_playback, kClockTolerance);
    EXPECT_TRUE(!existing_license.session_usage_table_entry().empty());

    // Verify usage report
    uint8_t* buffer = reinterpret_cast<uint8_t*>(
        const_cast<char*>(existing_license.session_usage_table_entry().data()));
    Unpacked_PST_Report usage_report(buffer);
    EXPECT_EQ(usage_report.report_size(),
              existing_license.session_usage_table_entry().size());
    EXPECT_EQ(license_used ? kInactiveUsed : kInactiveUnused,
              usage_report.status());
    EXPECT_EQ(id.provider_session_token().size(), usage_report.pst_length());
    std::string pst(reinterpret_cast<char*>(usage_report.pst()),
                    usage_report.pst_length());
    EXPECT_EQ(id.provider_session_token(), pst);
    EXPECT_LE(kInsecureClock, usage_report.clock_security_level());

    int64_t seconds_since_license_received =
        usage_report.seconds_since_license_received();
    int64_t seconds_since_first_decrypt =
        usage_report.seconds_since_first_decrypt();
    int64_t seconds_since_last_decrypt =
        usage_report.seconds_since_last_decrypt();
    // Detect licenses that were never used
    if (seconds_since_first_decrypt < 0 ||
        seconds_since_first_decrypt > seconds_since_license_received) {
      seconds_since_first_decrypt = 0;
      seconds_since_last_decrypt = 0;
    }

    EXPECT_NEAR(seconds_since_license_received,
                expected_seconds_since_license_received, kClockTolerance);
    if (license_used) {
      EXPECT_NEAR(seconds_since_first_decrypt,
                  expected_seconds_since_first_playback, kClockTolerance);
      EXPECT_NEAR(seconds_since_last_decrypt,
                  expected_seconds_since_last_playback, kClockTolerance);
    }

    EXPECT_EQ(::video_widevine::LicenseRequest_RequestType_RELEASE,
              license_renewal.type());
    EXPECT_LT(0, license_renewal.request_time());
    EXPECT_EQ(video_widevine::VERSION_2_1, license_renewal.protocol_version());
    EXPECT_TRUE(license_renewal.has_key_control_nonce());
  }

  void ValidateHasUpdateUsageEntry(
      const drm_metrics::WvCdmMetrics& metrics) const {
    bool has_update_usage_entry_metrics = false;
    for (const auto& session : metrics.session_metrics()) {
      has_update_usage_entry_metrics |=
          session.crypto_metrics()
              .usage_table_header_update_entry_time_us().size() > 0;
      has_update_usage_entry_metrics |=
          session.crypto_metrics().oemcrypto_update_usage_entry().size() > 0;
    }

    std::string serialized_metrics;
    ASSERT_TRUE(metrics.SerializeToString(&serialized_metrics));
    EXPECT_TRUE(has_update_usage_entry_metrics)
        << "metrics: " << wvcdm::b2a_hex(serialized_metrics);
  }

  void QueryKeyStatus(bool streaming, bool expect_renewal,
                      int64_t* license_duration_remaining,
                      int64_t* playback_duration_remaining) {
    CdmQueryMap query_info;
    EXPECT_EQ(NO_ERROR, decryptor_.QueryKeyStatus(session_id_, &query_info));

    EXPECT_THAT(query_info, Contains(Pair(QUERY_KEY_LICENSE_TYPE,
                                          streaming ? QUERY_VALUE_STREAMING
                                                    : QUERY_VALUE_OFFLINE)));
    EXPECT_THAT(query_info,
                Contains(Pair(QUERY_KEY_PLAY_ALLOWED, QUERY_VALUE_TRUE)));
    EXPECT_THAT(query_info, Contains(Pair(QUERY_KEY_PERSIST_ALLOWED,
                                          streaming ? QUERY_VALUE_FALSE
                                                    : QUERY_VALUE_TRUE)));
    if (expect_renewal) {
      EXPECT_THAT(query_info,
                  Contains(Pair(QUERY_KEY_RENEW_ALLOWED, QUERY_VALUE_TRUE)));
      EXPECT_THAT(query_info,
                  Contains(Pair(QUERY_KEY_RENEWAL_SERVER_URL, StrNe(""))));
    }

    std::string key = QUERY_KEY_LICENSE_DURATION_REMAINING;
    ASSERT_THAT(query_info, Contains(Pair(key, StrNe(""))));
    EXPECT_TRUE(StringToInt64(query_info[key], license_duration_remaining));
    key = QUERY_KEY_PLAYBACK_DURATION_REMAINING;
    ASSERT_THAT(query_info, Contains(Pair(key, StrNe(""))));
    EXPECT_TRUE(StringToInt64(query_info[key], playback_duration_remaining));
  }

  uint32_t QueryStatus(SecurityLevel security_level, const std::string& key) {
    std::string str;
    EXPECT_EQ(wvcdm::NO_ERROR,
              decryptor_.QueryStatus(security_level, key, &str));

    std::istringstream ss(str);
    uint32_t value;
    ss >> value;
    EXPECT_FALSE(ss.fail());
    EXPECT_TRUE(ss.eof());
    return value;
  }

  std::string GetSecurityLevel(TestWvCdmClientPropertySet* property_set) {
    decryptor_.OpenSession(config_.key_system(), property_set,
                           kDefaultCdmIdentifier, NULL, &session_id_);
    CdmQueryMap query_info;
    EXPECT_EQ(NO_ERROR,
              decryptor_.QuerySessionStatus(session_id_, &query_info));
    CdmQueryMap::iterator itr = query_info.find(QUERY_KEY_SECURITY_LEVEL);
    EXPECT_TRUE(itr != query_info.end());
    decryptor_.CloseSession(session_id_);
    return itr->second;
  }

  CdmSecurityLevel GetDefaultSecurityLevel() {
    std::string level = GetSecurityLevel(NULL);
    CdmSecurityLevel security_level = kSecurityLevelUninitialized;
    if (level == QUERY_VALUE_SECURITY_LEVEL_L1) {
      security_level = kSecurityLevelL1;
    } else if (level == QUERY_VALUE_SECURITY_LEVEL_L3) {
      security_level = kSecurityLevelL3;
    } else {
      EXPECT_TRUE(false);
    }
    return security_level;
  }

  class CloseSessionThread : public android::Thread {
   public:
     CloseSessionThread() :
         Thread(false),
         wv_content_decryption_module_(NULL) {}
     ~CloseSessionThread() {}

     bool Start(WvContentDecryptionModule* decryptor,
                const CdmSessionId& session_id,
                uint32_t time_in_msecs) {
       wv_content_decryption_module_ = decryptor;
       sess_id_ = session_id;
       delay_.tv_sec = time_in_msecs / 1000;
       delay_.tv_nsec = (time_in_msecs % 1000) * 10000000ll;
       return run("CloseSessionThread") == android::NO_ERROR;
     }

    void Stop() { requestExitAndWait(); }

   private:
    virtual bool threadLoop() {
      struct timespec delay_remaining;
      int result = nanosleep(&delay_, &delay_remaining);
      while (result < 0 &&
             (delay_remaining.tv_sec > 0 || delay_remaining.tv_nsec > 0)) {
        result = nanosleep(&delay_remaining, &delay_remaining);
      }
      wv_content_decryption_module_->CloseSession(sess_id_);
      return false;
    }

    WvContentDecryptionModule* wv_content_decryption_module_;
    CdmSessionId sess_id_;
    struct timespec delay_;
    CORE_DISALLOW_COPY_AND_ASSIGN(CloseSessionThread);
  };

  WvContentDecryptionModule decryptor_;
  CdmKeyMessage key_msg_;
  CdmKeyResponse key_response_;
  CdmSessionId session_id_;
  CdmKeySetId key_set_id_;
  video_widevine::LicenseIdentification license_id_;
};

TEST_F(WvCdmExtendedDurationTest, VerifyLicenseRequestTest) {
  Provision();
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);

  EXPECT_TRUE(!key_msg_.empty());

  // Validate signed request
  SignedMessage signed_message;
  EXPECT_TRUE(signed_message.ParseFromString(key_msg_));
  EXPECT_EQ(SignedMessage::LICENSE_REQUEST, signed_message.type());
  EXPECT_TRUE(signed_message.has_signature());
  EXPECT_TRUE(!signed_message.msg().empty());

  // Verify license request
  video_widevine::LicenseRequest license_request;
  EXPECT_TRUE(license_request.ParseFromString(signed_message.msg()));

  // Verify Client Identification
  const ClientIdentification& client_id = license_request.client_id();
  EXPECT_EQ(
      video_widevine::ClientIdentification_TokenType_DRM_DEVICE_CERTIFICATE,
      client_id.type());

  EXPECT_LT(0, client_id.client_info_size());
  for (int i = 0; i < client_id.client_info_size(); ++i) {
    const ::video_widevine::ClientIdentification_NameValue& name_value =
        client_id.client_info(i);
    EXPECT_TRUE(!name_value.name().empty());
    EXPECT_TRUE(!name_value.value().empty());
  }

  EXPECT_FALSE(client_id.has_provider_client_token());
  EXPECT_FALSE(client_id.has_license_counter());

  const ClientIdentification::ClientCapabilities& client_capabilities =
      client_id.client_capabilities();
  EXPECT_TRUE(client_capabilities.has_client_token());
  EXPECT_TRUE(client_capabilities.client_token());
  EXPECT_TRUE(client_capabilities.has_session_token());
  EXPECT_FALSE(client_capabilities.video_resolution_constraints());
  EXPECT_TRUE(client_capabilities.has_max_hdcp_version());
  EXPECT_TRUE(client_capabilities.has_oem_crypto_api_version());

  // Verify Content Identification
  const LicenseRequest_ContentIdentification& content_id =
      license_request.content_id();
  EXPECT_TRUE(content_id.has_cenc_id_deprecated());
  EXPECT_FALSE(content_id.has_webm_id_deprecated());
  EXPECT_FALSE(content_id.has_existing_license());

  const LicenseRequest_ContentIdentification_CencDeprecated& cenc_id =
      content_id.cenc_id_deprecated();
  EXPECT_TRUE(std::equal(cenc_id.pssh(0).begin(), cenc_id.pssh(0).end(),
                         binary_key_id().begin() + 32));
  EXPECT_EQ(video_widevine::STREAMING, cenc_id.license_type());
  EXPECT_TRUE(cenc_id.has_request_id());

  // Verify other license request fields
  EXPECT_EQ(::video_widevine::LicenseRequest_RequestType_NEW,
            license_request.type());
  EXPECT_LT(0, license_request.request_time());
  EXPECT_EQ(video_widevine::VERSION_2_1, license_request.protocol_version());
  EXPECT_TRUE(license_request.has_key_control_nonce());

  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmExtendedDurationTest, VerifyLicenseRenewalTest) {
  Provision();
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth(),
                           false);

  // Validate signed response
  SignedMessage signed_message;
  EXPECT_TRUE(signed_message.ParseFromString(key_response_));
  EXPECT_EQ(SignedMessage::LICENSE, signed_message.type());
  EXPECT_TRUE(signed_message.has_signature());
  EXPECT_TRUE(!signed_message.msg().empty());

  // Verify license
  video_widevine::License license;
  EXPECT_TRUE(license.ParseFromString(signed_message.msg()));

  // Verify license identification
  video_widevine::LicenseIdentification license_id = license.id();
  EXPECT_LT(0u, license_id.request_id().size());
  EXPECT_LT(0u, license_id.session_id().size());
  EXPECT_EQ(video_widevine::STREAMING, license_id.type());
  EXPECT_FALSE(license_id.has_provider_session_token());

  // Create renewal request
  std::string license_server;
  GenerateRenewalRequest(kLicenseTypeStreaming, &license_server);
  EXPECT_TRUE(!license_server.empty());
  EXPECT_TRUE(!key_msg_.empty());

  // Validate signed renewal request
  signed_message.Clear();
  EXPECT_TRUE(signed_message.ParseFromString(key_msg_));
  EXPECT_EQ(SignedMessage::LICENSE_REQUEST, signed_message.type());
  EXPECT_TRUE(signed_message.has_signature());
  EXPECT_TRUE(!signed_message.msg().empty());

  // Verify license request
  video_widevine::LicenseRequest license_renewal;
  EXPECT_TRUE(license_renewal.ParseFromString(signed_message.msg()));

  // Client Identification not filled in in renewal

  // Verify Content Identification
  const LicenseRequest_ContentIdentification& content_id =
      license_renewal.content_id();
  EXPECT_FALSE(content_id.has_cenc_id_deprecated());
  EXPECT_FALSE(content_id.has_webm_id_deprecated());
  EXPECT_TRUE(content_id.has_existing_license());

  const LicenseRequest_ContentIdentification::ExistingLicense&
      existing_license = content_id.existing_license();

  const LicenseIdentification& id = existing_license.license_id();
  EXPECT_EQ(license_id.request_id(), id.request_id());
  EXPECT_EQ(license_id.session_id(), id.session_id());
  EXPECT_EQ(license_id.purchase_id(), id.purchase_id());
  EXPECT_EQ(license_id.type(), id.type());
  EXPECT_EQ(license_id.version(), id.version());
  EXPECT_TRUE(id.provider_session_token().empty());

  EXPECT_EQ(0, existing_license.seconds_since_started());
  EXPECT_EQ(0, existing_license.seconds_since_last_played());
  EXPECT_TRUE(existing_license.session_usage_table_entry().empty());

  EXPECT_EQ(::video_widevine::LicenseRequest_RequestType_RENEWAL,
            license_renewal.type());
  EXPECT_LT(0, license_renewal.request_time());
  EXPECT_EQ(video_widevine::VERSION_2_1, license_renewal.protocol_version());
  EXPECT_TRUE(license_renewal.has_key_control_nonce());

  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmExtendedDurationTest, DecryptionCloseSessionConcurrencyTest) {
  Unprovision();
  Provision();

  // Leave session open to avoid CDM termination
  CdmSessionId session_id;
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id);

  // Retrieve offline license
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(kOfflineClip2PstInitData, kLicenseTypeOffline);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth(),
                           false);

  EXPECT_FALSE(key_set_id_.empty());

  decryptor_.CloseSession(session_id_);

  for (uint32_t j = 0; j < 500; ++j) {
    decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                           NULL, &session_id_);
    EXPECT_EQ(KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id_));

    CdmResponseType status = NO_ERROR;
    struct timespec decrypt_delay;
    decrypt_delay.tv_sec = 0;
    decrypt_delay.tv_nsec = 10000000ll;  // 10 ms

    CloseSessionThread* thread = new CloseSessionThread();
    thread->Start(&decryptor_, session_id_, 500 /* 500 ms */);
    thread = NULL;

    while (status == NO_ERROR) {
      struct timespec delay_remaining;
      int result = nanosleep(&decrypt_delay, &delay_remaining);
      while (result < 0 &&
             (delay_remaining.tv_sec > 0 || delay_remaining.tv_nsec > 0)) {
        result = nanosleep(&delay_remaining, &delay_remaining);
      }
      SubSampleInfo* data = &kEncryptedOfflineClip2SubSample;
      for (size_t i = 0; i < data->num_of_subsamples; i++) {
        std::vector<uint8_t> decrypt_buffer((data + i)->encrypt_data.size());
        CdmDecryptionParameters decryption_parameters(
            &(data + i)->key_id, &(data + i)->encrypt_data.front(),
            (data + i)->encrypt_data.size(), &(data + i)->iv,
            (data + i)->block_offset, &decrypt_buffer[0]);
        decryption_parameters.is_encrypted = (data + i)->is_encrypted;
        decryption_parameters.is_secure = (data + i)->is_secure;
        decryption_parameters.subsample_flags = (data + i)->subsample_flags;
        status = decryptor_.Decrypt(session_id_, (data + i)->validate_key_id,
                                    decryption_parameters);

        switch (status) {
          case SESSION_NOT_FOUND_FOR_DECRYPT:
          case SESSION_NOT_FOUND_18:
            // Session was closed before decrypt was called. This is expected
            // occasionally as we are testing resilience to concurrency.
            break;
          case NO_ERROR:
            EXPECT_EQ((data + i)->decrypt_data, decrypt_buffer);
            break;
          default:
            EXPECT_TRUE(false) << " Unexpected decrypt result: " << status;
        }
      }
    }
  }
  decryptor_.CloseSession(session_id);
}

// TODO(rfrias): Rewite this test when OEMCrypto v13 is the minimum version
// supported. The limitation of 50 usage entries are no longer present
// when using usage table header+entries.
TEST_F(WvCdmExtendedDurationTest, DISABLED_UsageOverflowTest) {
  Provision();
  TestWvCdmClientPropertySet client_property_set;
  TestWvCdmClientPropertySet* property_set = NULL;

  CdmSecurityLevel security_level = GetDefaultSecurityLevel();
  FileSystem file_system;
  DeviceFiles handle(&file_system);
  EXPECT_TRUE(handle.Init(security_level));
  std::vector<std::string> provider_session_tokens;
  EXPECT_TRUE(handle.DeleteAllUsageInfoForApp(
      DeviceFiles::GetUsageInfoFileName(""), &provider_session_tokens));

  for (size_t i = 0; i < kMaxUsageTableSize + 100; ++i) {
    decryptor_.OpenSession(config_.key_system(), property_set,
                           kDefaultCdmIdentifier, NULL, &session_id_);
    std::string key_id = a2bs_hex(
        "000000427073736800000000"                  // blob size and pssh
        "EDEF8BA979D64ACEA3C827DCD51D21ED00000022"  // Widevine system id
        "08011a0d7769646576696e655f74657374220f73"  // pssh data
        "747265616d696e675f636c697035");

    GenerateKeyRequest(key_id, kLicenseTypeStreaming);
    VerifyKeyRequestResponse(config_.license_server(), config_.client_auth(),
                             false);
    decryptor_.CloseSession(session_id_);
  }

  CdmUsageInfo usage_info;
  CdmUsageInfoReleaseMessage release_msg;
  CdmResponseType status =
      decryptor_.GetUsageInfo("", kDefaultCdmIdentifier, &usage_info);
  EXPECT_EQ(usage_info.empty() ? NO_ERROR : KEY_MESSAGE, status);
  int error_count = 0;
  while (usage_info.size() > 0) {
    for (size_t i = 0; i < usage_info.size(); ++i) {
      release_msg = GetUsageInfoResponse(config_.license_server(),
                                         config_.client_auth(), usage_info[i]);
      EXPECT_EQ(NO_ERROR,
                decryptor_.ReleaseUsageInfo(release_msg, kDefaultCdmIdentifier))
          << i << "/" << usage_info.size() << " (err " << (error_count++) << ")"
          << release_msg;
    }
    ASSERT_LE(error_count, 100);  // Give up after 100 failures.
    status = decryptor_.GetUsageInfo("", kDefaultCdmIdentifier, &usage_info);
    switch (status) {
      case KEY_MESSAGE:
        EXPECT_FALSE(usage_info.empty());
        break;
      case NO_ERROR:
        EXPECT_TRUE(usage_info.empty());
        break;
      default:
        FAIL() << "GetUsageInfo failed with error " << static_cast<int>(status);
        break;
    }
  }
}

// This test verifies that sessions allocated internally during key release
// message generation are deallocated after their time to live period expires
// by timer events (if other sessions are open).
TEST_F(WvCdmExtendedDurationTest, AutomatedOfflineSessionReleaseOnTimerEvent) {
  Unprovision();
  Provision();

  // Leave session open to run the CDM timer
  CdmSessionId streaming_session_id;
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &streaming_session_id);

  // override default settings unless configured through the command line
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);

  uint32_t initial_open_sessions =
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS);

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(kOfflineClip4, kLicenseTypeOffline);
  VerifyKeyRequestResponse(kUatLicenseServer, client_auth, false);

  EXPECT_FALSE(key_set_id_.empty());
  decryptor_.CloseSession(session_id_);
  CdmKeySetId key_set_id = key_set_id_;

  session_id_.clear();
  key_set_id_.clear();
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  GenerateKeyRelease(key_set_id);

  uint32_t open_sessions =
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS);

  EXPECT_GT(open_sessions, initial_open_sessions);

  sleep(kMinute + kClockTolerance);

  open_sessions =
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS);

  EXPECT_EQ(open_sessions, initial_open_sessions);

  session_id_.clear();
  GenerateKeyRelease(key_set_id);
  key_set_id_ = key_set_id;
  VerifyKeyRequestResponse(kUatLicenseServer, client_auth, false);
  decryptor_.CloseSession(streaming_session_id);
}

// This test verifies that sessions allocated internally during key release
// message generation are deallocated after their time to live period expires
// when a new session is opened.
TEST_F(WvCdmExtendedDurationTest, AutomatedOfflineSessionReleaseOnOpenSession) {
  Unprovision();
  Provision();

  // override default settings unless configured through the command line
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);

  uint32_t initial_open_sessions =
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS);

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(kOfflineClip4, kLicenseTypeOffline);
  VerifyKeyRequestResponse(kUatLicenseServer, client_auth, false);

  EXPECT_FALSE(key_set_id_.empty());
  decryptor_.CloseSession(session_id_);
  CdmKeySetId key_set_id = key_set_id_;

  session_id_.clear();
  key_set_id_.clear();
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  GenerateKeyRelease(key_set_id);

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);

  EXPECT_GT(
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS),
      initial_open_sessions);

  decryptor_.CloseSession(session_id_);

  EXPECT_GT(
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS),
      initial_open_sessions);

  sleep(kMinute + kClockTolerance);

  EXPECT_EQ(
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS),
      initial_open_sessions);

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);

  EXPECT_GT(
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS),
      initial_open_sessions);

  decryptor_.CloseSession(session_id_);

  EXPECT_EQ(
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS),
      initial_open_sessions);

  session_id_.clear();
  GenerateKeyRelease(key_set_id);
  key_set_id_ = key_set_id;
  VerifyKeyRequestResponse(kUatLicenseServer, client_auth, false);
}

// This test verifies that sessions allocated internally during
// key release message generation are deallocated after their
// time to live period expires.
// TODO: Disabled till b/32617908 is addressed
TEST_F(WvCdmExtendedDurationTest, DISABLED_AutomatedOfflineSessionReleaseTest) {
  Unprovision();
  Provision();

  // override default settings unless configured through the command line
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);

  uint32_t initial_open_sessions =
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS);

  uint32_t max_sessions =
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_MAX_NUMBER_OF_SESSIONS);

  uint32_t num_key_set_ids = max_sessions - initial_open_sessions;
  if (num_key_set_ids > kMaxUsageTableSize)
    num_key_set_ids = kMaxUsageTableSize;

  std::set<std::string> key_set_id_map;
  for (uint32_t i = 0; i < num_key_set_ids; ++i) {
    decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                           NULL, &session_id_);
    GenerateKeyRequest(kOfflineClip4, kLicenseTypeOffline);
    VerifyKeyRequestResponse(kUatLicenseServer, client_auth, false);

    EXPECT_FALSE(key_set_id_.empty());
    decryptor_.CloseSession(session_id_);
    key_set_id_map.insert(key_set_id_);
  }

  std::set<std::string>::iterator iter;
  for (iter = key_set_id_map.begin(); iter != key_set_id_map.end(); ++iter) {
    session_id_.clear();
    key_set_id_.clear();
    decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                           NULL, &session_id_);
    EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, *iter));
    decryptor_.CloseSession(session_id_);
  }

  for (iter = key_set_id_map.begin(); iter != key_set_id_map.end(); ++iter) {
    session_id_.clear();
    GenerateKeyRelease(*iter);
  }

  uint32_t open_sessions =
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS);

  EXPECT_EQ(open_sessions, key_set_id_map.size() + initial_open_sessions);

  sleep(kMinute + kClockTolerance);

  iter = key_set_id_map.begin();
  session_id_.clear();
  GenerateKeyRelease(*iter);

  open_sessions =
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS);

  EXPECT_GE(open_sessions, initial_open_sessions);
  EXPECT_LE(open_sessions - initial_open_sessions, key_set_id_map.size());

  for (iter = key_set_id_map.begin(); iter != key_set_id_map.end(); ++iter) {
    session_id_.clear();
    GenerateKeyRelease(*iter);
    key_set_id_ = *iter;
    VerifyKeyRequestResponse(kUatLicenseServer, client_auth, false);
  }
}

class WvCdmStreamingNoPstTest : public WvCdmExtendedDurationTest,
                                public ::testing::WithParamInterface<size_t> {};

TEST_P(WvCdmStreamingNoPstTest, UsageTest) {
  Unprovision();
  Provision();

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth(),
                           false);

  ValidateResponse(video_widevine::STREAMING, false);

  int64_t initial_license_duration_remaining = 0;
  int64_t initial_playback_duration_remaining = 0;
  QueryKeyStatus(true, true, &initial_license_duration_remaining,
                 &initial_playback_duration_remaining);

  sleep(kMinute);
  int64_t expected_seconds_since_license_received = kMinute;
  int64_t expected_seconds_since_initial_playback = 0;
  int64_t expected_seconds_since_last_playback = 0;

  for (size_t i = 0; i < GetParam(); ++i) {
    // Decrypt data
    SubSampleInfo* data = &kEncryptedStreamingNoPstSubSample;
    for (size_t i = 0; i < data->num_of_subsamples; i++) {
      std::vector<uint8_t> decrypt_buffer((data + i)->encrypt_data.size());
      CdmDecryptionParameters decryption_parameters(
          &(data + i)->key_id, &(data + i)->encrypt_data.front(),
          (data + i)->encrypt_data.size(), &(data + i)->iv,
          (data + i)->block_offset, &decrypt_buffer[0]);
      decryption_parameters.is_encrypted = (data + i)->is_encrypted;
      decryption_parameters.is_secure = (data + i)->is_secure;
      decryption_parameters.subsample_flags = (data + i)->subsample_flags;
      EXPECT_EQ(NO_ERROR,
                decryptor_.Decrypt(session_id_, (data + i)->validate_key_id,
                                   decryption_parameters));

      EXPECT_EQ((data + i)->decrypt_data, decrypt_buffer);
    }

    sleep(kMinute);
    expected_seconds_since_license_received += kMinute;
    expected_seconds_since_initial_playback += kMinute;
    expected_seconds_since_last_playback = kMinute;
  }

  // Create renewal request and validate
  std::string license_server;
  GenerateRenewalRequest(kLicenseTypeStreaming, &license_server);
  EXPECT_TRUE(!license_server.empty());
  EXPECT_TRUE(!key_msg_.empty());

  ValidateRenewalRequest(expected_seconds_since_initial_playback,
                         expected_seconds_since_last_playback, false);

  // Query and validate usage information
  int64_t license_duration_remaining = 0;
  int64_t playback_duration_remaining = 0;
  QueryKeyStatus(true, true, &license_duration_remaining,
                 &playback_duration_remaining);

  EXPECT_NEAR(initial_license_duration_remaining - license_duration_remaining,
              expected_seconds_since_license_received, kClockTolerance);
  EXPECT_NEAR(initial_playback_duration_remaining - playback_duration_remaining,
              expected_seconds_since_initial_playback, kClockTolerance);

  decryptor_.CloseSession(session_id_);
}

INSTANTIATE_TEST_CASE_P(Cdm, WvCdmStreamingNoPstTest,
                        ::testing::Values(0, 1, 2));

class WvCdmStreamingPstTest : public WvCdmExtendedDurationTest,
                              public ::testing::WithParamInterface<size_t> {};

TEST_P(WvCdmStreamingPstTest, UsageTest) {
  Unprovision();
  Provision();

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(kStreamingClip8PstInitData, kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth(),
                           false);

  ValidateResponse(video_widevine::STREAMING, true);

  int64_t initial_license_duration_remaining = 0;
  int64_t initial_playback_duration_remaining = 0;
  QueryKeyStatus(true, false, &initial_license_duration_remaining,
                 &initial_playback_duration_remaining);

  sleep(kMinute);
  int64_t expected_seconds_since_license_received = kMinute;
  int64_t expected_seconds_since_initial_playback = 0;
  int64_t expected_seconds_since_last_playback = 0;

  for (size_t i = 0; i < GetParam(); ++i) {
    // Decrypt data
    SubSampleInfo* data = &kEncryptedStreamingClip8SubSample;
    for (size_t i = 0; i < data->num_of_subsamples; i++) {
      std::vector<uint8_t> decrypt_buffer((data + i)->encrypt_data.size());
      CdmDecryptionParameters decryption_parameters(
          &(data + i)->key_id, &(data + i)->encrypt_data.front(),
          (data + i)->encrypt_data.size(), &(data + i)->iv,
          (data + i)->block_offset, &decrypt_buffer[0]);
      decryption_parameters.is_encrypted = (data + i)->is_encrypted;
      decryption_parameters.is_secure = (data + i)->is_secure;
      decryption_parameters.subsample_flags = (data + i)->subsample_flags;
      EXPECT_EQ(NO_ERROR,
                decryptor_.Decrypt(session_id_, (data + i)->validate_key_id,
                                   decryption_parameters));

      EXPECT_EQ((data + i)->decrypt_data, decrypt_buffer);
    }

    sleep(kMinute);
    expected_seconds_since_license_received += kMinute;
    expected_seconds_since_initial_playback += kMinute;
    expected_seconds_since_last_playback = kMinute;
  }

  // Query and validate usage information
  int64_t license_duration_remaining = 0;
  int64_t playback_duration_remaining = 0;
  QueryKeyStatus(true, false, &license_duration_remaining,
                 &playback_duration_remaining);

  EXPECT_NEAR(initial_license_duration_remaining - license_duration_remaining,
              expected_seconds_since_license_received, kClockTolerance);
  EXPECT_NEAR(initial_playback_duration_remaining - playback_duration_remaining,
              expected_seconds_since_initial_playback, kClockTolerance);

  decryptor_.CloseSession(session_id_);
}

INSTANTIATE_TEST_CASE_P(Cdm, WvCdmStreamingPstTest, ::testing::Values(0, 1, 2));

class WvCdmStreamingUsageReportTest
    : public WvCdmExtendedDurationTest,
      public ::testing::WithParamInterface<size_t> {};

TEST_P(WvCdmStreamingUsageReportTest, UsageTest) {
  Unprovision();
  Provision();

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(kStreamingClip8PstInitData, kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth(),
                           false);

  ValidateResponse(video_widevine::STREAMING, true);

  int64_t initial_license_duration_remaining = 0;
  int64_t initial_playback_duration_remaining = 0;
  QueryKeyStatus(true, false, &initial_license_duration_remaining,
                 &initial_playback_duration_remaining);

  sleep(kMinute);
  int64_t expected_seconds_since_license_received = kMinute;
  int64_t expected_seconds_since_initial_playback = 0;
  int64_t expected_seconds_since_last_playback = 0;

  for (size_t i = 0; i < GetParam(); ++i) {
    // Decrypt data
    SubSampleInfo* data = &kEncryptedStreamingClip8SubSample;
    for (size_t i = 0; i < data->num_of_subsamples; i++) {
      std::vector<uint8_t> decrypt_buffer((data + i)->encrypt_data.size());
      CdmDecryptionParameters decryption_parameters(
          &(data + i)->key_id, &(data + i)->encrypt_data.front(),
          (data + i)->encrypt_data.size(), &(data + i)->iv,
          (data + i)->block_offset, &decrypt_buffer[0]);
      decryption_parameters.is_encrypted = (data + i)->is_encrypted;
      decryption_parameters.is_secure = (data + i)->is_secure;
      decryption_parameters.subsample_flags = (data + i)->subsample_flags;
      EXPECT_EQ(NO_ERROR,
                decryptor_.Decrypt(session_id_, (data + i)->validate_key_id,
                                   decryption_parameters));

      EXPECT_EQ((data + i)->decrypt_data, decrypt_buffer);
    }

    sleep(kMinute);
    expected_seconds_since_license_received += kMinute;
    expected_seconds_since_initial_playback += kMinute;
    expected_seconds_since_last_playback = kMinute;
  }

  // Query and validate usage information
  int64_t license_duration_remaining = 0;
  int64_t playback_duration_remaining = 0;
  QueryKeyStatus(true, false, &license_duration_remaining,
                 &playback_duration_remaining);

  EXPECT_NEAR(initial_license_duration_remaining - license_duration_remaining,
              expected_seconds_since_license_received, kClockTolerance);
  EXPECT_NEAR(initial_playback_duration_remaining - playback_duration_remaining,
              expected_seconds_since_initial_playback, kClockTolerance);

  decryptor_.CloseSession(session_id_);

  // Create usage report and validate
  CdmUsageInfo usage_info;
  CdmUsageInfoReleaseMessage release_msg;
  CdmResponseType status =
      decryptor_.GetUsageInfo("", kDefaultCdmIdentifier, &usage_info);
  EXPECT_EQ(usage_info.empty() ? NO_ERROR : KEY_MESSAGE, status);
  int error_count = 0;
  while (usage_info.size() > 0) {
    for (size_t i = 0; i < usage_info.size(); ++i) {
      ValidateReleaseRequest(usage_info[i],
                             expected_seconds_since_initial_playback != 0,
                             expected_seconds_since_license_received,
                             expected_seconds_since_initial_playback,
                             expected_seconds_since_last_playback);
      release_msg = GetUsageInfoResponse(config_.license_server(),
                                         config_.client_auth(), usage_info[i]);
      EXPECT_EQ(NO_ERROR,
                decryptor_.ReleaseUsageInfo(release_msg, kDefaultCdmIdentifier))
          << i << "/" << usage_info.size() << " (err " << (error_count++) << ")"
          << release_msg;
    }
    ASSERT_LE(error_count, 100);  // Give up after 100 failures.
    status = decryptor_.GetUsageInfo("", kDefaultCdmIdentifier, &usage_info);
    switch (status) {
      case KEY_MESSAGE:
        EXPECT_FALSE(usage_info.empty());
        break;
      case NO_ERROR:
        EXPECT_TRUE(usage_info.empty());
        break;
      default:
        FAIL() << "GetUsageInfo failed with error " << static_cast<int>(status);
        break;
    }
  }

  // Validate that update usage table entry is exercised.
  drm_metrics::WvCdmMetrics metrics;
  ASSERT_EQ(NO_ERROR, decryptor_.GetMetrics(kDefaultCdmIdentifier, &metrics));
  ValidateHasUpdateUsageEntry(metrics);
}

INSTANTIATE_TEST_CASE_P(Cdm, WvCdmStreamingUsageReportTest,
                        ::testing::Values(0, 1, 2));

class WvCdmOfflineUsageReportTest
    : public WvCdmExtendedDurationTest,
      public ::testing::WithParamInterface<size_t> {};

TEST_P(WvCdmOfflineUsageReportTest, UsageTest) {
  Unprovision();
  Provision();

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(kOfflineClip2PstInitData, kLicenseTypeOffline);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth(),
                           false);

  ValidateResponse(video_widevine::OFFLINE, true);

  CdmKeySetId key_set_id = key_set_id_;
  EXPECT_FALSE(key_set_id_.empty());

  int64_t initial_license_duration_remaining = 0;
  int64_t initial_playback_duration_remaining = 0;
  QueryKeyStatus(false, true, &initial_license_duration_remaining,
                 &initial_playback_duration_remaining);

  decryptor_.CloseSession(session_id_);

  sleep(kMinute);
  int64_t expected_seconds_since_license_received = kMinute;
  int64_t expected_seconds_since_initial_playback = 0;
  int64_t expected_seconds_since_last_playback = 0;

  for (size_t i = 0; i < GetParam(); ++i) {
    session_id_.clear();
    decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                           NULL, &session_id_);
    EXPECT_EQ(KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));

    // Query and validate usage information
    int64_t license_duration_remaining = 0;
    int64_t playback_duration_remaining = 0;
    QueryKeyStatus(false, true, &license_duration_remaining,
                   &playback_duration_remaining);

    EXPECT_NEAR(initial_license_duration_remaining - license_duration_remaining,
                expected_seconds_since_license_received, kClockTolerance);
    EXPECT_NEAR(
        initial_playback_duration_remaining - playback_duration_remaining,
        expected_seconds_since_initial_playback, kClockTolerance);

    // Decrypt data
    SubSampleInfo* data = &kEncryptedOfflineClip2SubSample;
    for (size_t i = 0; i < data->num_of_subsamples; i++) {
      std::vector<uint8_t> decrypt_buffer((data + i)->encrypt_data.size());
      CdmDecryptionParameters decryption_parameters(
          &(data + i)->key_id, &(data + i)->encrypt_data.front(),
          (data + i)->encrypt_data.size(), &(data + i)->iv,
          (data + i)->block_offset, &decrypt_buffer[0]);
      decryption_parameters.is_encrypted = (data + i)->is_encrypted;
      decryption_parameters.is_secure = (data + i)->is_secure;
      decryption_parameters.subsample_flags = (data + i)->subsample_flags;
      EXPECT_EQ(NO_ERROR,
                decryptor_.Decrypt(session_id_, (data + i)->validate_key_id,
                                   decryption_parameters));

      EXPECT_EQ((data + i)->decrypt_data, decrypt_buffer);
    }

    sleep(10);
    decryptor_.CloseSession(session_id_);

    sleep(kMinute - 10);
    expected_seconds_since_license_received += kMinute;
    expected_seconds_since_initial_playback += kMinute;
    expected_seconds_since_last_playback = kMinute;
  }

  session_id_.clear();
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  EXPECT_EQ(KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));

  // Query and validate usage information
  int64_t license_duration_remaining = 0;
  int64_t playback_duration_remaining = 0;
  QueryKeyStatus(false, true, &license_duration_remaining,
                 &playback_duration_remaining);

  EXPECT_NEAR(initial_license_duration_remaining - license_duration_remaining,
              expected_seconds_since_license_received, kClockTolerance);
  EXPECT_NEAR(initial_playback_duration_remaining - playback_duration_remaining,
              expected_seconds_since_initial_playback, kClockTolerance);

  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  GenerateKeyRelease(key_set_id);
  ValidateReleaseRequest(key_msg_, expected_seconds_since_initial_playback != 0,
                         expected_seconds_since_license_received,
                         expected_seconds_since_initial_playback,
                         expected_seconds_since_last_playback);
  key_set_id_ = key_set_id;
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth(),
                           false);
}

INSTANTIATE_TEST_CASE_P(Cdm, WvCdmOfflineUsageReportTest,
                        ::testing::Values(0, 1, 2));

// This test verifies that a device can still work if the maximum capacity
// of the usage entry table is reached. New usage entries, for offline
// licenses, can still be added and existing licenses can still be played back.
TEST_F(WvCdmExtendedDurationTest, MaxUsageEntryOfflineRecoveryTest) {
  Unprovision();
  Provision();

  // override default settings unless configured through the command line
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);
  std::vector<CdmKeySetId> key_set_ids;

  // Download large number of offline licenses. If OEMCrypto returns
  // OEMCrypto_ERROR_INSUFFICIENT_RESOURCES when usage table is at capacity,
  // licenses will be deleted internally to make space and we will
  // not encounter an error.
  for (size_t i = 0; i < 2000; ++i) {
    decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                           NULL, &session_id_);
    GenerateKeyRequest(kOfflineClip2PstInitData, kLicenseTypeOffline);
    VerifyKeyRequestResponse(config_.license_server(), client_auth, false);

    key_set_ids.push_back(key_set_id_);

    decryptor_.CloseSession(session_id_);
  }

  uint32_t number_of_valid_offline_sessions = 0;
  for (size_t i = 0; i < key_set_ids.size(); ++i) {
    session_id_.clear();
    decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                           NULL, &session_id_);
    CdmResponseType result = decryptor_.RestoreKey(session_id_, key_set_ids[i]);

    if (result == KEY_ADDED) {
      ++number_of_valid_offline_sessions;

      // Decrypt data
      SubSampleInfo* data = &kEncryptedOfflineClip2SubSample;
      std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
      CdmDecryptionParameters decryption_parameters(
          &data->key_id, &data->encrypt_data.front(),
          data->encrypt_data.size(), &data->iv,
          data->block_offset, &decrypt_buffer[0]);
      decryption_parameters.is_encrypted = data->is_encrypted;
      decryption_parameters.is_secure = data->is_secure;
      decryption_parameters.subsample_flags = data->subsample_flags;
      EXPECT_EQ(NO_ERROR,
                decryptor_.Decrypt(session_id_, data->validate_key_id,
                                   decryption_parameters));

      EXPECT_EQ(data->decrypt_data, decrypt_buffer);

      decryptor_.CloseSession(session_id_);

      // Release the license
      GenerateKeyRelease(key_set_ids[i]);
      key_set_id_ = key_set_ids[i];
      VerifyKeyRequestResponse(config_.license_server(), client_auth, false);
    } else {
      decryptor_.CloseSession(session_id_);
    }
  }

  EXPECT_GE(number_of_valid_offline_sessions, 200u);
}

}  // namespace wvcdm
