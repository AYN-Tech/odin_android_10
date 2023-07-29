// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
// These tests are for the generic crypto operations.  They call on the
// CdmEngine class and exercise the classes below it as well.  In
// particular, we assume that the OEMCrypo layer works, and has a valid keybox.
// This is because we need a valid RSA certificate, and will attempt to connect
// to the provisioning server to request one if we don't.

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "cdm_engine.h"
#include "config_test_env.h"
#include "license_request.h"
#include "log.h"
#include "oec_session_util.h"
#include "oemcrypto_session_tests_helper.h"
#include "oemcrypto_types.h"
#include "platform.h"
#include "properties.h"
#include "string_conversions.h"
#include "test_base.h"
#include "test_printers.h"
#include "url_request.h"
#include "wv_cdm_constants.h"
#include "wv_cdm_types.h"

namespace wvcdm {

class WvGenericOperationsTest : public WvCdmTestBase {
 public:
  WvGenericOperationsTest()
      : dummy_engine_metrics_(new metrics::EngineMetrics),
        cdm_engine_(&file_system_, dummy_engine_metrics_),
        holder_(&cdm_engine_) {}

  void SetUp() override {
    WvCdmTestBase::SetUp();
    EnsureProvisioned();
    holder_.OpenSession(config_.key_system());
    holder_.GenerateKeyRequest(binary_key_id(), ISO_BMFF_VIDEO_MIME_TYPE);
    holder_.CreateDefaultLicense();

    ency_id_ = "ency";
    StripeBuffer(&ency_key_, CONTENT_KEY_SIZE, 'e');
    AddOneKey(ency_id_, ency_key_, wvoec::kControlAllowEncrypt);
    dency_id_ = "dency";
    StripeBuffer(&dency_key_, CONTENT_KEY_SIZE, 'd');
    AddOneKey(dency_id_, dency_key_, wvoec::kControlAllowDecrypt);
    siggy_id_ = "siggy";
    StripeBuffer(&siggy_key_, MAC_KEY_SIZE, 's');
    AddOneKey(siggy_id_, siggy_key_, wvoec::kControlAllowSign);
    vou_id_ = "vou";
    StripeBuffer(&vou_key_, MAC_KEY_SIZE, 'v');
    AddOneKey(vou_id_, vou_key_, wvoec::kControlAllowVerify);

    StripeBuffer(&in_vector_, CONTENT_KEY_SIZE * 15, '1');
    in_buffer_ = std::string(in_vector_.begin(), in_vector_.end());

    StripeBuffer(&iv_vector_, KEY_IV_SIZE, 'a');
    iv_ = std::string(iv_vector_.begin(), iv_vector_.end());
  }

  void TearDown() override { holder_.CloseSession(); }

  // Create a single key, and add it to the license.
  void AddOneKey(const KeyId& key_id, const std::vector<uint8_t>& key_data,
                 uint32_t key_control_block) {
    wvoec::KeyControlBlock block = {};
    block.control_bits = htonl(key_control_block);
    holder_.AddKey(key_id, key_data, block);
  }

 protected:
  FileSystem file_system_;
  std::shared_ptr<metrics::EngineMetrics> dummy_engine_metrics_;
  CdmEngine cdm_engine_;
  TestLicenseHolder holder_;

  KeyId ency_id_;
  KeyId dency_id_;
  KeyId siggy_id_;
  KeyId vou_id_;

  std::vector<uint8_t> ency_key_;
  std::vector<uint8_t> dency_key_;
  std::vector<uint8_t> siggy_key_;
  std::vector<uint8_t> vou_key_;

  std::vector<uint8_t> in_vector_;
  std::vector<uint8_t> iv_vector_;
  std::string in_buffer_;
  std::string iv_;
};

TEST_F(WvGenericOperationsTest, LoadSpecialKeys) {
  holder_.SignAndLoadLicense();
}

TEST_F(WvGenericOperationsTest, GenericEncryptGood) {
  CdmResponseType cdm_sts;
  std::string encrypted = Aes128CbcEncrypt(ency_key_, in_vector_, iv_vector_);
  std::string out_buffer;

  holder_.SignAndLoadLicense();
  cdm_sts = cdm_engine_.GenericEncrypt(
      holder_.session_id(), in_buffer_, ency_id_, iv_,
      wvcdm::kEncryptionAlgorithmAesCbc128, &out_buffer);

  EXPECT_EQ(NO_ERROR, cdm_sts);
  EXPECT_EQ(encrypted, out_buffer);
}

TEST_F(WvGenericOperationsTest, GenericEncryptNoKey) {
  CdmResponseType cdm_sts;
  std::string encrypted = Aes128CbcEncrypt(ency_key_, in_vector_, iv_vector_);
  std::string out_buffer;
  KeyId key_id("no_key");

  holder_.SignAndLoadLicense();
  cdm_sts = cdm_engine_.GenericEncrypt(
      holder_.session_id(), in_buffer_, key_id, iv_,
      wvcdm::kEncryptionAlgorithmAesCbc128, &out_buffer);
  EXPECT_EQ(NO_CONTENT_KEY_2, cdm_sts);
  EXPECT_NE(encrypted, out_buffer);
}

TEST_F(WvGenericOperationsTest, GenericEncryptKeyNotAllowed) {
  CdmResponseType cdm_sts;
  // Trying to use Decrypt key to encrypt, which is not allowed.
  KeyId key_id = dency_id_;
  std::string encrypted = Aes128CbcEncrypt(dency_key_, in_vector_, iv_vector_);
  std::string out_buffer;

  holder_.SignAndLoadLicense();
  cdm_sts = cdm_engine_.GenericEncrypt(
      holder_.session_id(), in_buffer_, key_id, iv_,
      wvcdm::kEncryptionAlgorithmAesCbc128, &out_buffer);
  EXPECT_EQ(UNKNOWN_ERROR, cdm_sts);
  EXPECT_NE(encrypted, out_buffer);
}

TEST_F(WvGenericOperationsTest, GenericDecryptGood) {
  CdmResponseType cdm_sts;
  std::string decrypted = Aes128CbcDecrypt(dency_key_, in_vector_, iv_vector_);
  std::string out_buffer;

  holder_.SignAndLoadLicense();
  cdm_sts = cdm_engine_.GenericDecrypt(
      holder_.session_id(), in_buffer_, dency_id_, iv_,
      wvcdm::kEncryptionAlgorithmAesCbc128, &out_buffer);

  EXPECT_EQ(NO_ERROR, cdm_sts);
  EXPECT_EQ(decrypted, out_buffer);
}

TEST_F(WvGenericOperationsTest, GenericDecryptNoKey) {
  CdmResponseType cdm_sts;
  std::string decrypted = Aes128CbcDecrypt(dency_key_, in_vector_, iv_vector_);
  std::string out_buffer;
  KeyId key_id = "no_key";

  holder_.SignAndLoadLicense();
  cdm_sts = cdm_engine_.GenericDecrypt(
      holder_.session_id(), in_buffer_, key_id, iv_,
      wvcdm::kEncryptionAlgorithmAesCbc128, &out_buffer);
  EXPECT_EQ(NO_CONTENT_KEY_2, cdm_sts);
  EXPECT_NE(decrypted, out_buffer);
}

TEST_F(WvGenericOperationsTest, GenericDecryptKeyNotAllowed) {
  CdmResponseType cdm_sts;
  // Trying to use Encrypt key to decrypt, which is not allowed.
  KeyId key_id = ency_id_;
  std::string decrypted = Aes128CbcDecrypt(ency_key_, in_vector_, iv_vector_);
  std::string out_buffer;

  holder_.SignAndLoadLicense();
  cdm_sts = cdm_engine_.GenericDecrypt(
      holder_.session_id(), in_buffer_, key_id, iv_,
      wvcdm::kEncryptionAlgorithmAesCbc128, &out_buffer);
  EXPECT_EQ(UNKNOWN_ERROR, cdm_sts);
  EXPECT_NE(decrypted, out_buffer);
}

TEST_F(WvGenericOperationsTest, GenericSignKeyNotAllowed) {
  CdmResponseType cdm_sts;
  // Wrong key
  std::string key_id = vou_id_;
  std::string out_buffer;
  std::string signature = SignHMAC(in_buffer_, siggy_key_);

  holder_.SignAndLoadLicense();
  cdm_sts =
      cdm_engine_.GenericSign(holder_.session_id(), in_buffer_, key_id,
                              wvcdm::kSigningAlgorithmHmacSha256, &out_buffer);
  EXPECT_EQ(UNKNOWN_ERROR, cdm_sts);
  EXPECT_NE(signature, out_buffer);
}

TEST_F(WvGenericOperationsTest, GenericSignGood) {
  CdmResponseType cdm_sts;
  std::string out_buffer;
  std::string signature = SignHMAC(in_buffer_, siggy_key_);

  holder_.SignAndLoadLicense();
  cdm_sts =
      cdm_engine_.GenericSign(holder_.session_id(), in_buffer_, siggy_id_,
                              wvcdm::kSigningAlgorithmHmacSha256, &out_buffer);
  EXPECT_EQ(NO_ERROR, cdm_sts);
  EXPECT_EQ(signature, out_buffer);
}

TEST_F(WvGenericOperationsTest, GenericVerifyKeyNotAllowed) {
  CdmResponseType cdm_sts;
  // Wrong key
  std::string key_id = siggy_id_;
  std::string signature = SignHMAC(in_buffer_, siggy_key_);

  holder_.SignAndLoadLicense();
  cdm_sts = cdm_engine_.GenericVerify(holder_.session_id(), in_buffer_, key_id,
                                      wvcdm::kSigningAlgorithmHmacSha256,
                                      signature);
  EXPECT_EQ(UNKNOWN_ERROR, cdm_sts);
}

TEST_F(WvGenericOperationsTest, GenericVerifyBadSignautre) {
  CdmResponseType cdm_sts;
  std::string signature(MAC_KEY_SIZE, 's');

  holder_.SignAndLoadLicense();
  cdm_sts = cdm_engine_.GenericVerify(
      holder_.session_id(), in_buffer_, vou_id_,
      wvcdm::kSigningAlgorithmHmacSha256, signature);
  // OEMCrypto error is OEMCrypto_ERROR_SIGNATURE_FAILURE
  EXPECT_EQ(UNKNOWN_ERROR, cdm_sts);
}

TEST_F(WvGenericOperationsTest, GenericVerifyGood) {
  CdmResponseType cdm_sts;
  std::string signature = SignHMAC(in_buffer_, vou_key_);

  holder_.SignAndLoadLicense();
  cdm_sts =
      cdm_engine_.GenericVerify(holder_.session_id(), in_buffer_, vou_id_,
                                wvcdm::kSigningAlgorithmHmacSha256, signature);
  EXPECT_EQ(NO_ERROR, cdm_sts);
}

TEST_F(WvGenericOperationsTest, GenericEncryptDecrypt) {
  CdmResponseType cdm_sts;
  std::string out_buffer;
  std::string clear_buffer;

  KeyId key_id = "enc and dec";

  std::vector<uint8_t> key_data;
  StripeBuffer(&key_data, CONTENT_KEY_SIZE, '3');
  AddOneKey(key_id, key_data,
            wvoec::kControlAllowEncrypt | wvoec::kControlAllowDecrypt);

  holder_.SignAndLoadLicense();

  cdm_sts = cdm_engine_.GenericEncrypt(
      holder_.session_id(), in_buffer_, key_id, iv_,
      wvcdm::kEncryptionAlgorithmAesCbc128, &out_buffer);
  EXPECT_EQ(NO_ERROR, cdm_sts);

  cdm_sts = cdm_engine_.GenericDecrypt(
      holder_.session_id(), out_buffer, key_id, iv_,
      wvcdm::kEncryptionAlgorithmAesCbc128, &clear_buffer);
  EXPECT_EQ(NO_ERROR, cdm_sts);

  EXPECT_EQ(in_buffer_, clear_buffer);
}

TEST_F(WvGenericOperationsTest, GenericSignVerify) {
  CdmResponseType cdm_sts;
  std::string signature_buffer;

  KeyId key_id = "sign and ver";

  std::vector<uint8_t> key_data;
  StripeBuffer(&key_data, MAC_KEY_SIZE, '4');
  AddOneKey(key_id, key_data,
            wvoec::kControlAllowSign | wvoec::kControlAllowVerify);
  std::string signature = SignHMAC(in_buffer_, key_data);

  holder_.SignAndLoadLicense();

  cdm_sts = cdm_engine_.GenericSign(holder_.session_id(), in_buffer_, key_id,
                                    wvcdm::kSigningAlgorithmHmacSha256,
                                    &signature_buffer);
  EXPECT_EQ(NO_ERROR, cdm_sts);
  EXPECT_EQ(MAC_KEY_SIZE, signature_buffer.size());

  cdm_sts = cdm_engine_.GenericVerify(holder_.session_id(), in_buffer_, key_id,
                                      wvcdm::kSigningAlgorithmHmacSha256,
                                      signature_buffer);
  EXPECT_EQ(NO_ERROR, cdm_sts);
  EXPECT_EQ(signature, signature_buffer);
}

}  // namespace wvcdm
