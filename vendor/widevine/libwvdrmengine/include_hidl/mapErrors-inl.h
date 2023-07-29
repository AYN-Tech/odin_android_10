//
// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//

#ifndef WV_MAP_ERRORS_H_
#define WV_MAP_ERRORS_H_

#include "media/stagefright/MediaErrors.h"
#include "utils/Errors.h"
#include "wv_cdm_types.h"
#include "HidlTypes.h"
#include "WVErrors.h"

namespace wvdrm {

static Status mapCdmResponseType(wvcdm::CdmResponseType res) {
  switch (res) {
    case wvcdm::KEY_ADDED:
    case wvcdm::KEY_MESSAGE:
    case wvcdm::KEY_CANCELED:
      // KEY_ADDED, KEY_MESSAGE, and KEY_CANCELLED are all alternative
      // success messages for certain CDM methods instead of NO_ERROR.
    case wvcdm::NO_ERROR:
      return Status::OK;

    case wvcdm::DECRYPT_NOT_READY:
    case wvcdm::KEY_NOT_FOUND_IN_SESSION:
    case wvcdm::NEED_KEY:
    case wvcdm::NO_MATCHING_ENTITLEMENT_KEY:
    // TODO(http://b/119690361): there are several NO_CONTENT_* errors.
    // that should probably all turn into NO_LICENSE.  Here, and below, and
    // everywhere.
    case wvcdm::NO_CONTENT_KEY_3:
      return Status::ERROR_DRM_NO_LICENSE;

    case wvcdm::NEED_PROVISIONING:
      return Status::ERROR_DRM_NOT_PROVISIONED;

    case wvcdm::DEVICE_REVOKED:
      return Status::ERROR_DRM_DEVICE_REVOKED;

    case wvcdm::INSUFFICIENT_CRYPTO_RESOURCES:
      return Status::ERROR_DRM_RESOURCE_BUSY;

    case wvcdm::RELEASE_USAGE_INFO_ERROR:
    case wvcdm::RELEASE_USAGE_INFO_FAILED:
    case wvcdm::SYSTEM_INVALIDATED_ERROR:
      return Status::ERROR_DRM_INVALID_STATE;

    case wvcdm::SESSION_NOT_FOUND_FOR_DECRYPT:
    case wvcdm::SESSION_NOT_FOUND_1:
    case wvcdm::SESSION_NOT_FOUND_2:
    case wvcdm::SESSION_NOT_FOUND_3:
    case wvcdm::SESSION_NOT_FOUND_4:
    case wvcdm::SESSION_NOT_FOUND_5:
    case wvcdm::SESSION_NOT_FOUND_6:
    case wvcdm::SESSION_NOT_FOUND_7:
    case wvcdm::SESSION_NOT_FOUND_8:
    case wvcdm::SESSION_NOT_FOUND_9:
    case wvcdm::SESSION_NOT_FOUND_10:
    case wvcdm::SESSION_NOT_FOUND_17:
    case wvcdm::SESSION_NOT_FOUND_18:
    case wvcdm::SESSION_NOT_FOUND_19:
    case wvcdm::SESSION_NOT_FOUND_20:
    case wvcdm::SESSION_NOT_FOUND_21:
      return Status::ERROR_DRM_SESSION_NOT_OPENED;

    case wvcdm::DECRYPT_ERROR:
    case wvcdm::SECURE_BUFFER_REQUIRED:
      return Status::ERROR_DRM_CANNOT_HANDLE;

    case wvcdm::ANALOG_OUTPUT_ERROR:
    case wvcdm::KEY_PROHIBITED_FOR_SECURITY_LEVEL:
    case wvcdm::INSUFFICIENT_OUTPUT_PROTECTION:
      return Status::ERROR_DRM_INSUFFICIENT_OUTPUT_PROTECTION;

    case wvcdm::KEYSET_ID_NOT_FOUND_4:
      return Status::BAD_VALUE;

    // The following cases follow the order in wv_cdm_types.h
    // to make it easier to keep track of newly defined errors.
    case wvcdm::KEY_ERROR:
    case wvcdm::ADD_KEY_ERROR:
    case wvcdm::CERT_PROVISIONING_EMPTY_SERVICE_CERTIFICATE:
    case wvcdm::CERT_PROVISIONING_GET_KEYBOX_ERROR_1:
    case wvcdm::CERT_PROVISIONING_INVALID_CERT_TYPE:
    case wvcdm::CERT_PROVISIONING_REQUEST_ERROR_1:
    case wvcdm::CERT_PROVISIONING_NONCE_GENERATION_ERROR:
    case wvcdm::CERT_PROVISIONING_REQUEST_ERROR_4:
    case wvcdm::CERT_PROVISIONING_RESPONSE_ERROR_1:
    case wvcdm::CERT_PROVISIONING_RESPONSE_ERROR_2:
    case wvcdm::CERT_PROVISIONING_RESPONSE_ERROR_3:
    case wvcdm::CERT_PROVISIONING_RESPONSE_ERROR_4:
    case wvcdm::CERT_PROVISIONING_RESPONSE_ERROR_5:
    case wvcdm::CERT_PROVISIONING_RESPONSE_ERROR_6:
    case wvcdm::CERT_PROVISIONING_RESPONSE_ERROR_7:
    case wvcdm::CERT_PROVISIONING_RESPONSE_ERROR_8:
    case wvcdm::DEVICE_CERTIFICATE_ERROR_1:
    case wvcdm::DEVICE_CERTIFICATE_ERROR_2:
    case wvcdm::DEVICE_CERTIFICATE_ERROR_3:
    case wvcdm::DEVICE_CERTIFICATE_ERROR_4:
    case wvcdm::EMPTY_KEY_DATA_1:
    case wvcdm::EMPTY_KEY_DATA_2:
    case wvcdm::EMPTY_KEYSET_ID:
    case wvcdm::EMPTY_KEYSET_ID_ENG_1:
    case wvcdm::EMPTY_KEYSET_ID_ENG_2:
    case wvcdm::EMPTY_KEYSET_ID_ENG_3:
    case wvcdm::EMPTY_KEYSET_ID_ENG_4:
    case wvcdm::EMPTY_LICENSE_RENEWAL:
    case wvcdm::EMPTY_LICENSE_RESPONSE_1:
    case wvcdm::EMPTY_LICENSE_RESPONSE_2:
    case wvcdm::EMPTY_PROVISIONING_CERTIFICATE_1:
    case wvcdm::EMPTY_PROVISIONING_RESPONSE:
    case wvcdm::EMPTY_SESSION_ID:
    case wvcdm::GENERATE_DERIVED_KEYS_ERROR:
    case wvcdm::LICENSE_RENEWAL_NONCE_GENERATION_ERROR:
    case wvcdm::GENERATE_USAGE_REPORT_ERROR:
    case wvcdm::GET_LICENSE_ERROR:
    case wvcdm::GET_RELEASED_LICENSE_ERROR:
    case wvcdm::GET_USAGE_INFO_ERROR_1:
    case wvcdm::GET_USAGE_INFO_ERROR_2:
    case wvcdm::GET_USAGE_INFO_ERROR_3:
    case wvcdm::GET_USAGE_INFO_ERROR_4:
    case wvcdm::INIT_DATA_NOT_FOUND:
    case wvcdm::INVALID_DECRYPT_PARAMETERS_ENG_1:
    case wvcdm::INVALID_DECRYPT_PARAMETERS_ENG_2:
    case wvcdm::INVALID_DECRYPT_PARAMETERS_ENG_3:
    case wvcdm::INVALID_DECRYPT_PARAMETERS_ENG_4:
    case wvcdm::INVALID_DEVICE_CERTIFICATE_TYPE:
    case wvcdm::INVALID_KEY_SYSTEM:
    case wvcdm::INVALID_LICENSE_RESPONSE:
    case wvcdm::INVALID_LICENSE_TYPE:
    case wvcdm::PARAMETER_NULL:
    case wvcdm::INVALID_PARAMETERS_LIC_1:
    case wvcdm::INVALID_PARAMETERS_LIC_2:
    case wvcdm::INVALID_PROVISIONING_PARAMETERS_1:
    case wvcdm::INVALID_PROVISIONING_PARAMETERS_2:
    case wvcdm::INVALID_PROVISIONING_REQUEST_PARAM_1:
    case wvcdm::INVALID_PROVISIONING_REQUEST_PARAM_2:
    case wvcdm::INVALID_QUERY_KEY:
    case wvcdm::INVALID_SESSION_ID:
    case wvcdm::KEY_REQUEST_ERROR_1:
    case wvcdm::KEY_SIZE_ERROR_1:
    case wvcdm::KEY_SIZE_ERROR_2:
    case wvcdm::KEYSET_ID_NOT_FOUND_1:
    case wvcdm::KEYSET_ID_NOT_FOUND_2:
    case wvcdm::KEYSET_ID_NOT_FOUND_3:
    case wvcdm::LICENSE_ID_NOT_FOUND:
    case wvcdm::LICENSE_PARSER_INIT_ERROR:
    case wvcdm::LICENSE_PARSER_NOT_INITIALIZED_1:
    case wvcdm::LICENSE_PARSER_NOT_INITIALIZED_2:
    case wvcdm::LICENSE_PARSER_NOT_INITIALIZED_3:
    case wvcdm::LICENSE_RESPONSE_NOT_SIGNED:
    case wvcdm::LICENSE_RESPONSE_PARSE_ERROR_1:
    case wvcdm::LICENSE_RESPONSE_PARSE_ERROR_2:
    case wvcdm::LICENSE_RESPONSE_PARSE_ERROR_3:
    case wvcdm::LOAD_KEY_ERROR:
    case wvcdm::NO_CONTENT_KEY:
    case wvcdm::REFRESH_KEYS_ERROR:
    case wvcdm::REMOVE_ALL_USAGE_INFO_ERROR_1:
    case wvcdm::REMOVE_ALL_USAGE_INFO_ERROR_2:
    case wvcdm::RELEASE_KEY_ERROR:
    case wvcdm::RELEASE_KEY_REQUEST_ERROR:
    case wvcdm::RELEASE_LICENSE_ERROR_1:
    case wvcdm::RELEASE_LICENSE_ERROR_2:
    case wvcdm::RENEW_KEY_ERROR_1:
    case wvcdm::RENEW_KEY_ERROR_2:
    case wvcdm::RESTORE_OFFLINE_LICENSE_ERROR_2:
    case wvcdm::NOT_INITIALIZED_ERROR:
    case wvcdm::REINIT_ERROR:
    case wvcdm::SESSION_KEYS_NOT_FOUND:
    case wvcdm::SIGNATURE_NOT_FOUND:
    case wvcdm::STORE_LICENSE_ERROR_1:
    case wvcdm::STORE_LICENSE_ERROR_2:
    case wvcdm::STORE_USAGE_INFO_ERROR:
    case wvcdm::UNPROVISION_ERROR_1:
    case wvcdm::UNPROVISION_ERROR_2:
    case wvcdm::UNPROVISION_ERROR_3:
    case wvcdm::UNPROVISION_ERROR_4:
    case wvcdm::UNSUPPORTED_INIT_DATA:
    case wvcdm::USAGE_INFO_NOT_FOUND:
    case wvcdm::PARSE_SERVICE_CERTIFICATE_ERROR:
    case wvcdm::CLIENT_ID_GENERATE_RANDOM_ERROR:
    case wvcdm::CLIENT_ID_AES_INIT_ERROR:
    case wvcdm::CLIENT_ID_AES_ENCRYPT_ERROR:
    case wvcdm::CLIENT_ID_RSA_INIT_ERROR:
    case wvcdm::CLIENT_ID_RSA_ENCRYPT_ERROR:
    case wvcdm::LICENSE_PARSER_NOT_INITIALIZED_4:
    case wvcdm::INVALID_PARAMETERS_LIC_3:
    case wvcdm::INVALID_PARAMETERS_LIC_4:
    case wvcdm::INVALID_PARAMETERS_LIC_6:
    case wvcdm::INVALID_PARAMETERS_LIC_7:
    case wvcdm::CENC_INIT_DATA_UNAVAILABLE:
    case wvcdm::PREPARE_CENC_CONTENT_ID_FAILED:
    case wvcdm::WEBM_INIT_DATA_UNAVAILABLE:
    case wvcdm::PREPARE_WEBM_CONTENT_ID_FAILED:
    case wvcdm::UNSUPPORTED_INIT_DATA_FORMAT:
    case wvcdm::LICENSE_REQUEST_NONCE_GENERATION_ERROR:
    case wvcdm::LICENSE_REQUEST_SERVICE_CERTIFICATE_GENERATION_ERROR:
    case wvcdm::EMPTY_LICENSE_REQUEST:
    case wvcdm::DUPLICATE_SESSION_ID_SPECIFIED:
    case wvcdm::LICENSE_RENEWAL_PROHIBITED:
    case wvcdm::EMPTY_PROVISIONING_CERTIFICATE_2:
    case wvcdm::OFFLINE_LICENSE_PROHIBITED:
    case wvcdm::STORAGE_PROHIBITED:
    case wvcdm::EMPTY_KEYSET_ID_ENG_5:
    case wvcdm::SESSION_NOT_FOUND_11:
    case wvcdm::LOAD_USAGE_INFO_FILE_ERROR:
    case wvcdm::LOAD_USAGE_INFO_MISSING:
    case wvcdm::SESSION_FILE_HANDLE_INIT_ERROR:
    case wvcdm::INCORRECT_CRYPTO_MODE:
    case wvcdm::INVALID_PARAMETERS_ENG_5:
    case wvcdm::SESSION_NOT_FOUND_12:
    case wvcdm::KEY_NOT_FOUND_1:
    case wvcdm::KEY_NOT_FOUND_2:
    case wvcdm::KEY_CONFLICT_1:
    case wvcdm::SESSION_NOT_FOUND_13:
    case wvcdm::SESSION_NOT_FOUND_14:
    case wvcdm::SESSION_NOT_FOUND_15:
    case wvcdm::SESSION_NOT_FOUND_16:
    case wvcdm::KEY_NOT_FOUND_3:
    case wvcdm::KEY_NOT_FOUND_4:
    case wvcdm::KEY_NOT_FOUND_5:
    case wvcdm::KEY_NOT_FOUND_6:
    case wvcdm::INVALID_SESSION_1:
    case wvcdm::NO_DEVICE_KEY_1:
    case wvcdm::NO_CONTENT_KEY_2:
    case wvcdm::INSUFFICIENT_CRYPTO_RESOURCES_2:
    case wvcdm::INVALID_PARAMETERS_ENG_13:
    case wvcdm::INVALID_PARAMETERS_ENG_14:
    case wvcdm::INVALID_PARAMETERS_ENG_15:
    case wvcdm::INVALID_PARAMETERS_ENG_16:
    case wvcdm::CLIENT_IDENTIFICATION_TOKEN_ERROR_1:
    case wvcdm::UNKNOWN_SELECT_KEY_ERROR_1:
    case wvcdm::UNKNOWN_SELECT_KEY_ERROR_2:
    case wvcdm::CREATE_USAGE_TABLE_ERROR:
    case wvcdm::LOAD_USAGE_HEADER_GENERATION_SKEW:
    case wvcdm::LOAD_USAGE_HEADER_SIGNATURE_FAILURE:
    case wvcdm::LOAD_USAGE_HEADER_BAD_MAGIC:
    case wvcdm::LOAD_USAGE_HEADER_UNKNOWN_ERROR:
    case wvcdm::INVALID_PARAMETERS_ENG_17:
    case wvcdm::INVALID_PARAMETERS_ENG_18:
    case wvcdm::INSUFFICIENT_CRYPTO_RESOURCES_3:
    case wvcdm::INSUFFICIENT_CRYPTO_RESOURCES_4:
    case wvcdm::INSUFFICIENT_CRYPTO_RESOURCES_5:
    case wvcdm::CREATE_USAGE_ENTRY_UNKNOWN_ERROR:
    case wvcdm::LOAD_USAGE_ENTRY_GENERATION_SKEW:
    case wvcdm::LOAD_USAGE_ENTRY_SIGNATURE_FAILURE:
    case wvcdm::LOAD_USAGE_ENTRY_UNKNOWN_ERROR:
    case wvcdm::INVALID_PARAMETERS_ENG_19:
    case wvcdm::INVALID_PARAMETERS_ENG_20:
    case wvcdm::UPDATE_USAGE_ENTRY_UNKNOWN_ERROR:
    case wvcdm::INVALID_PARAMETERS_ENG_21:
    case wvcdm::SHRINK_USAGE_TABLER_HEADER_UNKNOWN_ERROR:
    case wvcdm::MOVE_USAGE_ENTRY_UNKNOWN_ERROR:
    case wvcdm::COPY_OLD_USAGE_ENTRY_UNKNOWN_ERROR:
    case wvcdm::INVALID_PARAMETERS_ENG_22:
    case wvcdm::INVALID_PARAMETERS_ENG_23:
    case wvcdm::USAGE_INFORMATION_SUPPORT_FAILED:
    case wvcdm::USAGE_SUPPORT_GET_API_FAILED:
    case wvcdm::UNEXPECTED_EMPTY_USAGE_ENTRY:
    case wvcdm::INVALID_USAGE_ENTRY_NUMBER_MODIFICATION:
    case wvcdm::USAGE_INVALID_NEW_ENTRY:
    case wvcdm::USAGE_INVALID_PARAMETERS_1:
    case wvcdm::USAGE_ENTRY_NUMBER_MISMATCH:
    case wvcdm::USAGE_GET_ENTRY_RETRIEVE_INVALID_STORAGE_TYPE:
    case wvcdm::USAGE_GET_ENTRY_RETRIEVE_LICENSE_FAILED:
    case wvcdm::USAGE_GET_ENTRY_RETRIEVE_USAGE_INFO_FAILED:
    case wvcdm::USAGE_STORE_ENTRY_RETRIEVE_INVALID_STORAGE_TYPE:
    case wvcdm::USAGE_STORE_ENTRY_RETRIEVE_LICENSE_FAILED:
    case wvcdm::USAGE_STORE_ENTRY_RETRIEVE_USAGE_INFO_FAILED:
    case wvcdm::USAGE_STORE_LICENSE_FAILED:
    case wvcdm::USAGE_STORE_USAGE_INFO_FAILED:
    case wvcdm::USAGE_INVALID_LOAD_ENTRY:
    case wvcdm::REMOVE_ALL_USAGE_INFO_ERROR_5:
    case wvcdm::REMOVE_ALL_USAGE_INFO_ERROR_6:
    case wvcdm::REMOVE_ALL_USAGE_INFO_ERROR_7:
    case wvcdm::INCORRECT_USAGE_SUPPORT_TYPE_1:
    case wvcdm::INCORRECT_USAGE_SUPPORT_TYPE_2:
    case wvcdm::NO_USAGE_ENTRIES:
    case wvcdm::LIST_LICENSE_ERROR_1:
    case wvcdm::LIST_LICENSE_ERROR_2:
    case wvcdm::LIST_USAGE_ERROR_1:
    case wvcdm::LIST_USAGE_ERROR_2:
    case wvcdm::DELETE_USAGE_ERROR_1:
    case wvcdm::DELETE_USAGE_ERROR_2:
    case wvcdm::DELETE_USAGE_ERROR_3:
    case wvcdm::PRIVACY_MODE_ERROR_1:
    case wvcdm::PRIVACY_MODE_ERROR_2:
    case wvcdm::PRIVACY_MODE_ERROR_3:
    case wvcdm::EMPTY_RESPONSE_ERROR_1:
    case wvcdm::INVALID_PARAMETERS_ENG_24:
    case wvcdm::PARSE_RESPONSE_ERROR_1:
    case wvcdm::PARSE_RESPONSE_ERROR_2:
    case wvcdm::PARSE_RESPONSE_ERROR_3:
    case wvcdm::PARSE_RESPONSE_ERROR_4:
    case wvcdm::LOAD_SYSTEM_ID_ERROR:
    case wvcdm::REMOVE_USAGE_INFO_ERROR_1:
    case wvcdm::REMOVE_USAGE_INFO_ERROR_2:
    case wvcdm::REMOVE_USAGE_INFO_ERROR_3:
    case wvcdm::INSUFFICIENT_CRYPTO_RESOURCES_6:
    case wvcdm::NOT_AN_ENTITLEMENT_SESSION:
    case wvcdm::LOAD_ENTITLED_CONTENT_KEYS_ERROR:
    case wvcdm::GET_PROVISIONING_METHOD_ERROR:
    case wvcdm::DEVICE_CANNOT_REPROVISION:
    case wvcdm::SET_DECRYPT_HASH_ERROR:
    case wvcdm::GET_DECRYPT_HASH_ERROR:
    case wvcdm::INVALID_DECRYPT_HASH_FORMAT:
    case wvcdm::EMPTY_LICENSE_REQUEST_2:
    case wvcdm::EMPTY_LICENSE_REQUEST_3:
    case wvcdm::EMPTY_LICENSE_RESPONSE_3:
    case wvcdm::EMPTY_LICENSE_RESPONSE_4:
    case wvcdm::PARSE_REQUEST_ERROR_1:
    case wvcdm::PARSE_REQUEST_ERROR_2:
    case wvcdm::INVALID_LICENSE_REQUEST_TYPE_1:
    case wvcdm::INVALID_LICENSE_REQUEST_TYPE_2:
    case wvcdm::LICENSE_RESPONSE_PARSE_ERROR_4:
    case wvcdm::LICENSE_RESPONSE_PARSE_ERROR_5:
    case wvcdm::INVALID_LICENSE_TYPE_2:
    case wvcdm::SIGNATURE_NOT_FOUND_2:
    case wvcdm::SESSION_KEYS_NOT_FOUND_2:
    case wvcdm::GET_OFFLINE_LICENSE_STATE_ERROR_1:
    case wvcdm::GET_OFFLINE_LICENSE_STATE_ERROR_2:
    case wvcdm::REMOVE_OFFLINE_LICENSE_ERROR_1:
    case wvcdm::REMOVE_OFFLINE_LICENSE_ERROR_2:
    case wvcdm::OUTPUT_TOO_LARGE_ERROR:
    case wvcdm::SESSION_LOST_STATE_ERROR:
    case wvcdm::GENERATE_DERIVED_KEYS_ERROR_2:
    case wvcdm::LOAD_DEVICE_RSA_KEY_ERROR:
    case wvcdm::NONCE_GENERATION_ERROR:
    case wvcdm::GENERATE_SIGNATURE_ERROR:
    case wvcdm::UNKNOWN_CLIENT_TOKEN_TYPE:
    case wvcdm::DEACTIVATE_USAGE_ENTRY_ERROR:
    case wvcdm::SERVICE_CERTIFICATE_PROVIDER_ID_EMPTY:
    case wvcdm::OPEN_CRYPTO_SESSION_ERROR:
    case wvcdm::LOAD_SRM_ERROR:
    case wvcdm::RANDOM_GENERATION_ERROR:
    case wvcdm::CRYPTO_SESSION_NOT_INITIALIZED:
    case wvcdm::GET_DEVICE_ID_ERROR:
    case wvcdm::GET_TOKEN_FROM_OEM_CERT_ERROR:
    case wvcdm::CRYPTO_SESSION_NOT_OPEN:
    case wvcdm::GET_TOKEN_FROM_KEYBOX_ERROR:
    case wvcdm::KEYBOX_TOKEN_TOO_SHORT:
    case wvcdm::EXTRACT_SYSTEM_ID_FROM_OEM_CERT_ERROR:
    case wvcdm::RSA_SIGNATURE_GENERATION_ERROR:
    case wvcdm::GET_HDCP_CAPABILITY_FAILED:
    case wvcdm::GET_NUMBER_OF_OPEN_SESSIONS_ERROR:
    case wvcdm::GET_MAX_NUMBER_OF_OPEN_SESSIONS_ERROR:
    case wvcdm::NOT_IMPLEMENTED_ERROR:
    case wvcdm::GET_SRM_VERSION_ERROR:
    case wvcdm::REWRAP_DEVICE_RSA_KEY_ERROR:
    case wvcdm::REWRAP_DEVICE_RSA_KEY_30_ERROR:
    case wvcdm::INVALID_SRM_LIST:

      ALOGW("Returns UNKNOWN error for legacy status: %d", res);
      return Status::ERROR_DRM_UNKNOWN;

    case wvcdm::UNKNOWN_ERROR:
      return Status::ERROR_DRM_UNKNOWN;
  }

  // Return here instead of as a default case so that the compiler will warn
  // us if we forget to include an enum member in the switch statement.
  return Status::ERROR_DRM_UNKNOWN;
}

static Status_V1_2 mapCdmResponseType_1_2(
        wvcdm::CdmResponseType res) {
    switch(res) {
        case wvcdm::KEY_PROHIBITED_FOR_SECURITY_LEVEL:
            return Status_V1_2::ERROR_DRM_INSUFFICIENT_SECURITY;
        case wvcdm::OUTPUT_TOO_LARGE_ERROR:
            return Status_V1_2::ERROR_DRM_FRAME_TOO_LARGE;
        case wvcdm::SESSION_LOST_STATE_ERROR:
            return Status_V1_2::ERROR_DRM_SESSION_LOST_STATE;
        case wvcdm::LICENSE_REQUEST_NONCE_GENERATION_ERROR:
        case wvcdm::LICENSE_RENEWAL_NONCE_GENERATION_ERROR:
        case wvcdm::CERT_PROVISIONING_NONCE_GENERATION_ERROR:
        case wvcdm::NONCE_GENERATION_ERROR:
            // These are likely nonce flood errors
            return Status_V1_2::ERROR_DRM_RESOURCE_CONTENTION;

    default:
            return static_cast<Status_V1_2>(mapCdmResponseType(res));
    }
}


static inline bool isCdmResponseTypeSuccess(wvcdm::CdmResponseType res) {
  return mapCdmResponseType(res) == Status::OK;
}

} // namespace wvdrm

#endif // WV_MAP_ERRORS_H_
