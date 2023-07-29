// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_CORE_CRYPTO_SESSION_H_
#define WVCDM_CORE_CRYPTO_SESSION_H_

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "OEMCryptoCENC.h"
#include "disallow_copy_and_assign.h"
#include "key_session.h"
#include "metrics_collections.h"
#include "oemcrypto_adapter.h"
#include "rw_lock.h"
#include "timer_metric.h"
#include "wv_cdm_types.h"

namespace wvcdm {

class CryptoKey;
class UsageTableHeader;

typedef std::map<std::string, CryptoKey*> CryptoKeyMap;

// Crypto session utility functions used by KeySession implementations.
void GenerateMacContext(const std::string& input_context,
                        std::string* deriv_context);
void GenerateEncryptContext(const std::string& input_context,
                            std::string* deriv_context);
size_t GetOffset(std::string message, std::string field);
OEMCrypto_Substring GetSubstring(const std::string& message = "",
                                 const std::string& field = "",
                                 bool set_zero = false);
OEMCryptoCipherMode ToOEMCryptoCipherMode(CdmCipherMode cipher_mode);

class CryptoSessionFactory;

class CryptoSession {
 public:
  typedef OEMCrypto_HDCP_Capability HdcpCapability;
  typedef enum {
    kUsageDurationsInvalid = 0,
    kUsageDurationPlaybackNotBegun = 1,
    kUsageDurationsValid = 2,
  } UsageDurationStatus;

  struct SupportedCertificateTypes {
    bool rsa_2048_bit;
    bool rsa_3072_bit;
    bool rsa_cast;
  };

  // Creates an instance of CryptoSession with the given |crypto_metrics|.
  // |crypto_metrics| is owned by the caller, must NOT be null, and must
  // exist as long as the new CryptoSession exists.
  static CryptoSession* MakeCryptoSession(
      metrics::CryptoMetrics* crypto_metrics);

  virtual ~CryptoSession();

  virtual CdmResponseType GetProvisioningToken(std::string* client_token);
  virtual CdmClientTokenType GetPreProvisionTokenType() {
    return pre_provision_token_type_;
  }

  // The overloaded methods with |requested_level| may be called
  // without a preceding call to Open. The other method must call Open first.
  virtual CdmSecurityLevel GetSecurityLevel();
  virtual CdmSecurityLevel GetSecurityLevel(SecurityLevel requested_level);
  virtual bool GetApiVersion(uint32_t* version);
  virtual bool GetApiVersion(SecurityLevel requested_level, uint32_t* version);

  virtual CdmResponseType GetInternalDeviceUniqueId(std::string* device_id);
  virtual CdmResponseType GetExternalDeviceUniqueId(std::string* device_id);
  virtual bool GetSystemId(uint32_t* system_id);
  virtual CdmResponseType GetProvisioningId(std::string* provisioning_id);
  virtual uint8_t GetSecurityPatchLevel();

  virtual CdmResponseType Open() { return Open(kLevelDefault); }
  virtual CdmResponseType Open(SecurityLevel requested_security_level);
  virtual void Close();

  virtual bool IsOpen() { return open_; }
  virtual CryptoSessionId oec_session_id() { return oec_session_id_; }

  // Key request/response
  virtual const std::string& request_id() { return request_id_; }
  virtual CdmResponseType PrepareRequest(const std::string& key_deriv_message,
                                         bool is_provisioning,
                                         std::string* signature);
  virtual CdmResponseType PrepareRenewalRequest(const std::string& message,
                                                std::string* signature);
  virtual CdmResponseType LoadKeys(
      const std::string& message, const std::string& signature,
      const std::string& mac_key_iv, const std::string& mac_key,
      const std::vector<CryptoKey>& key_array,
      const std::string& provider_session_token,
      const std::string& srm_requirement,
      CdmLicenseKeyType key_type);
  virtual CdmResponseType LoadEntitledContentKeys(
      const std::vector<CryptoKey>& key_array);
  virtual CdmResponseType LoadCertificatePrivateKey(std::string& wrapped_key);
  virtual CdmResponseType RefreshKeys(const std::string& message,
                                      const std::string& signature,
                                      int num_keys,
                                      const CryptoKey* key_array);
  virtual CdmResponseType GenerateNonce(uint32_t* nonce);
  virtual CdmResponseType GenerateDerivedKeys(const std::string& message);
  virtual CdmResponseType GenerateDerivedKeys(const std::string& message,
                                              const std::string& session_key);
  virtual CdmResponseType RewrapCertificate(
      const std::string& signed_message,
      const std::string& signature,
      const std::string& nonce,
      const std::string& private_key,
      const std::string& iv,
      const std::string& wrapping_key,
      std::string* wrapped_private_key);

  // Media data path
  virtual CdmResponseType Decrypt(const CdmDecryptionParameters& params);

  // Usage related methods
  // The overloaded method with |security_level| may be called without a
  // preceding call to Open. The other method must call Open first.
  virtual bool UsageInformationSupport(bool* has_support);
  virtual bool UsageInformationSupport(SecurityLevel security_level,
                                       bool* has_support);
  virtual CdmResponseType UpdateUsageInformation();  // only for OEMCrypto v9-12
  virtual CdmResponseType DeactivateUsageInformation(
      const std::string& provider_session_token);
  virtual CdmResponseType GenerateUsageReport(
      const std::string& provider_session_token, std::string* usage_report,
      UsageDurationStatus* usage_duration_status,
      int64_t* seconds_since_started, int64_t* seconds_since_last_played);
  virtual CdmResponseType ReleaseUsageInformation(
      const std::string& message, const std::string& signature,
      const std::string& provider_session_token);
  // Delete a usage information for a single token.  This does not require
  // a signed message from the server.
  virtual CdmResponseType DeleteUsageInformation(
      const std::string& provider_session_token);
  // Delete usage information for a list of tokens.  This does not require
  // a signed message from the server.
  virtual CdmResponseType DeleteMultipleUsageInformation(
      const std::vector<std::string>& provider_session_tokens);
  virtual CdmResponseType DeleteAllUsageReports();
  virtual bool IsAntiRollbackHwPresent();

  // The overloaded methods with |security_level| may be called without a
  // preceding call to Open. The other methods must call Open first.
  virtual CdmResponseType GetHdcpCapabilities(HdcpCapability* current,
                                              HdcpCapability* max);
  virtual CdmResponseType GetHdcpCapabilities(SecurityLevel security_level,
                                              HdcpCapability* current,
                                              HdcpCapability* max);
  virtual bool GetResourceRatingTier(uint32_t* tier);
  virtual bool GetResourceRatingTier(SecurityLevel security_level,
                                     uint32_t* tier);

  virtual bool GetSupportedCertificateTypes(SupportedCertificateTypes* support);
  virtual CdmResponseType GetRandom(size_t data_length, uint8_t* random_data);
  virtual CdmResponseType GetNumberOfOpenSessions(SecurityLevel security_level,
                                                  size_t* count);
  virtual CdmResponseType GetMaxNumberOfSessions(SecurityLevel security_level,
                                                 size_t* max);

  virtual CdmResponseType GetSrmVersion(uint16_t* srm_version);
  virtual bool IsSrmUpdateSupported();
  virtual CdmResponseType LoadSrm(const std::string& srm);

  virtual bool GetBuildInformation(SecurityLevel security_level,
                                   std::string* info);
  virtual bool GetBuildInformation(std::string* info);

  virtual uint32_t IsDecryptHashSupported(SecurityLevel security_level);

  virtual CdmResponseType SetDecryptHash(uint32_t frame_number,
                                         const std::string& hash);

  virtual CdmResponseType GetDecryptHashError(std::string* error_string);

  virtual CdmResponseType GenericEncrypt(const std::string& in_buffer,
                                         const std::string& key_id,
                                         const std::string& iv,
                                         CdmEncryptionAlgorithm algorithm,
                                         std::string* out_buffer);
  virtual CdmResponseType GenericDecrypt(const std::string& in_buffer,
                                         const std::string& key_id,
                                         const std::string& iv,
                                         CdmEncryptionAlgorithm algorithm,
                                         std::string* out_buffer);
  virtual CdmResponseType GenericSign(const std::string& message,
                                      const std::string& key_id,
                                      CdmSigningAlgorithm algorithm,
                                      std::string* signature);
  virtual CdmResponseType GenericVerify(const std::string& message,
                                        const std::string& key_id,
                                        CdmSigningAlgorithm algorithm,
                                        const std::string& signature);

  // Usage table header and usage entry related methods
  virtual UsageTableHeader* GetUsageTableHeader() {
    return usage_table_header_;
  }
  virtual CdmResponseType GetUsageSupportType(CdmUsageSupportType* type);
  virtual CdmResponseType CreateUsageTableHeader(
      CdmUsageTableHeader* usage_table_header);
  virtual CdmResponseType LoadUsageTableHeader(
      const CdmUsageTableHeader& usage_table_header);
  virtual CdmResponseType CreateUsageEntry(uint32_t* entry_number);
  virtual CdmResponseType LoadUsageEntry(uint32_t entry_number,
                                         const CdmUsageEntry& usage_entry);
  virtual CdmResponseType UpdateUsageEntry(
      CdmUsageTableHeader* usage_table_header, CdmUsageEntry* usage_entry);
  virtual CdmResponseType ShrinkUsageTableHeader(
      uint32_t new_entry_count, CdmUsageTableHeader* usage_table_header);
  virtual CdmResponseType MoveUsageEntry(uint32_t new_entry_number);
  virtual bool CreateOldUsageEntry(uint64_t time_since_license_received,
                                   uint64_t time_since_first_decrypt,
                                   uint64_t time_since_last_decrypt,
                                   UsageDurationStatus status,
                                   const std::string& server_mac_key,
                                   const std::string& client_mac_key,
                                   const std::string& provider_session_token);
  virtual CdmResponseType CopyOldUsageEntry(
      const std::string& provider_session_token);
  virtual bool GetAnalogOutputCapabilities(bool* can_support_output,
                                           bool* can_disable_output,
                                           bool* can_support_cgms_a);
  virtual metrics::CryptoMetrics* GetCryptoMetrics() { return metrics_; }

  virtual CdmResponseType GetProvisioningMethod(
      SecurityLevel requested_security_level,
      CdmClientTokenType* token_type);

 protected:
  // Creates an instance of CryptoSession with the given |crypto_metrics|.
  // |crypto_metrics| is owned by the caller, must NOT be null, and must
  // exist as long as the new CryptoSession exists.
  explicit CryptoSession(metrics::CryptoMetrics* crypto_metrics);

  int session_count() { return session_count_; }

 private:
  friend class CryptoSessionForTest;
  friend class CryptoSessionFactory;
  friend class WvCdmTestBase;

  // The global factory method can be set to generate special crypto sessions
  // just for testing.  These sessions will avoid nonce floods and will ask
  // OEMCrypto to use a test keybox.
  // Ownership of the object is transfered to CryptoSession.
  static void SetCryptoSessionFactory(CryptoSessionFactory* factory) {
    std::unique_lock<std::mutex> auto_lock(factory_mutex_);
    factory_.reset(factory);
  }

  void Init();
  void Terminate();
  CdmResponseType GetTokenFromKeybox(std::string* token);
  CdmResponseType GetTokenFromOemCert(std::string* token);
  static bool ExtractSystemIdFromOemCert(const std::string& oem_cert,
                                         uint32_t* system_id);
  CdmResponseType GetSystemIdInternal(uint32_t* system_id);
  CdmResponseType GenerateSignature(
      const std::string& message, std::string* signature);
  CdmResponseType GenerateRsaSignature(const std::string& message,
                                       std::string* signature);

  bool SetDestinationBufferType();

  CdmResponseType RewrapDeviceRSAKey(const std::string& message,
                                     const std::string& signature,
                                     const std::string& nonce,
                                     const std::string& enc_rsa_key,
                                     const std::string& rsa_key_iv,
                                     std::string* wrapped_rsa_key);

  CdmResponseType RewrapDeviceRSAKey30(const std::string& message,
                                       const std::string& nonce,
                                       const std::string& private_key,
                                       const std::string& iv,
                                       const std::string& wrapping_key,
                                       std::string* wrapped_private_key);

  CdmResponseType SelectKey(const std::string& key_id,
                            CdmCipherMode cipher_mode);

  static const OEMCrypto_Algorithm kInvalidAlgorithm =
      static_cast<OEMCrypto_Algorithm>(-1);

  OEMCrypto_Algorithm GenericSigningAlgorithm(CdmSigningAlgorithm algorithm);
  OEMCrypto_Algorithm GenericEncryptionAlgorithm(
      CdmEncryptionAlgorithm algorithm);
  size_t GenericEncryptionBlockSize(CdmEncryptionAlgorithm algorithm);

  // These methods are used when a subsample exceeds the maximum buffer size
  // that the device can handle.
  OEMCryptoResult CopyBufferInChunks(
      const CdmDecryptionParameters& params,
      OEMCrypto_DestBufferDesc buffer_descriptor);
  OEMCryptoResult DecryptInChunks(
      const CdmDecryptionParameters& params,
      const OEMCrypto_DestBufferDesc& full_buffer_descriptor,
      const OEMCrypto_CENCEncryptPatternDesc& pattern_descriptor,
      size_t max_chunk_size);
  static void IncrementIV(uint64_t increase_by, std::vector<uint8_t>* iv_out);

  // These methods should be used to take the various CryptoSession mutexes in
  // preference to taking the mutexes directly.
  //
  // A lock should be taken on the Static Field Mutex before accessing any of
  // CryptoSession's non-atomic static fields. It can be taken as a reader or as
  // a writer, depending on how you will be accessing the static fields.
  //
  // Before calling into OEMCrypto, code must take locks on the OEMCrypto Mutex
  // and/or the OEMCrypto Session Mutex. Which of them should be taken and how
  // depends on the OEMCrypto function being called; consult the OEMCrypto
  // specification's threading guarantees before making any calls. The OEMCrypto
  // specification defines several classes of functions for the purposes of
  // parallelism. The methods below lock the OEMCrypto Mutex and OEMCrypto
  // Session Mutex in the correct order and manner to fulfill the guarantees in
  // the specification.
  //
  // For this function class...    | ...use this locking method
  // ------------------------------+---------------------------
  // Initialization & Termination  |         WithOecWriteLock()
  // Property                      |          WithOecReadLock()
  // Session Initialization        |         WithOecWriteLock()
  // Usage Table                   |         WithOecWriteLock()
  // Session                       |       WithOecSessionLock()
  //
  // Note that accessing |key_session_| often accesses the OEMCrypto session, so
  // WithOecSessionLock() should be used before accessing |key_session_| as
  // well.
  //
  // If a function needs to take a lock on both the Static Field Mutex and some
  // of the OEMCrypto mutexes simultaneously, it must *always* lock the Static
  // Field Mutex before the OEMCrypto mutexes.
  //
  // In general, all locks should only be held for the minimum time necessary
  // (e.g. a lock on the OEMCrypto mutexes should only be held for the duration
  // of a single call into OEMCrypto) unless there is a compelling argument
  // otherwise, such as making two calls into OEMCrypto immediately after each
  // other.
  template <class Func>
  static auto WithStaticFieldWriteLock(const char* tag, Func body)
      -> decltype(body());

  template <class Func>
  static auto WithStaticFieldReadLock(const char* tag, Func body)
      -> decltype(body());

  template <class Func>
  static auto WithOecWriteLock(const char* tag, Func body) -> decltype(body());

  template <class Func>
  static auto WithOecReadLock(const char* tag, Func body) -> decltype(body());

  template <class Func>
  auto WithOecSessionLock(const char* tag, Func body) -> decltype(body());

  static bool IsInitialized();

  // Constants
  static const size_t kAes128BlockSize = 16;  // Block size for AES_CBC_128
  static const size_t kSignatureSize = 32;    // size for HMAC-SHA256 signature

  // The locking methods above should be used in preference to taking these
  // mutexes directly. If code takes these manually and needs to take more
  // than one, it must *always* take them in the order they are defined here.
  static shared_mutex static_field_mutex_;
  static shared_mutex oem_crypto_mutex_;
  std::mutex oem_crypto_session_mutex_;

  static bool initialized_;
  static int session_count_;

  metrics::CryptoMetrics* metrics_;
  metrics::TimerMetric life_span_;
  uint32_t system_id_;

  bool open_;
  CdmClientTokenType pre_provision_token_type_;
  std::string oem_token_;  // Cached OEMCrypto Public Key
  bool update_usage_table_after_close_session_;
  CryptoSessionId oec_session_id_;
  std::unique_ptr<KeySession> key_session_;

  OEMCryptoBufferType destination_buffer_type_;
  bool is_destination_buffer_type_valid_;
  SecurityLevel requested_security_level_;

  bool is_usage_support_type_valid_;
  CdmUsageSupportType usage_support_type_;
  UsageTableHeader* usage_table_header_;
  static UsageTableHeader* usage_table_header_l1_;
  static UsageTableHeader* usage_table_header_l3_;

  std::string request_id_;
  static std::atomic<uint64_t> request_id_index_source_;

  CdmCipherMode cipher_mode_;
  uint32_t api_version_;

  // In order to avoid creating a deadlock if instantiation needs to take any
  // of the CryptoSession static mutexes, |factory_| is protected by its own
  // mutex that is only used in the two funtions that interact with it.
  static std::mutex factory_mutex_;
  static std::unique_ptr<CryptoSessionFactory> factory_;

  CORE_DISALLOW_COPY_AND_ASSIGN(CryptoSession);
};

class CryptoSessionFactory {
 public:
  virtual ~CryptoSessionFactory() {}
  virtual CryptoSession* MakeCryptoSession(
      metrics::CryptoMetrics* crypto_metrics);

 protected:
  friend class CryptoSession;
  CryptoSessionFactory() {}

 private:
  CORE_DISALLOW_COPY_AND_ASSIGN(CryptoSessionFactory);
};

}  // namespace wvcdm

#endif  // WVCDM_CORE_CRYPTO_SESSION_H_
