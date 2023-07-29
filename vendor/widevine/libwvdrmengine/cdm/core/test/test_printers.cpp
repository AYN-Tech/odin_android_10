// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
// This file adds some print methods so that when unit tests fail, the
// will print the name of an enumeration instead of the numeric value.

#include "test_printers.h"

namespace wvcdm {

void PrintTo(const enum CdmResponseType& value, ::std::ostream* os) {
  switch (value) {
    case NO_ERROR:
      *os << "NO_ERROR";
      break;
    case UNKNOWN_ERROR:
      *os << "UNKNOWN_ERROR";
      break;
    case KEY_ADDED:
      *os << "KEY_ADDED";
      break;
    case KEY_ERROR:
      *os << "KEY_ERROR";
      break;
    case KEY_MESSAGE:
      *os << "KEY_MESSAGE";
      break;
    case NEED_KEY:
      *os << "NEED_KEY";
      break;
    case KEY_CANCELED:
      *os << "KEY_CANCELED";
      break;
    case NEED_PROVISIONING:
      *os << "NEED_PROVISIONING";
      break;
    case DEVICE_REVOKED:
      *os << "DEVICE_REVOKED";
      break;
    case INSUFFICIENT_CRYPTO_RESOURCES:
      *os << "INSUFFICIENT_CRYPTO_RESOURCES";
      break;
    case ADD_KEY_ERROR:
      *os << "ADD_KEY_ERROR";
      break;
    case CERT_PROVISIONING_GET_KEYBOX_ERROR_1:
      *os << "CERT_PROVISIONING_GET_KEYBOX_ERROR_1";
      break;
    case CERT_PROVISIONING_INVALID_CERT_TYPE:
      *os << "CERT_PROVISIONING_INVALID_CERT_TYPE";
      break;
    case CERT_PROVISIONING_REQUEST_ERROR_1:
      *os << "CERT_PROVISIONING_REQUEST_ERROR_1";
      break;
    case CERT_PROVISIONING_NONCE_GENERATION_ERROR:
      *os << "CERT_PROVISIONING_NONCE_GENERATION_ERROR";
      break;
    case CERT_PROVISIONING_REQUEST_ERROR_4:
      *os << "CERT_PROVISIONING_REQUEST_ERROR_4";
      break;
    case CERT_PROVISIONING_RESPONSE_ERROR_1:
      *os << "CERT_PROVISIONING_RESPONSE_ERROR_1";
      break;
    case CERT_PROVISIONING_RESPONSE_ERROR_2:
      *os << "CERT_PROVISIONING_RESPONSE_ERROR_2";
      break;
    case CERT_PROVISIONING_RESPONSE_ERROR_3:
      *os << "CERT_PROVISIONING_RESPONSE_ERROR_3";
      break;
    case CERT_PROVISIONING_RESPONSE_ERROR_4:
      *os << "CERT_PROVISIONING_RESPONSE_ERROR_4";
      break;
    case CERT_PROVISIONING_RESPONSE_ERROR_5:
      *os << "CERT_PROVISIONING_RESPONSE_ERROR_5";
      break;
    case CERT_PROVISIONING_RESPONSE_ERROR_6:
      *os << "CERT_PROVISIONING_RESPONSE_ERROR_6";
      break;
    case CERT_PROVISIONING_RESPONSE_ERROR_7:
      *os << "CERT_PROVISIONING_RESPONSE_ERROR_7";
      break;
    case CERT_PROVISIONING_RESPONSE_ERROR_8:
      *os << "CERT_PROVISIONING_RESPONSE_ERROR_8";
      break;
    case DECRYPT_NOT_READY:
      *os << "DECRYPT_NOT_READY";
      break;
    case DEVICE_CERTIFICATE_ERROR_1:
      *os << "DEVICE_CERTIFICATE_ERROR_1";
      break;
    case DEVICE_CERTIFICATE_ERROR_2:
      *os << "DEVICE_CERTIFICATE_ERROR_2";
      break;
    case DEVICE_CERTIFICATE_ERROR_3:
      *os << "DEVICE_CERTIFICATE_ERROR_3";
      break;
    case DEVICE_CERTIFICATE_ERROR_4:
      *os << "DEVICE_CERTIFICATE_ERROR_4";
      break;
    case EMPTY_KEY_DATA_1:
      *os << "EMPTY_KEY_DATA_1";
      break;
    case EMPTY_KEY_DATA_2:
      *os << "EMPTY_KEY_DATA_2";
      break;
    case EMPTY_KEYSET_ID:
      *os << "EMPTY_KEYSET_ID";
      break;
    case EMPTY_KEYSET_ID_ENG_1:
      *os << "EMPTY_KEYSET_ID_ENG_1";
      break;
    case EMPTY_KEYSET_ID_ENG_2:
      *os << "EMPTY_KEYSET_ID_ENG_2";
      break;
    case EMPTY_KEYSET_ID_ENG_3:
      *os << "EMPTY_KEYSET_ID_ENG_3";
      break;
    case EMPTY_KEYSET_ID_ENG_4:
      *os << "EMPTY_KEYSET_ID_ENG_4";
      break;
    case EMPTY_LICENSE_RENEWAL:
      *os << "EMPTY_LICENSE_RENEWAL";
      break;
    case EMPTY_LICENSE_RESPONSE_1:
      *os << "EMPTY_LICENSE_RESPONSE_1";
      break;
    case EMPTY_LICENSE_RESPONSE_2:
      *os << "EMPTY_LICENSE_RESPONSE_2";
      break;
    case EMPTY_PROVISIONING_CERTIFICATE_1:
      *os << "EMPTY_PROVISIONING_CERTIFICATE_1";
      break;
    case EMPTY_PROVISIONING_RESPONSE:
      *os << "EMPTY_PROVISIONING_RESPONSE";
      break;
    case EMPTY_SESSION_ID:
      *os << "EMPTY_SESSION_ID";
      break;
    case GENERATE_DERIVED_KEYS_ERROR:
      *os << "GENERATE_DERIVED_KEYS_ERROR";
      break;
    case LICENSE_RENEWAL_NONCE_GENERATION_ERROR:
      *os << "LICENSE_RENEWAL_NONCE_GENERATION_ERROR";
      break;
    case GENERATE_USAGE_REPORT_ERROR:
      *os << "GENERATE_USAGE_REPORT_ERROR";
      break;
    case GET_LICENSE_ERROR:
      *os << "GET_LICENSE_ERROR";
      break;
    case GET_OFFLINE_LICENSE_STATE_ERROR_1:
      *os << "GET_OFFLINE_LICENSE_STATE_ERROR_1";
      break;
    case GET_OFFLINE_LICENSE_STATE_ERROR_2:
      *os << "GET_OFFLINE_LICENSE_STATE_ERROR_2";
      break;
    case GET_RELEASED_LICENSE_ERROR:
      *os << "GET_RELEASED_LICENSE_ERROR";
      break;
    case GET_USAGE_INFO_ERROR_1:
      *os << "GET_USAGE_INFO_ERROR_1";
      break;
    case GET_USAGE_INFO_ERROR_2:
      *os << "GET_USAGE_INFO_ERROR_2";
      break;
    case GET_USAGE_INFO_ERROR_3:
      *os << "GET_USAGE_INFO_ERROR_3";
      break;
    case GET_USAGE_INFO_ERROR_4:
      *os << "GET_USAGE_INFO_ERROR_4";
      break;
    case INIT_DATA_NOT_FOUND:
      *os << "INIT_DATA_NOT_FOUND";
      break;
    case INVALID_DECRYPT_PARAMETERS_ENG_1:
      *os << "INVALID_DECRYPT_PARAMETERS_ENG_1";
      break;
    case INVALID_DECRYPT_PARAMETERS_ENG_2:
      *os << "INVALID_DECRYPT_PARAMETERS_ENG_2";
      break;
    case INVALID_DECRYPT_PARAMETERS_ENG_3:
      *os << "INVALID_DECRYPT_PARAMETERS_ENG_3";
      break;
    case INVALID_DECRYPT_PARAMETERS_ENG_4:
      *os << "INVALID_DECRYPT_PARAMETERS_ENG_4";
      break;
    case INVALID_DEVICE_CERTIFICATE_TYPE:
      *os << "INVALID_DEVICE_CERTIFICATE_TYPE";
      break;
    case INVALID_KEY_SYSTEM:
      *os << "INVALID_KEY_SYSTEM";
      break;
    case INVALID_LICENSE_RESPONSE:
      *os << "INVALID_LICENSE_RESPONSE";
      break;
    case INVALID_LICENSE_TYPE:
      *os << "INVALID_LICENSE_TYPE";
      break;
    case PARAMETER_NULL:
      *os << "PARAMETER_NULL";
      break;
    case NOT_INITIALIZED_ERROR:
      *os << "NOT_INITIALIZED_ERROR";
      break;
    case REINIT_ERROR:
      *os << "REINIT_ERROR";
      break;
    case INVALID_PARAMETERS_LIC_1:
      *os << "INVALID_PARAMETERS_LIC_1";
      break;
    case INVALID_PARAMETERS_LIC_2:
      *os << "INVALID_PARAMETERS_LIC_2";
      break;
    case INVALID_PROVISIONING_PARAMETERS_1:
      *os << "INVALID_PROVISIONING_PARAMETERS_1";
      break;
    case INVALID_PROVISIONING_PARAMETERS_2:
      *os << "INVALID_PROVISIONING_PARAMETERS_2";
      break;
    case INVALID_PROVISIONING_REQUEST_PARAM_1:
      *os << "INVALID_PROVISIONING_REQUEST_PARAM_1";
      break;
    case INVALID_PROVISIONING_REQUEST_PARAM_2:
      *os << "INVALID_PROVISIONING_REQUEST_PARAM_2";
      break;
    case INVALID_QUERY_KEY:
      *os << "INVALID_QUERY_KEY";
      break;
    case INVALID_SESSION_ID:
      *os << "INVALID_SESSION_ID";
      break;
    case KEY_REQUEST_ERROR_1:
      *os << "KEY_REQUEST_ERROR_1";
      break;
    case KEY_SIZE_ERROR_1:
      *os << "KEY_SIZE_ERROR_1";
      break;
    case KEY_SIZE_ERROR_2:
      *os << "KEY_SIZE_ERROR_2";
      break;
    case KEYSET_ID_NOT_FOUND_1:
      *os << "KEYSET_ID_NOT_FOUND_1";
      break;
    case KEYSET_ID_NOT_FOUND_2:
      *os << "KEYSET_ID_NOT_FOUND_2";
      break;
    case KEYSET_ID_NOT_FOUND_3:
      *os << "KEYSET_ID_NOT_FOUND_3";
      break;
    case KEYSET_ID_NOT_FOUND_4:
      *os << "KEYSET_ID_NOT_FOUND_4";
      break;
    case LICENSE_ID_NOT_FOUND:
      *os << "LICENSE_ID_NOT_FOUND";
      break;
    case LICENSE_PARSER_INIT_ERROR:
      *os << "LICENSE_PARSER_INIT_ERROR";
      break;
    case LICENSE_PARSER_NOT_INITIALIZED_1:
      *os << "LICENSE_PARSER_NOT_INITIALIZED_1";
      break;
    case LICENSE_PARSER_NOT_INITIALIZED_2:
      *os << "LICENSE_PARSER_NOT_INITIALIZED_2";
      break;
    case LICENSE_PARSER_NOT_INITIALIZED_3:
      *os << "LICENSE_PARSER_NOT_INITIALIZED_3";
      break;
    case LICENSE_RESPONSE_NOT_SIGNED:
      *os << "LICENSE_RESPONSE_NOT_SIGNED";
      break;
    case LICENSE_RESPONSE_PARSE_ERROR_1:
      *os << "LICENSE_RESPONSE_PARSE_ERROR_1";
      break;
    case LICENSE_RESPONSE_PARSE_ERROR_2:
      *os << "LICENSE_RESPONSE_PARSE_ERROR_2";
      break;
    case LICENSE_RESPONSE_PARSE_ERROR_3:
      *os << "LICENSE_RESPONSE_PARSE_ERROR_3";
      break;
    case LOAD_KEY_ERROR:
      *os << "LOAD_KEY_ERROR";
      break;
    case NO_CONTENT_KEY:
      *os << "NO_CONTENT_KEY";
      break;
    case REFRESH_KEYS_ERROR:
      *os << "REFRESH_KEYS_ERROR";
      break;
    case REMOVE_ALL_USAGE_INFO_ERROR_1:
      *os << "REMOVE_ALL_USAGE_INFO_ERROR_1";
      break;
    case REMOVE_ALL_USAGE_INFO_ERROR_2:
      *os << "REMOVE_ALL_USAGE_INFO_ERROR_2";
      break;
    case REMOVE_ALL_USAGE_INFO_ERROR_5:
      *os << "REMOVE_ALL_USAGE_INFO_ERROR_5";
      break;
    case REMOVE_ALL_USAGE_INFO_ERROR_6:
      *os << "REMOVE_ALL_USAGE_INFO_ERROR_6";
      break;
    case REMOVE_ALL_USAGE_INFO_ERROR_7:
      *os << "REMOVE_ALL_USAGE_INFO_ERROR_7";
      break;
    case RELEASE_KEY_ERROR:
      *os << "RELEASE_KEY_ERROR";
      break;
    case RELEASE_KEY_REQUEST_ERROR:
      *os << "RELEASE_KEY_REQUEST_ERROR";
      break;
    case RELEASE_LICENSE_ERROR_1:
      *os << "RELEASE_LICENSE_ERROR_1";
      break;
    case RELEASE_LICENSE_ERROR_2:
      *os << "RELEASE_LICENSE_ERROR_2";
      break;
    case RELEASE_USAGE_INFO_ERROR:
      *os << "RELEASE_USAGE_INFO_ERROR";
      break;
    case RENEW_KEY_ERROR_1:
      *os << "RENEW_KEY_ERROR_1";
      break;
    case RENEW_KEY_ERROR_2:
      *os << "RENEW_KEY_ERROR_2";
      break;
    case RESTORE_OFFLINE_LICENSE_ERROR_2:
      *os << "RESTORE_OFFLINE_LICENSE_ERROR_2";
      break;
    case SESSION_NOT_FOUND_1:
      *os << "SESSION_NOT_FOUND_1";
      break;
    case SESSION_NOT_FOUND_2:
      *os << "SESSION_NOT_FOUND_2";
      break;
    case SESSION_NOT_FOUND_3:
      *os << "SESSION_NOT_FOUND_3";
      break;
    case SESSION_NOT_FOUND_4:
      *os << "SESSION_NOT_FOUND_4";
      break;
    case SESSION_NOT_FOUND_5:
      *os << "SESSION_NOT_FOUND_5";
      break;
    case SESSION_NOT_FOUND_6:
      *os << "SESSION_NOT_FOUND_6";
      break;
    case SESSION_NOT_FOUND_7:
      *os << "SESSION_NOT_FOUND_7";
      break;
    case SESSION_NOT_FOUND_8:
      *os << "SESSION_NOT_FOUND_8";
      break;
    case SESSION_NOT_FOUND_9:
      *os << "SESSION_NOT_FOUND_9";
      break;
    case SESSION_NOT_FOUND_10:
      *os << "SESSION_NOT_FOUND_10";
      break;
    case SESSION_NOT_FOUND_FOR_DECRYPT:
      *os << "SESSION_NOT_FOUND_FOR_DECRYPT";
      break;
    case SESSION_KEYS_NOT_FOUND:
      *os << "SESSION_KEYS_NOT_FOUND";
      break;
    case SIGNATURE_NOT_FOUND:
      *os << "SIGNATURE_NOT_FOUND";
      break;
    case STORE_LICENSE_ERROR_1:
      *os << "STORE_LICENSE_ERROR_1";
      break;
    case STORE_LICENSE_ERROR_2:
      *os << "STORE_LICENSE_ERROR_2";
      break;
    case STORE_USAGE_INFO_ERROR:
      *os << "STORE_USAGE_INFO_ERROR";
      break;
    case UNPROVISION_ERROR_1:
      *os << "UNPROVISION_ERROR_1";
      break;
    case UNPROVISION_ERROR_2:
      *os << "UNPROVISION_ERROR_2";
      break;
    case UNPROVISION_ERROR_3:
      *os << "UNPROVISION_ERROR_3";
      break;
    case UNPROVISION_ERROR_4:
      *os << "UNPROVISION_ERROR_4";
      break;
    case UNSUPPORTED_INIT_DATA:
      *os << "UNSUPPORTED_INIT_DATA";
      break;
    case USAGE_INFO_NOT_FOUND:
      *os << "USAGE_INFO_NOT_FOUND";
      break;
    case EMPTY_PROVISIONING_CERTIFICATE_2:
      *os << "EMPTY_PROVISIONING_CERTIFICATE_2";
      break;
    case PARSE_SERVICE_CERTIFICATE_ERROR:
      *os << "PARSE_SERVICE_CERTIFICATE_ERROR";
      break;
    case CLIENT_ID_GENERATE_RANDOM_ERROR:
      *os << "CLIENT_ID_GENERATE_RANDOM_ERROR";
      break;
    case CLIENT_ID_AES_INIT_ERROR:
      *os << "CLIENT_ID_AES_INIT_ERROR";
      break;
    case CLIENT_ID_AES_ENCRYPT_ERROR:
      *os << "CLIENT_ID_AES_ENCRYPT_ERROR";
      break;
    case CLIENT_ID_RSA_INIT_ERROR:
      *os << "CLIENT_ID_RSA_INIT_ERROR";
      break;
    case CLIENT_ID_RSA_ENCRYPT_ERROR:
      *os << "CLIENT_ID_RSA_ENCRYPT_ERROR";
      break;
    case LICENSE_PARSER_NOT_INITIALIZED_4:
      *os << "LICENSE_PARSER_NOT_INITIALIZED_4";
      break;
    case INVALID_PARAMETERS_LIC_3:
      *os << "INVALID_PARAMETERS_LIC_3";
      break;
    case INVALID_PARAMETERS_LIC_4:
      *os << "INVALID_PARAMETERS_LIC_4";
      break;
    case INVALID_PARAMETERS_LIC_6:
      *os << "INVALID_PARAMETERS_LIC_6";
      break;
    case INVALID_PARAMETERS_LIC_7:
      *os << "INVALID_PARAMETERS_LIC_7";
      break;
    case CENC_INIT_DATA_UNAVAILABLE:
      *os << "CENC_INIT_DATA_UNAVAILABLE";
      break;
    case PREPARE_CENC_CONTENT_ID_FAILED:
      *os << "PREPARE_CENC_CONTENT_ID_FAILED";
      break;
    case WEBM_INIT_DATA_UNAVAILABLE:
      *os << "WEBM_INIT_DATA_UNAVAILABLE";
      break;
    case PREPARE_WEBM_CONTENT_ID_FAILED:
      *os << "PREPARE_WEBM_CONTENT_ID_FAILED";
      break;
    case UNSUPPORTED_INIT_DATA_FORMAT:
      *os << "UNSUPPORTED_INIT_DATA_FORMAT";
      break;
    case LICENSE_REQUEST_NONCE_GENERATION_ERROR:
      *os << "LICENSE_REQUEST_NONCE_GENERATION_ERROR";
      break;
    case EMPTY_LICENSE_REQUEST:
      *os << "EMPTY_LICENSE_REQUEST";
      break;
    case DUPLICATE_SESSION_ID_SPECIFIED:
      *os << "DUPLICATE_SESSION_ID_SPECIFIED";
      break;
    case LICENSE_RENEWAL_PROHIBITED:
      *os << "LICENSE_RENEWAL_PROHIBITED";
      break;
    case SESSION_FILE_HANDLE_INIT_ERROR:
      *os << "SESSION_FILE_HANDLE_INIT_ERROR";
      break;
    case INCORRECT_CRYPTO_MODE:
      *os << "INCORRECT_CRYPTO_MODE";
      break;
    case INVALID_PARAMETERS_ENG_5:
      *os << "INVALID_PARAMETERS_ENG_5";
      break;
    case DECRYPT_ERROR:
      *os << "DECRYPT_ERROR";
      break;
    case INSUFFICIENT_OUTPUT_PROTECTION:
      *os << "INSUFFICIENT_OUTPUT_PROTECTION";
      break;
    case SESSION_NOT_FOUND_12:
      *os << "SESSION_NOT_FOUND_12";
      break;
    case KEY_NOT_FOUND_1:
      *os << "KEY_NOT_FOUND_1";
      break;
    case KEY_NOT_FOUND_2:
      *os << "KEY_NOT_FOUND_2";
      break;
    case KEY_CONFLICT_1:
      *os << "KEY_CONFLICT_1";
      break;
    case SESSION_NOT_FOUND_13:
      *os << "SESSION_NOT_FOUND_13";
      break;
    case SESSION_NOT_FOUND_14:
      *os << "SESSION_NOT_FOUND_14";
      break;
    case SESSION_NOT_FOUND_15:
      *os << "SESSION_NOT_FOUND_15";
      break;
    case SESSION_NOT_FOUND_16:
      *os << "SESSION_NOT_FOUND_16";
      break;
    case KEY_NOT_FOUND_3:
      *os << "KEY_NOT_FOUND_3";
      break;
    case KEY_NOT_FOUND_4:
      *os << "KEY_NOT_FOUND_4";
      break;
    case KEY_NOT_FOUND_5:
      *os << "KEY_NOT_FOUND_5";
      break;
    case KEY_NOT_FOUND_6:
      *os << "KEY_NOT_FOUND_6";
      break;
    case INVALID_SESSION_1:
      *os << "INVALID_SESSION_1";
      break;
    case NO_DEVICE_KEY_1:
      *os << "NO_DEVICE_KEY_1";
      break;
    case NO_CONTENT_KEY_2:
      *os << "NO_CONTENT_KEY_2";
      break;
    case NO_CONTENT_KEY_3:
      *os << "NO_CONTENT_KEY_3";
      break;
    case INSUFFICIENT_CRYPTO_RESOURCES_2:
      *os << "INSUFFICIENT_CRYPTO_RESOURCES_2";
      break;
    case INVALID_PARAMETERS_ENG_13:
      *os << "INVALID_PARAMETERS_ENG_13";
      break;
    case INVALID_PARAMETERS_ENG_14:
      *os << "INVALID_PARAMETERS_ENG_14";
      break;
    case INVALID_PARAMETERS_ENG_15:
      *os << "INVALID_PARAMETERS_ENG_15";
      break;
    case INVALID_PARAMETERS_ENG_16:
      *os << "INVALID_PARAMETERS_ENG_16";
      break;
    case INVALID_PARAMETERS_ENG_17:
      *os << "INVALID_PARAMETERS_ENG_17";
      break;
    case INVALID_PARAMETERS_ENG_18:
      *os << "INVALID_PARAMETERS_ENG_18";
      break;
    case INVALID_PARAMETERS_ENG_19:
      *os << "INVALID_PARAMETERS_ENG_19";
      break;
    case CLIENT_IDENTIFICATION_TOKEN_ERROR_1:
      *os << "CLIENT_IDENTIFICATION_TOKEN_ERROR_1";
      break;
    case ANALOG_OUTPUT_ERROR:
      *os << "ANALOG_OUTPUT_ERROR";
      break;
    case UNKNOWN_SELECT_KEY_ERROR_1:
      *os << "UNKNOWN_SELECT_KEY_ERROR_1";
      break;
    case UNKNOWN_SELECT_KEY_ERROR_2:
      *os << "UNKNOWN_SELECT_KEY_ERROR_2";
      break;
    case CREATE_USAGE_TABLE_ERROR:
      *os << "CREATE_USAGE_TABLE_ERROR";
      break;
    case LOAD_USAGE_HEADER_GENERATION_SKEW:
      *os << "LOAD_USAGE_HEADER_GENERATION_SKEW";
      break;
    case LOAD_USAGE_HEADER_SIGNATURE_FAILURE:
      *os << "LOAD_USAGE_HEADER_SIGNATURE_FAILURE";
      break;
    case LOAD_USAGE_HEADER_BAD_MAGIC:
      *os << "LOAD_USAGE_HEADER_BAD_MAGIC";
      break;
    case LOAD_USAGE_HEADER_UNKNOWN_ERROR:
      *os << "LOAD_USAGE_HEADER_UNKNOWN_ERROR";
      break;
    case INSUFFICIENT_CRYPTO_RESOURCES_3:
      *os << "INSUFFICIENT_CRYPTO_RESOURCES_3";
      break;
    case CREATE_USAGE_ENTRY_UNKNOWN_ERROR:
      *os << "CREATE_USAGE_ENTRY_UNKNOWN_ERROR";
      break;
    case LOAD_USAGE_ENTRY_GENERATION_SKEW:
      *os << "LOAD_USAGE_ENTRY_GENERATION_SKEW";
      break;
    case LOAD_USAGE_ENTRY_SIGNATURE_FAILURE:
      *os << "LOAD_USAGE_ENTRY_SIGNATURE_FAILURE";
      break;
    case LOAD_USAGE_ENTRY_UNKNOWN_ERROR:
      *os << "LOAD_USAGE_ENTRY_UNKNOWN_ERROR";
      break;
    case INVALID_PARAMETERS_ENG_20:
      *os << "INVALID_PARAMETERS_ENG_20";
      break;
    case UPDATE_USAGE_ENTRY_UNKNOWN_ERROR:
      *os << "UPDATE_USAGE_ENTRY_UNKNOWN_ERROR";
      break;
    case INVALID_PARAMETERS_ENG_21:
      *os << "INVALID_PARAMETERS_ENG_21";
      break;
    case INVALID_PARAMETERS_ENG_22:
      *os << "INVALID_PARAMETERS_ENG_22";
      break;
    case SHRINK_USAGE_TABLER_HEADER_UNKNOWN_ERROR:
      *os << "SHRINK_USAGE_TABLER_HEADER_UNKNOWN_ERROR";
      break;
    case MOVE_USAGE_ENTRY_UNKNOWN_ERROR:
      *os << "MOVE_USAGE_ENTRY_UNKNOWN_ERROR";
      break;
    case COPY_OLD_USAGE_ENTRY_UNKNOWN_ERROR:
      *os << "COPY_OLD_USAGE_ENTRY_UNKNOWN_ERROR";
      break;
    case INVALID_PARAMETERS_ENG_23:
      *os << "INVALID_PARAMETERS_ENG_23";
      break;
    case INVALID_PARAMETERS_ENG_24:
      *os << "INVALID_PARAMETERS_ENG_24";
      break;
    case USAGE_INFORMATION_SUPPORT_FAILED:
      *os << "USAGE_INFORMATION_SUPPORT_FAILED";
      break;
    case USAGE_SUPPORT_GET_API_FAILED:
      *os << "USAGE_SUPPORT_GET_API_FAILED";
      break;
    case UNEXPECTED_EMPTY_USAGE_ENTRY:
      *os << "UNEXPECTED_EMPTY_USAGE_ENTRY";
      break;
    case INVALID_USAGE_ENTRY_NUMBER_MODIFICATION:
      *os << "INVALID_USAGE_ENTRY_NUMBER_MODIFICATION";
      break;
    case USAGE_INVALID_NEW_ENTRY:
      *os << "USAGE_INVALID_NEW_ENTRY";
      break;
    case USAGE_INVALID_PARAMETERS_1:
      *os << "USAGE_INVALID_PARAMETERS_1";
      break;
    case USAGE_GET_ENTRY_RETRIEVE_LICENSE_FAILED:
      *os << "USAGE_GET_ENTRY_RETRIEVE_LICENSE_FAILED";
      break;
    case USAGE_GET_ENTRY_RETRIEVE_USAGE_INFO_FAILED:
      *os << "USAGE_GET_ENTRY_RETRIEVE_USAGE_INFO_FAILED";
      break;
    case USAGE_GET_ENTRY_RETRIEVE_INVALID_STORAGE_TYPE:
      *os << "USAGE_GET_ENTRY_RETRIEVE_INVALID_STORAGE_TYPE";
      break;
    case USAGE_ENTRY_NUMBER_MISMATCH:
      *os << "USAGE_ENTRY_NUMBER_MISMATCH";
      break;
    case USAGE_STORE_LICENSE_FAILED:
      *os << "USAGE_STORE_LICENSE_FAILED";
      break;
    case USAGE_STORE_USAGE_INFO_FAILED:
      *os << "USAGE_STORE_USAGE_INFO_FAILED";
      break;
    case USAGE_INVALID_LOAD_ENTRY:
      *os << "USAGE_INVALID_LOAD_ENTRY";
      break;
    case RELEASE_USAGE_INFO_FAILED:
      *os << "RELEASE_USAGE_INFO_FAILED";
      break;
    case INCORRECT_USAGE_SUPPORT_TYPE_1:
      *os << "INCORRECT_USAGE_SUPPORT_TYPE_1";
      break;
    case INCORRECT_USAGE_SUPPORT_TYPE_2:
      *os << "INCORRECT_USAGE_SUPPORT_TYPE_2";
      break;
    case KEY_PROHIBITED_FOR_SECURITY_LEVEL:
      *os << "KEY_PROHIBITED_FOR_SECURITY_LEVEL";
      break;
    case KEY_NOT_FOUND_IN_SESSION:
      *os << "KEY_NOT_FOUND_IN_SESSION";
      break;
    case NO_USAGE_ENTRIES:
      *os << "NO_USAGE_ENTRIES";
      break;
    case LIST_LICENSE_ERROR_1:
      *os << "LIST_LICENSE_ERROR_1";
      break;
    case LIST_LICENSE_ERROR_2:
      *os << "LIST_LICENSE_ERROR_2";
      break;
    case LIST_USAGE_ERROR_1:
      *os << "LIST_USAGE_ERROR_1";
      break;
    case LIST_USAGE_ERROR_2:
      *os << "LIST_USAGE_ERROR_2";
      break;
    case DELETE_USAGE_ERROR_1:
      *os << "DELETE_USAGE_ERROR_1";
      break;
    case DELETE_USAGE_ERROR_2:
      *os << "DELETE_USAGE_ERROR_2";
      break;
    case DELETE_USAGE_ERROR_3:
      *os << "DELETE_USAGE_ERROR_3";
      break;
    case PRIVACY_MODE_ERROR_1:
      *os << "PRIVACY_MODE_ERROR_1";
      break;
    case PRIVACY_MODE_ERROR_2:
      *os << "PRIVACY_MODE_ERROR_2";
      break;
    case PRIVACY_MODE_ERROR_3:
      *os << "PRIVACY_MODE_ERROR_3";
      break;
    case EMPTY_RESPONSE_ERROR_1:
      *os << "EMPTY_RESPONSE_ERROR_1";
      break;
    case PARSE_RESPONSE_ERROR_1:
      *os << "PARSE_RESPONSE_ERROR_1";
      break;
    case PARSE_RESPONSE_ERROR_2:
      *os << "PARSE_RESPONSE_ERROR_2";
      break;
    case PARSE_RESPONSE_ERROR_3:
      *os << "PARSE_RESPONSE_ERROR_3";
      break;
    case PARSE_RESPONSE_ERROR_4:
      *os << "PARSE_RESPONSE_ERROR_4";
      break;
    case USAGE_STORE_ENTRY_RETRIEVE_LICENSE_FAILED:
      *os << "USAGE_STORE_ENTRY_RETRIEVE_LICENSE_FAILED";
      break;
    case USAGE_STORE_ENTRY_RETRIEVE_USAGE_INFO_FAILED:
      *os << "USAGE_STORE_ENTRY_RETRIEVE_USAGE_INFO_FAILED";
      break;
    case USAGE_STORE_ENTRY_RETRIEVE_INVALID_STORAGE_TYPE:
      *os << "USAGE_STORE_ENTRY_RETRIEVE_INVALID_STORAGE_TYPE";
      break;
    case CERT_PROVISIONING_EMPTY_SERVICE_CERTIFICATE:
      *os << "CERT_PROVISIONING_EMPTY_SERVICE_CERTIFICATE";
      break;
    case LOAD_SYSTEM_ID_ERROR:
      *os << "LOAD_SYSTEM_ID_ERROR";
      break;
    case INSUFFICIENT_CRYPTO_RESOURCES_4:
      *os << "INSUFFICIENT_CRYPTO_RESOURCES_4";
      break;
    case INSUFFICIENT_CRYPTO_RESOURCES_5:
      *os << "INSUFFICIENT_CRYPTO_RESOURCES_5";
      break;
    case REMOVE_USAGE_INFO_ERROR_1:
      *os << "REMOVE_USAGE_INFO_ERROR_1";
      break;
    case REMOVE_USAGE_INFO_ERROR_2:
      *os << "REMOVE_USAGE_INFO_ERROR_2";
      break;
    case REMOVE_USAGE_INFO_ERROR_3:
      *os << "REMOVE_USAGE_INFO_ERROR_3";
      break;
    case INSUFFICIENT_CRYPTO_RESOURCES_6:
      *os << "INSUFFICIENT_CRYPTO_RESOURCES_6";
      break;
    case NOT_AN_ENTITLEMENT_SESSION:
      *os << "NOT_AN_ENTITLEMENT_SESSION";
      break;
    case NO_MATCHING_ENTITLEMENT_KEY:
      *os << "NO_MATCHING_ENTITLEMENT_KEY";
      break;
    case LOAD_ENTITLED_CONTENT_KEYS_ERROR:
      *os << "LOAD_ENTITLED_CONTENT_KEYS_ERROR";
      break;
    case GET_PROVISIONING_METHOD_ERROR:
      *os << "GET_PROVISIONING_METHOD_ERROR";
      break;
    case SESSION_NOT_FOUND_17:
      *os << "SESSION_NOT_FOUND_17";
      break;
    case SESSION_NOT_FOUND_18:
      *os << "SESSION_NOT_FOUND_18";
      break;
    case SESSION_NOT_FOUND_19:
      *os << "SESSION_NOT_FOUND_19";
      break;
    case DEVICE_CANNOT_REPROVISION:
      *os << "DEVICE_CANNOT_REPROVISION";
      break;
    case SET_DECRYPT_HASH_ERROR:
      *os << "SET_DECRYPT_HASH_ERROR";
      break;
    case GET_DECRYPT_HASH_ERROR:
      *os << "GET_DECRYPT_HASH_ERROR";
      break;
    case SESSION_NOT_FOUND_20:
      *os << "SESSION_NOT_FOUND_20";
      break;
    case SESSION_NOT_FOUND_21:
      *os << "SESSION_NOT_FOUND_21";
      break;
    case INVALID_DECRYPT_HASH_FORMAT:
      *os << "INVALID_DECRYPT_HASH_FORMAT";
      break;
    case EMPTY_LICENSE_REQUEST_2:
      *os << "EMPTY_LICENSE_REQUEST_2";
      break;
    case EMPTY_LICENSE_REQUEST_3:
      *os << "EMPTY_LICENSE_REQUEST_3";
      break;
    case EMPTY_LICENSE_RESPONSE_3:
      *os << "EMPTY_LICENSE_RESPONSE_3";
      break;
    case EMPTY_LICENSE_RESPONSE_4:
      *os << "EMPTY_LICENSE_RESPONSE_4";
      break;
    case PARSE_REQUEST_ERROR_1:
      *os << "PARSE_REQUEST_ERROR_1";
      break;
    case PARSE_REQUEST_ERROR_2:
      *os << "PARSE_REQUEST_ERROR_2";
      break;
    case INVALID_LICENSE_REQUEST_TYPE_1:
      *os << "INVALID_LICENSE_REQUEST_TYPE_1";
      break;
    case INVALID_LICENSE_REQUEST_TYPE_2:
      *os << "INVALID_LICENSE_REQUEST_TYPE_2";
      break;
    case LICENSE_RESPONSE_PARSE_ERROR_4:
      *os << "LICENSE_RESPONSE_PARSE_ERROR_4";
      break;
    case LICENSE_RESPONSE_PARSE_ERROR_5:
      *os << "LICENSE_RESPONSE_PARSE_ERROR_5";
      break;
    case INVALID_LICENSE_TYPE_2:
      *os << "INVALID_LICENSE_TYPE_2";
      break;
    case SIGNATURE_NOT_FOUND_2:
      *os << "SIGNATURE_NOT_FOUND_2";
      break;
    case SESSION_KEYS_NOT_FOUND_2:
      *os << "SESSION_KEYS_NOT_FOUND_2";
      break;
    case REMOVE_OFFLINE_LICENSE_ERROR_1:
      *os << "REMOVE_OFFLINE_LICENSE_ERROR_1";
      break;
    case REMOVE_OFFLINE_LICENSE_ERROR_2:
      *os << "REMOVE_OFFLINE_LICENSE_ERROR_2";
      break;
    case OUTPUT_TOO_LARGE_ERROR:
      *os << "OUTPUT_TOO_LARGE_ERROR";
      break;
    case SESSION_LOST_STATE_ERROR:
      *os << "SESSION_LOST_STATE_ERROR";
      break;
    case GENERATE_DERIVED_KEYS_ERROR_2:
      *os << "GENERATE_DERIVED_KEYS_ERROR_2";
      break;
    case LOAD_DEVICE_RSA_KEY_ERROR:
      *os << "LOAD_DEVICE_RSA_KEY_ERROR";
      break;
    case NONCE_GENERATION_ERROR:
      *os << "NONCE_GENERATION_ERROR";
      break;
    case GENERATE_SIGNATURE_ERROR:
      *os << "GENERATE_SIGNATURE_ERROR";
      break;
    case UNKNOWN_CLIENT_TOKEN_TYPE:
      *os << "UNKNOWN_CLIENT_TOKEN_TYPE";
      break;
    case DEACTIVATE_USAGE_ENTRY_ERROR:
      *os << "DEACTIVATE_USAGE_ENTRY_ERROR";
      break;
    case SYSTEM_INVALIDATED_ERROR:
      *os << "SYSTEM_INVALIDATED_ERROR";
      break;
    case OPEN_CRYPTO_SESSION_ERROR:
      *os << "OPEN_CRYPTO_SESSION_ERROR";
      break;
    case LOAD_SRM_ERROR:
      *os << "LOAD_SRM_ERROR";
      break;
    case RANDOM_GENERATION_ERROR:
      *os << "RANDOM_GENERATION_ERROR";
      break;
    case CRYPTO_SESSION_NOT_INITIALIZED:
      *os << "CRYPTO_SESSION_NOT_INITIALIZED";
      break;
    case GET_DEVICE_ID_ERROR:
      *os << "GET_DEVICE_ID_ERROR";
      break;
    case GET_TOKEN_FROM_OEM_CERT_ERROR:
      *os << "GET_TOKEN_FROM_OEM_CERT_ERROR";
      break;
    case CRYPTO_SESSION_NOT_OPEN:
      *os << "CRYPTO_SESSION_NOT_OPEN";
      break;
    case GET_TOKEN_FROM_KEYBOX_ERROR:
      *os << "GET_TOKEN_FROM_KEYBOX_ERROR";
      break;
    case KEYBOX_TOKEN_TOO_SHORT:
      *os << "KEYBOX_TOKEN_TOO_SHORT";
      break;
    case EXTRACT_SYSTEM_ID_FROM_OEM_CERT_ERROR:
      *os << "EXTRACT_SYSTEM_ID_FROM_OEM_CERT_ERROR";
      break;
    case RSA_SIGNATURE_GENERATION_ERROR:
      *os << "RSA_SIGNATURE_GENERATION_ERROR";
      break;
    case GET_HDCP_CAPABILITY_FAILED:
      *os << "GET_HDCP_CAPABILITY_FAILED";
      break;
    case GET_NUMBER_OF_OPEN_SESSIONS_ERROR:
      *os << "GET_NUMBER_OF_OPEN_SESSIONS_ERROR";
      break;
    case GET_MAX_NUMBER_OF_OPEN_SESSIONS_ERROR:
      *os << "GET_MAX_NUMBER_OF_OPEN_SESSIONS_ERROR";
      break;
    case NOT_IMPLEMENTED_ERROR:
      *os << "NOT_IMPLEMENTED_ERROR";
      break;
    case GET_SRM_VERSION_ERROR:
      *os << "GET_SRM_VERSION_ERROR";
      break;
    case REWRAP_DEVICE_RSA_KEY_ERROR:
      *os << "REWRAP_DEVICE_RSA_KEY_ERROR";
      break;
    case REWRAP_DEVICE_RSA_KEY_30_ERROR:
      *os << "REWRAP_DEVICE_RSA_KEY_30_ERROR";
      break;
    case SERVICE_CERTIFICATE_PROVIDER_ID_EMPTY:
      *os << "SERVICE_CERTIFICATE_PROVIDER_ID_EMPTY";
      break;
    case INVALID_SRM_LIST:
      *os << "INVALID_SRM_LIST";
      break;
    default:
      *os << "Unknown CdmResponseType";
      break;
  }
}

void PrintTo(const enum CdmLicenseType& value, ::std::ostream* os) {
  switch (value) {
    case kLicenseTypeOffline:
      *os << "kLicenseTypeOffline";
      break;
    case kLicenseTypeStreaming:
      *os << "kLicenseTypeStreaming";
      break;
    case kLicenseTypeRelease:
      *os << "kLicenseTypeRelease";
      break;
    default:
      *os << "Unknown  CdmLicenseType";
      break;
  }
};

void PrintTo(const enum CdmSecurityLevel& value, ::std::ostream* os) {
  switch (value) {
    case kSecurityLevelUninitialized:
      *os << "kSecurityLevelUninitialized";
      break;
    case kSecurityLevelL1:
      *os << "kSecurityLevelL1";
      break;
    case kSecurityLevelL2:
      *os << "kSecurityLevelL2";
      break;
    case kSecurityLevelL3:
      *os << "kSecurityLevelL3";
      break;
    case kSecurityLevelUnknown:
      *os << "kSecurityLevelUnknown";
      break;
    default:
      *os << "Unknown CdmSecurityLevel";
      break;
  }
};

void PrintTo(const enum CdmCertificateType& value, ::std::ostream* os) {
  switch (value) {
    case kCertificateWidevine:
      *os << "kCertificateWidevine";
      break;
    case kCertificateX509:
      *os << "kCertificateX509";
      break;
    default:
      *os << "Unknown CdmCertificateType";
      break;
  }
};

}  // namespace wvcdm
