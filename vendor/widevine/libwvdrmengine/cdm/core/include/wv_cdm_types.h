// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#ifndef WVCDM_CORE_WV_CDM_TYPES_H_
#define WVCDM_CORE_WV_CDM_TYPES_H_

#include <array>
#include <stdint.h>
#include <map>
#include <string>
#include <vector>

namespace wvcdm {

typedef std::string CdmKeySystem;
typedef std::string CdmInitData;
typedef std::string CdmKeyMessage;
typedef std::string CdmKeyResponse;
typedef std::string KeyId;
typedef std::string CdmSecureStopId;
typedef std::string CdmSessionId;
typedef std::string CdmKeySetId;
typedef std::string RequestId;
typedef uint32_t CryptoResult;
typedef uint32_t CryptoSessionId;
typedef std::map<std::string, std::string> CdmAppParameterMap;
typedef std::map<std::string, std::string> CdmQueryMap;
typedef std::vector<std::string> CdmUsageInfo;
typedef std::string CdmUsageInfoReleaseMessage;
typedef std::string CdmProvisioningRequest;
typedef std::string CdmProvisioningResponse;
typedef std::string CdmUsageTableHeader;
typedef std::string CdmUsageEntry;

enum CdmKeyRequestType {
  kKeyRequestTypeUnknown,
  kKeyRequestTypeInitial,
  kKeyRequestTypeRenewal,
  kKeyRequestTypeRelease,
  kKeyRequestTypeNone,  // Keys are loaded and no license request is necessary
  kKeyRequestTypeUpdate,
};

enum CdmOfflineLicenseState {
  kLicenseStateActive,
  kLicenseStateReleasing,
  kLicenseStateUnknown,
};

enum CdmResponseType {
  NO_ERROR = 0,
  UNKNOWN_ERROR = 1,
  KEY_ADDED = 2,
  KEY_ERROR = 3,
  KEY_MESSAGE = 4,
  NEED_KEY = 5,
  KEY_CANCELED = 6,
  NEED_PROVISIONING = 7,
  DEVICE_REVOKED = 8,
  INSUFFICIENT_CRYPTO_RESOURCES = 9,
  ADD_KEY_ERROR = 10,
  CERT_PROVISIONING_GET_KEYBOX_ERROR_1 = 11,
  /* previously CERT_PROVISIONING_GET_KEYBOX_ERROR_2 = 12 */
  CERT_PROVISIONING_INVALID_CERT_TYPE = 13,
  CERT_PROVISIONING_REQUEST_ERROR_1 = 14,
  CERT_PROVISIONING_NONCE_GENERATION_ERROR = 15,
  /* previously CERT_PROVISIONING_REQUEST_ERROR_3 = 16 */
  CERT_PROVISIONING_REQUEST_ERROR_4 = 17,
  CERT_PROVISIONING_RESPONSE_ERROR_1 = 18,
  CERT_PROVISIONING_RESPONSE_ERROR_2 = 19,
  CERT_PROVISIONING_RESPONSE_ERROR_3 = 20,
  CERT_PROVISIONING_RESPONSE_ERROR_4 = 21,
  CERT_PROVISIONING_RESPONSE_ERROR_5 = 22,
  CERT_PROVISIONING_RESPONSE_ERROR_6 = 23,
  CERT_PROVISIONING_RESPONSE_ERROR_7 = 24,
  CERT_PROVISIONING_RESPONSE_ERROR_8 = 25,
  /* previously CRYPTO_SESSION_OPEN_ERROR_1 = 26 */
  /* previously CRYPTO_SESSION_OPEN_ERROR_2 = 27 */
  /* previously CRYPTO_SESSION_OPEN_ERROR_3 = 28 */
  /* previously CRYPTO_SESSION_OPEN_ERROR_4 = 29 */
  /* previously CRYPTO_SESSION_OPEN_ERROR_5 = 30 */
  DECRYPT_NOT_READY = 31,
  DEVICE_CERTIFICATE_ERROR_1 = 32,
  DEVICE_CERTIFICATE_ERROR_2 = 33,
  DEVICE_CERTIFICATE_ERROR_3 = 34,
  DEVICE_CERTIFICATE_ERROR_4 = 35,
  EMPTY_KEY_DATA_1 = 36,
  EMPTY_KEY_DATA_2 = 37,
  EMPTY_KEYSET_ID = 38,
  EMPTY_KEYSET_ID_ENG_1 = 39,
  EMPTY_KEYSET_ID_ENG_2 = 40,
  EMPTY_KEYSET_ID_ENG_3 = 41,
  EMPTY_KEYSET_ID_ENG_4 = 42,
  EMPTY_LICENSE_RENEWAL = 43,
  EMPTY_LICENSE_RESPONSE_1 = 44,
  EMPTY_LICENSE_RESPONSE_2 = 45,
  EMPTY_PROVISIONING_CERTIFICATE_1 = 46,
  EMPTY_PROVISIONING_RESPONSE = 47,
  EMPTY_SESSION_ID = 48,
  GENERATE_DERIVED_KEYS_ERROR = 49,
  LICENSE_RENEWAL_NONCE_GENERATION_ERROR = 50,
  GENERATE_USAGE_REPORT_ERROR = 51,
  GET_LICENSE_ERROR = 52,
  GET_RELEASED_LICENSE_ERROR = 53,
  GET_USAGE_INFO_ERROR_1 = 54,
  GET_USAGE_INFO_ERROR_2 = 55,
  GET_USAGE_INFO_ERROR_3 = 56,
  GET_USAGE_INFO_ERROR_4 = 57,
  INIT_DATA_NOT_FOUND = 58,
  /* previously INVALID_CRYPTO_SESSION_1 = 59 */
  /* previously INVALID_CRYPTO_SESSION_2 = 60 */
  /* previously INVALID_CRYPTO_SESSION_3 = 61 */
  /* previously INVALID_CRYPTO_SESSION_4 = 62 */
  /* previously INVALID_CRYPTO_SESSION_5 = 63 */
  INVALID_DECRYPT_PARAMETERS_ENG_1 = 64,
  INVALID_DECRYPT_PARAMETERS_ENG_2 = 65,
  INVALID_DECRYPT_PARAMETERS_ENG_3 = 66,
  INVALID_DECRYPT_PARAMETERS_ENG_4 = 67,
  INVALID_DEVICE_CERTIFICATE_TYPE = 68,
  INVALID_KEY_SYSTEM = 69,
  INVALID_LICENSE_RESPONSE = 70,
  INVALID_LICENSE_TYPE = 71,
  PARAMETER_NULL = 72, /* prior to pi, INVALID_PARAMETERS_ENG_1 = 72 */
  /* previously INVALID_PARAMETERS_ENG_2 = 73 */
  /* previously INVALID_PARAMETERS_ENG_3 = 74 */
  /* previously INVALID_PARAMETERS_ENG_4 = 75 */
  INVALID_PARAMETERS_LIC_1 = 76,
  INVALID_PARAMETERS_LIC_2 = 77,
  INVALID_PROVISIONING_PARAMETERS_1 = 78,
  INVALID_PROVISIONING_PARAMETERS_2 = 79,
  INVALID_PROVISIONING_REQUEST_PARAM_1 = 80,
  INVALID_PROVISIONING_REQUEST_PARAM_2 = 81,
  INVALID_QUERY_KEY = 82,
  INVALID_SESSION_ID = 83,
  KEY_REQUEST_ERROR_1 = 84,
  /* previously KEY_REQUEST_ERROR_2 = 85 */
  KEY_SIZE_ERROR_1 = 86,
  KEYSET_ID_NOT_FOUND_1 = 87,
  KEYSET_ID_NOT_FOUND_2 = 88,
  KEYSET_ID_NOT_FOUND_3 = 89,
  LICENSE_ID_NOT_FOUND = 90,
  LICENSE_PARSER_INIT_ERROR = 91,
  LICENSE_PARSER_NOT_INITIALIZED_1 = 92,
  LICENSE_PARSER_NOT_INITIALIZED_2 = 93,
  LICENSE_PARSER_NOT_INITIALIZED_3 = 94,
  LICENSE_RESPONSE_NOT_SIGNED = 95,
  LICENSE_RESPONSE_PARSE_ERROR_1 = 96,
  LICENSE_RESPONSE_PARSE_ERROR_2 = 97,
  LICENSE_RESPONSE_PARSE_ERROR_3 = 98,
  LOAD_KEY_ERROR = 99,
  NO_CONTENT_KEY = 100,
  REFRESH_KEYS_ERROR = 101,
  REMOVE_ALL_USAGE_INFO_ERROR_1 = 102,
  REMOVE_ALL_USAGE_INFO_ERROR_2 = 103,
  RELEASE_KEY_ERROR = 104,
  RELEASE_KEY_REQUEST_ERROR = 105,
  RELEASE_LICENSE_ERROR_1 = 106,
  RELEASE_LICENSE_ERROR_2 = 107,
  RELEASE_USAGE_INFO_ERROR = 108,
  RENEW_KEY_ERROR_1 = 109,
  RENEW_KEY_ERROR_2 = 110,
  /* previously LICENSE_RENEWAL_SIGNING_ERROR = 111 */
  /* previously RESTORE_OFFLINE_LICENSE_ERROR_1 = 112 */
  RESTORE_OFFLINE_LICENSE_ERROR_2 = 113,
  NOT_INITIALIZED_ERROR = 114, /* prior to pi, SESSION_INIT_ERROR_1 = 114 */
  REINIT_ERROR = 115,          /* prior to pi, SESSION_INIT_ERROR_2 = 115 */
  /* previously SESSION_INIT_GET_KEYBOX_ERROR = 116 */
  SESSION_NOT_FOUND_1 = 117,
  SESSION_NOT_FOUND_2 = 118,
  SESSION_NOT_FOUND_3 = 119,
  SESSION_NOT_FOUND_4 = 120,
  SESSION_NOT_FOUND_5 = 121,
  SESSION_NOT_FOUND_6 = 122,
  SESSION_NOT_FOUND_7 = 123,
  SESSION_NOT_FOUND_8 = 124,
  SESSION_NOT_FOUND_9 = 125,
  SESSION_NOT_FOUND_10 = 126,
  SESSION_NOT_FOUND_FOR_DECRYPT = 127,
  SESSION_KEYS_NOT_FOUND = 128,
  SIGNATURE_NOT_FOUND = 129,
  STORE_LICENSE_ERROR_1 = 130,
  STORE_LICENSE_ERROR_2 = 131,
  /* previously STORE_LICENSE_ERROR_3 = 132 */
  STORE_USAGE_INFO_ERROR = 133,
  UNPROVISION_ERROR_1 = 134,
  UNPROVISION_ERROR_2 = 135,
  UNPROVISION_ERROR_3 = 136,
  UNPROVISION_ERROR_4 = 137,
  UNSUPPORTED_INIT_DATA = 138,
  USAGE_INFO_NOT_FOUND = 139,
  /* previously LICENSE_RENEWAL_SERVICE_CERTIFICATE_GENERATION_ERROR = 140 */
  PARSE_SERVICE_CERTIFICATE_ERROR = 141,
  /* previously SERVICE_CERTIFICATE_TYPE_ERROR = 142 */
  CLIENT_ID_GENERATE_RANDOM_ERROR = 143,
  CLIENT_ID_AES_INIT_ERROR = 144,
  CLIENT_ID_AES_ENCRYPT_ERROR = 145,
  CLIENT_ID_RSA_INIT_ERROR = 146,
  CLIENT_ID_RSA_ENCRYPT_ERROR = 147,
  /* previously INVALID_QUERY_STATUS = 148 */
  /* previously EMPTY_PROVISIONING_CERTIFICATE_2 = 149 on mnc-dev, */
  /* and DUPLICATE_SESSION_ID_SPECIFIED = 149 on master */
  LICENSE_PARSER_NOT_INITIALIZED_4 = 150,
  INVALID_PARAMETERS_LIC_3 = 151,
  INVALID_PARAMETERS_LIC_4 = 152,
  /* previously INVALID_PARAMETERS_LIC_5 = 153 */
  INVALID_PARAMETERS_LIC_6 = 154,
  INVALID_PARAMETERS_LIC_7 = 155,
  LICENSE_REQUEST_SERVICE_CERTIFICATE_GENERATION_ERROR = 156,
  CENC_INIT_DATA_UNAVAILABLE = 157,
  PREPARE_CENC_CONTENT_ID_FAILED = 158,
  WEBM_INIT_DATA_UNAVAILABLE = 159,
  PREPARE_WEBM_CONTENT_ID_FAILED = 160,
  UNSUPPORTED_INIT_DATA_FORMAT = 161,
  LICENSE_REQUEST_NONCE_GENERATION_ERROR = 162,
  /* previously LICENSE_REQUEST_SIGNING_ERROR = 163, */
  EMPTY_LICENSE_REQUEST = 164,
  SECURE_BUFFER_REQUIRED = 165,
  DUPLICATE_SESSION_ID_SPECIFIED = 166,
  LICENSE_RENEWAL_PROHIBITED = 167,
  EMPTY_PROVISIONING_CERTIFICATE_2 = 168,
  OFFLINE_LICENSE_PROHIBITED = 169,
  STORAGE_PROHIBITED = 170,
  EMPTY_KEYSET_ID_ENG_5 = 171,
  SESSION_NOT_FOUND_11 = 172,
  LOAD_USAGE_INFO_FILE_ERROR = 173,
  LOAD_USAGE_INFO_MISSING = 174,
  SESSION_FILE_HANDLE_INIT_ERROR = 175,
  INCORRECT_CRYPTO_MODE = 176,
  INVALID_PARAMETERS_ENG_5 = 177,
  DECRYPT_ERROR = 178,
  INSUFFICIENT_OUTPUT_PROTECTION = 179,
  SESSION_NOT_FOUND_12 = 180,
  KEY_NOT_FOUND_1 = 181,
  KEY_NOT_FOUND_2 = 182,
  KEY_CONFLICT_1 = 183,
  /* previously INVALID_PARAMETERS_ENG_6 = 184 */
  /* previously INVALID_PARAMETERS_ENG_7 = 185 */
  /* previously INVALID_PARAMETERS_ENG_8 = 186 */
  /* previously INVALID_PARAMETERS_ENG_9 = 187 */
  /* previously INVALID_PARAMETERS_ENG_10 = 188 */
  /* previously INVALID_PARAMETERS_ENG_11 = 189 */
  /* previously INVALID_PARAMETERS_ENG_12 = 190 */
  SESSION_NOT_FOUND_13 = 191,
  SESSION_NOT_FOUND_14 = 192,
  SESSION_NOT_FOUND_15 = 193,
  SESSION_NOT_FOUND_16 = 194,
  KEY_NOT_FOUND_3 = 195,
  KEY_NOT_FOUND_4 = 196,
  KEY_NOT_FOUND_5 = 197,
  KEY_NOT_FOUND_6 = 198,
  INVALID_SESSION_1 = 199,
  NO_DEVICE_KEY_1 = 200,
  NO_CONTENT_KEY_2 = 201,
  INSUFFICIENT_CRYPTO_RESOURCES_2 = 202,
  INVALID_PARAMETERS_ENG_13 = 203,
  INVALID_PARAMETERS_ENG_14 = 204,
  INVALID_PARAMETERS_ENG_15 = 205,
  INVALID_PARAMETERS_ENG_16 = 206,
  /* previously DEVICE_CERTIFICATE_ERROR_5 = 207 */
  CLIENT_IDENTIFICATION_TOKEN_ERROR_1 = 208,
  /* previously CLIENT_IDENTIFICATION_TOKEN_ERROR_2 = 209 */
  /* previously LICENSING_CLIENT_TOKEN_ERROR_1 = 210 */
  ANALOG_OUTPUT_ERROR = 211,
  UNKNOWN_SELECT_KEY_ERROR_1 = 212,
  UNKNOWN_SELECT_KEY_ERROR_2 = 213,
  CREATE_USAGE_TABLE_ERROR = 214,
  LOAD_USAGE_HEADER_GENERATION_SKEW = 215,
  LOAD_USAGE_HEADER_SIGNATURE_FAILURE = 216,
  LOAD_USAGE_HEADER_BAD_MAGIC = 217,
  LOAD_USAGE_HEADER_UNKNOWN_ERROR = 218,
  INVALID_PARAMETERS_ENG_17 = 219,
  INVALID_PARAMETERS_ENG_18 = 220,
  INSUFFICIENT_CRYPTO_RESOURCES_3 = 221,
  CREATE_USAGE_ENTRY_UNKNOWN_ERROR = 222,
  LOAD_USAGE_ENTRY_GENERATION_SKEW = 223,
  LOAD_USAGE_ENTRY_SIGNATURE_FAILURE = 224,
  LOAD_USAGE_ENTRY_UNKNOWN_ERROR = 225,
  INVALID_PARAMETERS_ENG_19 = 226,
  INVALID_PARAMETERS_ENG_20 = 227,
  UPDATE_USAGE_ENTRY_UNKNOWN_ERROR = 228,
  INVALID_PARAMETERS_ENG_21 = 229,
  SHRINK_USAGE_TABLER_HEADER_UNKNOWN_ERROR = 230,
  MOVE_USAGE_ENTRY_UNKNOWN_ERROR = 231,
  COPY_OLD_USAGE_ENTRY_UNKNOWN_ERROR = 232,
  INVALID_PARAMETERS_ENG_22 = 233,
  LIST_LICENSE_ERROR_1 = 234,
  LIST_LICENSE_ERROR_2 = 235,
  INVALID_PARAMETERS_ENG_23 = 236,
  USAGE_INFORMATION_SUPPORT_FAILED = 237,
  USAGE_SUPPORT_GET_API_FAILED = 238,
  UNEXPECTED_EMPTY_USAGE_ENTRY = 239,
  INVALID_USAGE_ENTRY_NUMBER_MODIFICATION = 240,
  USAGE_INVALID_NEW_ENTRY = 241,
  USAGE_INVALID_PARAMETERS_1 = 242,
  USAGE_GET_ENTRY_RETRIEVE_LICENSE_FAILED = 243,
  USAGE_GET_ENTRY_RETRIEVE_USAGE_INFO_FAILED = 244,
  USAGE_GET_ENTRY_RETRIEVE_INVALID_STORAGE_TYPE = 245,
  USAGE_ENTRY_NUMBER_MISMATCH = 246,
  USAGE_STORE_LICENSE_FAILED = 247,
  USAGE_STORE_USAGE_INFO_FAILED = 248,
  USAGE_INVALID_LOAD_ENTRY = 249,
  /* previously REMOVE_ALL_USAGE_INFO_ERROR_4 = 250, */
  REMOVE_ALL_USAGE_INFO_ERROR_5 = 251,
  RELEASE_USAGE_INFO_FAILED = 252,
  INCORRECT_USAGE_SUPPORT_TYPE_1 = 253,
  INCORRECT_USAGE_SUPPORT_TYPE_2 = 254,
  KEY_PROHIBITED_FOR_SECURITY_LEVEL = 255,
  KEY_NOT_FOUND_IN_SESSION = 256,
  NO_USAGE_ENTRIES = 257,
  LIST_USAGE_ERROR_1 = 258,
  LIST_USAGE_ERROR_2 = 259,
  DELETE_USAGE_ERROR_1 = 260,
  DELETE_USAGE_ERROR_2 = 261,
  DELETE_USAGE_ERROR_3 = 262,
  PRIVACY_MODE_ERROR_1 = 263,
  PRIVACY_MODE_ERROR_2 = 264,
  PRIVACY_MODE_ERROR_3 = 265,
  EMPTY_RESPONSE_ERROR_1 = 266,
  INVALID_PARAMETERS_ENG_24 = 267,
  PARSE_RESPONSE_ERROR_1 = 268,
  PARSE_RESPONSE_ERROR_2 = 269,
  PARSE_RESPONSE_ERROR_3 = 270,
  PARSE_RESPONSE_ERROR_4 = 271,
  USAGE_STORE_ENTRY_RETRIEVE_LICENSE_FAILED = 272,
  USAGE_STORE_ENTRY_RETRIEVE_USAGE_INFO_FAILED = 273,
  USAGE_STORE_ENTRY_RETRIEVE_INVALID_STORAGE_TYPE = 274,
  REMOVE_ALL_USAGE_INFO_ERROR_6 = 275,
  REMOVE_ALL_USAGE_INFO_ERROR_7 = 276,
  /* previously LICENSE_REQUEST_INVALID_SUBLICENSE = 277, */
  CERT_PROVISIONING_EMPTY_SERVICE_CERTIFICATE = 278,
  LOAD_SYSTEM_ID_ERROR = 279,
  INSUFFICIENT_CRYPTO_RESOURCES_4 = 280,
  INSUFFICIENT_CRYPTO_RESOURCES_5 = 281,
  REMOVE_USAGE_INFO_ERROR_1 = 282,
  REMOVE_USAGE_INFO_ERROR_2 = 283,
  REMOVE_USAGE_INFO_ERROR_3 = 284,
  INSUFFICIENT_CRYPTO_RESOURCES_6 = 285,
  NOT_AN_ENTITLEMENT_SESSION = 286,
  NO_MATCHING_ENTITLEMENT_KEY = 287,
  LOAD_ENTITLED_CONTENT_KEYS_ERROR = 288,
  GET_PROVISIONING_METHOD_ERROR = 289,
  SESSION_NOT_FOUND_17 = 290,
  SESSION_NOT_FOUND_18 = 291,
  NO_CONTENT_KEY_3 = 292,
  DEVICE_CANNOT_REPROVISION = 293,
  SESSION_NOT_FOUND_19 = 294,
  KEY_SIZE_ERROR_2 = 295,
  SET_DECRYPT_HASH_ERROR = 296,
  GET_DECRYPT_HASH_ERROR = 297,
  SESSION_NOT_FOUND_20 = 298,
  INVALID_DECRYPT_HASH_FORMAT = 299,
  EMPTY_LICENSE_REQUEST_2 = 300,
  EMPTY_LICENSE_REQUEST_3 = 301,
  EMPTY_LICENSE_RESPONSE_3 = 302,
  EMPTY_LICENSE_RESPONSE_4 = 303,
  PARSE_REQUEST_ERROR_1 = 304,
  PARSE_REQUEST_ERROR_2 = 305,
  INVALID_LICENSE_REQUEST_TYPE_1 = 306,
  INVALID_LICENSE_REQUEST_TYPE_2 = 307,
  LICENSE_RESPONSE_PARSE_ERROR_4 = 308,
  LICENSE_RESPONSE_PARSE_ERROR_5 = 309,
  INVALID_LICENSE_TYPE_2 = 310,
  SIGNATURE_NOT_FOUND_2 = 311,
  SESSION_KEYS_NOT_FOUND_2 = 312,
  GET_OFFLINE_LICENSE_STATE_ERROR_1 = 313,
  GET_OFFLINE_LICENSE_STATE_ERROR_2 = 314,
  REMOVE_OFFLINE_LICENSE_ERROR_1 = 315,
  REMOVE_OFFLINE_LICENSE_ERROR_2 = 316,
  SESSION_NOT_FOUND_21 = 317,
  OUTPUT_TOO_LARGE_ERROR = 318,
  SESSION_LOST_STATE_ERROR = 319,
  GENERATE_DERIVED_KEYS_ERROR_2 = 320,
  LOAD_DEVICE_RSA_KEY_ERROR = 321,
  NONCE_GENERATION_ERROR = 322,
  GENERATE_SIGNATURE_ERROR = 323,
  UNKNOWN_CLIENT_TOKEN_TYPE = 324,
  DEACTIVATE_USAGE_ENTRY_ERROR = 325,
  SERVICE_CERTIFICATE_PROVIDER_ID_EMPTY = 326,
  SYSTEM_INVALIDATED_ERROR = 327,
  OPEN_CRYPTO_SESSION_ERROR = 328,
  LOAD_SRM_ERROR = 329,
  RANDOM_GENERATION_ERROR = 330,
  CRYPTO_SESSION_NOT_INITIALIZED = 331,
  GET_DEVICE_ID_ERROR = 332,
  GET_TOKEN_FROM_OEM_CERT_ERROR = 333,
  CRYPTO_SESSION_NOT_OPEN = 334,
  GET_TOKEN_FROM_KEYBOX_ERROR = 335,
  KEYBOX_TOKEN_TOO_SHORT = 336,
  EXTRACT_SYSTEM_ID_FROM_OEM_CERT_ERROR = 337,
  RSA_SIGNATURE_GENERATION_ERROR = 338,
  GET_HDCP_CAPABILITY_FAILED = 339,
  GET_NUMBER_OF_OPEN_SESSIONS_ERROR = 340,
  GET_MAX_NUMBER_OF_OPEN_SESSIONS_ERROR = 341,
  NOT_IMPLEMENTED_ERROR = 342,
  GET_SRM_VERSION_ERROR = 343,
  REWRAP_DEVICE_RSA_KEY_ERROR = 344,
  REWRAP_DEVICE_RSA_KEY_30_ERROR = 345,
  INVALID_SRM_LIST = 346,
  KEYSET_ID_NOT_FOUND_4 = 347,
  // Don't forget to add new values to ../test/test_printers.cpp.
};

enum CdmKeyStatus {
  kKeyStatusKeyUnknown,
  kKeyStatusUsable,
  kKeyStatusExpired,
  kKeyStatusOutputNotAllowed,
  kKeyStatusPending,
  kKeyStatusInternalError,
  kKeyStatusUsableInFuture,
};
typedef std::map<KeyId, CdmKeyStatus> CdmKeyStatusMap;

enum CdmLicenseType {
  kLicenseTypeOffline,
  kLicenseTypeStreaming,
  kLicenseTypeRelease,
  // Like Streaming, but stricter.  Does not permit storage of any kind.
  // Named after the 'temporary' session type in EME, which has this behavior.
  kLicenseTypeTemporary,
  // TODO(jfore): The kLicenseTypeEmbeddedKeyData currently is to differentiate
  // between call types made to GenerateKeyRequest. This type is used to
  // differentiate between calls to generate a license renewal and a new pssh
  // with embedded keys. Please refer to CdmSession::GenerateKeyRequest. Based
  // on code review comments from go/wvgerrit/41860 this license type should not
  // be added. This type can be removed once it is no longer needed by
  // GenerateKeyRequest.
  kLicenseTypeEmbeddedKeyData
};

enum CdmLicenseKeyType { kLicenseKeyTypeContent, kLicenseKeyTypeEntitlement };

enum SecurityLevel { kLevelDefault, kLevel3 };

enum CdmSecurityLevel {
  kSecurityLevelUninitialized,
  kSecurityLevelL1,
  kSecurityLevelL2,
  kSecurityLevelL3,
  kSecurityLevelUnknown
};

enum CdmCertificateType {
  kCertificateWidevine,
  kCertificateX509,
};

enum CdmHlsMethod {
  kHlsMethodNone,
  kHlsMethodAes128,
  kHlsMethodSampleAes,
};

enum CdmCipherMode {
  kCipherModeCtr,
  kCipherModeCbc,
};

enum CdmEncryptionAlgorithm {
  kEncryptionAlgorithmUnknown,
  kEncryptionAlgorithmAesCbc128
};

enum CdmSigningAlgorithm {
  kSigningAlgorithmUnknown,
  kSigningAlgorithmHmacSha256
};

enum CdmClientTokenType {
  kClientTokenKeybox,
  kClientTokenDrmCert,
  kClientTokenOemCert,
  kClientTokenUninitialized,
};

// kNonSecureUsageSupport - TEE does not provide any support for usage
//     information.
// kUsageTableSupport - TEE persists usage information securely in a fixed
//     size table, commonly 50 entries. (OEMCrypto v9+)
// kUsageEntrySupport - usage information (table headers and entries) are
//     persisted in non-secure storage but are loaded and unloaded from
//     the TEE during use (OEMCrypto v13+)
// kUnknownUsageSupport - usage support type is uninitialized or unavailable
enum CdmUsageSupportType {
  kNonSecureUsageSupport,
  kUsageTableSupport,
  kUsageEntrySupport,
  kUnknownUsageSupport,
};

enum CdmUsageEntryStorageType {
  kStorageLicense,
  kStorageUsageInfo,
  kStorageTypeUnknown,
};

struct CdmUsageEntryInfo {
  CdmUsageEntryStorageType storage_type;
  CdmKeySetId key_set_id;
  std::string usage_info_file_name;
  bool operator==(const CdmUsageEntryInfo& other) const {
    return storage_type == other.storage_type &&
           key_set_id == other.key_set_id &&
           (storage_type != kStorageUsageInfo ||
            usage_info_file_name == other.usage_info_file_name);
  }
};

enum CdmKeySecurityLevel {
  kKeySecurityLevelUnset,
  kSoftwareSecureCrypto,
  kSoftwareSecureDecode,
  kHardwareSecureCrypto,
  kHardwareSecureDecode,
  kHardwareSecureAll,
  kKeySecurityLevelUnknown,
};

class CdmKeyAllowedUsage {
 public:
  CdmKeyAllowedUsage() { Clear(); }

  bool Valid() const { return valid_; }
  void SetValid() { valid_ = true; }

  void Clear() {
    decrypt_to_clear_buffer = false;
    decrypt_to_secure_buffer = false;
    generic_encrypt = false;
    generic_decrypt = false;
    generic_sign = false;
    generic_verify = false;
    key_security_level_ = kKeySecurityLevelUnset;
    valid_ = false;
  }

  bool Equals(const CdmKeyAllowedUsage& other) {
    if (!valid_ || !other.Valid() ||
        decrypt_to_clear_buffer != other.decrypt_to_clear_buffer ||
        decrypt_to_secure_buffer != other.decrypt_to_secure_buffer ||
        generic_encrypt != other.generic_encrypt ||
        generic_decrypt != other.generic_decrypt ||
        generic_sign != other.generic_sign ||
        generic_verify != other.generic_verify ||
        key_security_level_ != other.key_security_level_) {
      return false;
    }
    return true;
  }

  bool decrypt_to_clear_buffer;
  bool decrypt_to_secure_buffer;
  bool generic_encrypt;
  bool generic_decrypt;
  bool generic_sign;
  bool generic_verify;
  CdmKeySecurityLevel key_security_level_;

 private:
  bool valid_;
};

// For schemes that do not use pattern encryption (cenc and cbc1), encrypt
// and skip should be set to 0. For those that do (cens and cbcs), it is
// recommended that encrypt+skip bytes sum to 10 and for cbcs that a 1:9
// encrypt:skip ratio be used. See ISO/IEC DIS 23001-7, section 10.4.2 for
// more information.
struct CdmCencPatternEncryptionDescriptor {
  size_t encrypt_blocks;  // number of 16 byte blocks to decrypt
  size_t skip_blocks;     // number of 16 byte blocks to leave in clear
  CdmCencPatternEncryptionDescriptor() : encrypt_blocks(0), skip_blocks(0) {}
};

struct CdmDecryptionParameters {
  bool is_encrypted;
  bool is_secure;
  CdmCipherMode cipher_mode;
  const KeyId* key_id;
  const uint8_t* encrypt_buffer;
  size_t encrypt_length;
  const std::vector<uint8_t>* iv;
  size_t block_offset;
  void* decrypt_buffer;
  size_t decrypt_buffer_length;
  size_t decrypt_buffer_offset;
  uint8_t subsample_flags;
  bool is_video;
  CdmCencPatternEncryptionDescriptor pattern_descriptor;
  CdmDecryptionParameters()
      : is_encrypted(true),
        is_secure(true),
        cipher_mode(kCipherModeCtr),
        key_id(NULL),
        encrypt_buffer(NULL),
        encrypt_length(0),
        iv(NULL),
        block_offset(0),
        decrypt_buffer(NULL),
        decrypt_buffer_length(0),
        decrypt_buffer_offset(0),
        subsample_flags(0),
        is_video(true) {}
  CdmDecryptionParameters(const KeyId* key, const uint8_t* encrypted_buffer,
                          size_t encrypted_length,
                          const std::vector<uint8_t>* initialization_vector,
                          size_t offset, void* decrypted_buffer)
      : is_encrypted(true),
        is_secure(true),
        cipher_mode(kCipherModeCtr),
        key_id(key),
        encrypt_buffer(encrypted_buffer),
        encrypt_length(encrypted_length),
        iv(initialization_vector),
        block_offset(offset),
        decrypt_buffer(decrypted_buffer),
        decrypt_buffer_length(encrypted_length),
        decrypt_buffer_offset(0),
        subsample_flags(0),
        is_video(true) {}
};

struct CdmKeyRequest {
  CdmKeyMessage message;
  CdmKeyRequestType type;
  std::string url;
};

// forward class references
class KeyMessage;
class Request;
class Key;

}  // namespace wvcdm

#endif  // WVCDM_CORE_WV_CDM_TYPES_H_
