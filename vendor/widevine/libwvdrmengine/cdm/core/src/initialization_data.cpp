// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "initialization_data.h"

#include <string.h>
#include <sstream>

#include "buffer_reader.h"
#include "cdm_engine.h"
#include "jsmn.h"
#include "log.h"
#include "platform.h"
#include "properties.h"
#include "string_conversions.h"
#include "wv_cdm_constants.h"
#include "wv_cdm_types.h"

namespace {
const char kKeyFormatVersionsSeparator = '/';
const std::string kBase64String = "base64,";
const uint32_t kFourCcCbc1 = 0x63626331;
const uint32_t kFourCcCbcs = 0x63626373;

// json init data key values
const std::string kProvider = "provider";
const std::string kContentId = "content_id";
const std::string kKeyIds = "key_ids";

// Being conservative, usually we expect 6 + number of Key Ids
const int kDefaultNumJsonTokens = 128;

// Widevine's registered system ID.
const uint8_t kWidevineSystemId[] = {
    0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6, 0x4A, 0xCE,
    0xA3, 0xC8, 0x27, 0xDC, 0xD5, 0x1D, 0x21, 0xED,
};
}  // namespace

namespace wvcdm {

// Protobuf generated classes.
using video_widevine::WidevinePsshData;
using video_widevine::WidevinePsshData_Algorithm;
using video_widevine::WidevinePsshData_Algorithm_AESCTR;
using video_widevine::WidevinePsshData_Type;
using video_widevine::WidevinePsshData_Type_ENTITLED_KEY;
using video_widevine::WidevinePsshData_Type_SINGLE;

InitializationData::InitializationData(const std::string& type,
                                       const CdmInitData& data,
                                       const std::string& oec_version)
    : type_(type),
      is_cenc_(false),
      is_hls_(false),
      is_webm_(false),
      is_audio_(false),
      contains_entitled_keys_(false),
      hls_method_(kHlsMethodNone) {
  if (type == ISO_BMFF_VIDEO_MIME_TYPE || type == ISO_BMFF_AUDIO_MIME_TYPE ||
      type == CENC_INIT_DATA_FORMAT) {
    is_cenc_ = true;
  } else if (type == WEBM_VIDEO_MIME_TYPE || type == WEBM_AUDIO_MIME_TYPE ||
             type == WEBM_INIT_DATA_FORMAT) {
    is_webm_ = true;
  } else if (type == HLS_INIT_DATA_FORMAT) {
    is_hls_ = true;
  }
  if (type == ISO_BMFF_AUDIO_MIME_TYPE || type == WEBM_AUDIO_MIME_TYPE) {
    is_audio_ = true;
  }

  if (data.size() && is_supported()) {
    if (is_cenc()) {
      bool oec_prefers_entitlements = DetectEntitlementPreference(oec_version);
      if (!SelectWidevinePssh(data, oec_prefers_entitlements, &data_)) {
        LOGE(
            "InitializationData: Unable to select a supported Widevine PSSH "
            "from the init data.");
      }
    } else if (is_webm()) {
      data_ = data;
    } else if (is_hls()) {
      std::string uri;
      if (ExtractHlsAttributes(data, &hls_method_, &hls_iv_, &uri)) {
        ConstructWidevineInitData(hls_method_, uri, &data_);
      }
    }
  }
}

std::vector<video_widevine::WidevinePsshData_EntitledKey>
InitializationData::ExtractWrappedKeys() const {
  std::vector<video_widevine::WidevinePsshData_EntitledKey> keys;
  WidevinePsshData cenc_header;
  if (!is_cenc_ || !cenc_header.ParseFromString(data_) ||
      cenc_header.entitled_keys().size() == 0)
    return keys;
  keys.reserve(cenc_header.entitled_keys().size());
  for (int i = 0; i < cenc_header.entitled_keys().size(); ++i) {
    keys.push_back(cenc_header.entitled_keys(i));
  }
  return keys;
}

// Parse a blob of multiple concatenated PSSH atoms to extract the Widevine
// PSSH. The blob may have multiple Widevine PSSHs. Devices that prefer
// entitlements should choose the |ENTITLED_KEY| PSSH while other devices should
// stick to the |SINGLE| PSSH.
bool InitializationData::SelectWidevinePssh(const CdmInitData& init_data,
                                            bool prefer_entitlements,
                                            CdmInitData* output) {
  // Extract the data payloads from the Widevine PSSHs.
  std::vector<CdmInitData> pssh_payloads;
  if (!ExtractWidevinePsshs(init_data, &pssh_payloads)) {
    LOGE(
        "InitializationData::SelectWidevinePssh: Unable to parse concatenated "
        "PSSH boxes.");
    return false;
  }
  if (pssh_payloads.empty()) {
    LOGE(
        "InitializationData::SelectWidevinePssh: The concatenated PSSH boxes "
        "could be parsed, but no Widevine PSSH was found.");
    return false;
  }

  // If this device prefers entitlements, search through available PSSHs.
  // If present, select the first |ENTITLED_KEY| PSSH.
  if (prefer_entitlements && !pssh_payloads.empty()) {
    for (size_t i = 0; i < pssh_payloads.size(); ++i) {
      WidevinePsshData pssh;
      if (!pssh.ParseFromString(pssh_payloads[i])) {
        LOGE(
            "InitializationData::SelectWidevinePssh: Unable to parse PSSH data "
            "%lu into a protobuf.", i);
        continue;
      }
      if (pssh.type() == WidevinePsshData_Type_ENTITLED_KEY) {
        contains_entitled_keys_ = true;
        *output = pssh_payloads[i];
        return true;
      }
    }
  }

  // Either there is only one PSSH, this device does not prefer entitlements,
  // or no entitlement PSSH was found. Regardless of how we got here, select the
  // first PSSH, which should be a |SINGLE| PSSH.
  *output = pssh_payloads[0];
  return true;
}

// one PSSH box consists of:
// 4 byte size of the atom, inclusive.  (0 means the rest of the buffer.)
// 4 byte atom type, "pssh".
// (optional, if size == 1) 8 byte size of the atom, inclusive.
// 1 byte version, value 0 or 1.  (skip if larger.)
// 3 byte flags, value 0.  (ignored.)
// 16 byte system id.
// (optional, if version == 1) 4 byte key ID count. (K)
// (optional, if version == 1) K * 16 byte key ID.
// 4 byte size of PSSH data, exclusive. (N)
// N byte PSSH data.

bool InitializationData::ExtractWidevinePsshs(
  const CdmInitData& init_data, std::vector<CdmInitData>* psshs) {
  if (psshs == NULL) {
    LOGE("InitializationData::ExtractWidevinePsshs: NULL psshs parameter");
    return false;
  }
  psshs->clear();
  psshs->reserve(2);  // We expect 1 or 2 Widevine PSSHs

  const uint8_t* data_start = reinterpret_cast<const uint8_t*>(init_data.data());
  BufferReader reader(data_start, init_data.length());

  while (!reader.IsEOF()) {
    const size_t start_pos = reader.pos();

    // Atom size. Used for bounding the inner reader and knowing how far to skip
    // forward after parsing this PSSH.
    uint64_t size;
    if (!reader.Read4Into8(&size)) {
      LOGV(
          "InitializationData::ExtractWidevinePsshs: Unable to read the "
          "32-bit atom size.");
      return false;  // We cannot continue reading safely. Abort.
    }

    // Skip the atom type for now.
    if (!reader.SkipBytes(4)) {
      LOGV(
          "InitializationData::ExtractWidevinePsshs: Unable to skip the "
          "atom type.");
      return false;  // We cannot continue reading safely. Abort.
    }

    if (size == 1) {
      // An "atom size" of 1 means the real atom size is a 64-bit number stored
      // after the atom type.
      if (!reader.Read8(&size)) {
        LOGV(
            "InitializationData::ExtractWidevinePsshs: Unable to read the "
            "64-bit atom size.");
        return false;  // We cannot continue reading safely. Abort.
      }
    } else if (size == 0) {
      // An "atom size" of 0 means the atom takes up the rest of the buffer.
      size = reader.size() - start_pos;
    }

    const size_t bytes_read = reader.pos() - start_pos;
    const size_t bytes_remaining = size - bytes_read;

    if (!reader.HasBytes(bytes_remaining)) {
      LOGV(
          "InitializationData::ExtractWidevinePsshs: Invalid atom size. The "
          "atom claims to be larger than the remaining init data.");
      return false;  // We cannot continue reading safely. Abort.
    }

    // Use ExtractWidevinePsshData() to check if this atom is a Widevine PSSH
    // and, if so, add its data to the list of PSSHs.
    CdmInitData data;
    if (ExtractWidevinePsshData(data_start + start_pos, size, &data)) {
      psshs->push_back(data);
    }

    // Skip past the rest of the atom.
    if (!reader.SkipBytes(bytes_remaining)) {
      LOGV(
          "InitializationData::LocateWidevinePsshOffsets: Unable to skip the "
          "rest of the atom.");
      return false;  // We cannot continue reading safely. Abort.
    }
  }

  return true;
}

// Checks if the given reader contains a Widevine PSSH. If so, it extracts the
// data from the box. Returns true if a PSSH was extracted, false if there was
// an error or if the data is not a Widevine PSSH.
bool InitializationData::ExtractWidevinePsshData(
    const uint8_t* data, size_t length, CdmInitData* output) {
  BufferReader reader(data, length);

  // Read the 32-bit size only so we can check if we need to expect a 64-bit
  // size.
  uint64_t size_32;
  if (!reader.Read4Into8(&size_32)) {
    LOGV(
        "InitializationData::ExtractWidevinePsshData: Unable to read the "
        "32-bit atom size.");
    return false;
  }
  const bool has_size_64 = (size_32 == 1);

  // Read the atom type and check that it is "pssh".
  std::vector<uint8_t> atom_type;
  if (!reader.ReadVec(&atom_type, 4)) {
    LOGV(
        "InitializationData::ExtractWidevinePsshData: Unable to read the atom "
        "type.");
    return false;
  }
  if (memcmp(&atom_type[0], "pssh", 4) != 0) {
    LOGV(
        "InitializationData::ExtractWidevinePsshData: Atom type is not PSSH.");
    return false;
  }

  // If there is a 64-bit size, skip it.
  if (has_size_64) {
    if (!reader.SkipBytes(8)) {
      LOGV(
          "InitializationData::ExtractWidevinePsshData: Unable to skip the "
          "64-bit atom size.");
      return false;
    }
  }

  // Read the version number and abort if it is not one we can handle.
  uint8_t version;
  if (!reader.Read1(&version)) {
    LOGV(
        "InitializationData::ExtractWidevinePsshData: Unable to read the PSSH "
        "version.");
    return false;
  }
  if (version > 1) {
    LOGV(
        "InitializationData::ExtractWidevinePsshData: Unrecognized PSSH "
        "version.");
    return false;
  }

  // Skip the flags.
  if (!reader.SkipBytes(3)) {
    LOGV(
        "InitializationData::ExtractWidevinePsshData: Unable to skip the PSSH "
        "flags.");
    return false;
  }

  // Read the System ID and validate that it is the Widevine System ID.
  std::vector<uint8_t> system_id;
  if (!reader.ReadVec(&system_id, sizeof(kWidevineSystemId))) {
    LOGV(
        "InitializationData::ExtractWidevinePsshData: Unable to read the "
        "system ID.");
    return false;
  }
  if (memcmp(&system_id[0],
             kWidevineSystemId,
             sizeof(kWidevineSystemId)) != 0) {
    LOGV(
        "InitializationData::ExtractWidevinePsshData: Found a non-Widevine "
        "PSSH.");
    return false;
  }

  // Skip the key ID fields, if present.
  if (version == 1) {
    // Read the number of key IDs so we know how far to skip ahead.
    uint32_t num_key_ids;
    if (!reader.Read4(&num_key_ids)) {
      LOGV(
          "InitializationData::ExtractWidevinePsshData: Unable to read the "
          "number of key IDs.");
      return false;
    }

    // Skip the key IDs.
    if (!reader.SkipBytes(num_key_ids * 16)) {
      LOGV(
          "InitializationData::ExtractWidevinePsshData: Unable to skip the key "
          "IDs.");
      return false;
    }
  }

  // Read the size of the PSSH data.
  uint32_t data_length;
  if (!reader.Read4(&data_length)) {
    LOGV(
        "InitializationData::ExtractWidevinePsshData: Unable to read the PSSH "
        "data size.");
    return false;
  }

  // Read the PSSH data.
  output->clear();
  if (!reader.ReadString(output, data_length)) {
    LOGV(
        "InitializationData::ExtractWidevinePsshData: Unable to read the PSSH "
        "data.");
    return false;
  }

  return true;
}

// Parse an EXT-X-KEY tag attribute list. Verify that Widevine supports it
// by validating KEYFORMAT and KEYFORMATVERSION attributes. Extract out
// method, IV, URI and WV init data.
//
// An example of a widevine supported attribute list from an HLS media playlist
// is,
//    "EXT-X-KEY: METHOD=SAMPLE-AES, \"
//    "URI=”data:text/plain;base64,eyANCiAgICJwcm92aWRlciI6Im1sYmFtaGJvIiwNCiAg"
//         "ICJjb250ZW50X2lkIjoiMjAxNV9UZWFycyIsDQogICAia2V5X2lkcyI6DQogICBbDQo"
//         "gICAgICAiMzcxZTEzNWUxYTk4NWQ3NWQxOThhN2Y0MTAyMGRjMjMiDQogICBdDQp9DQ"
//         "o=, \"
//    "IV=0x6df49213a781e338628d0e9c812d328e, \"
//    "KEYFORMAT=”com.widevine”, \"
//    "KEYFORMATVERSIONS=”1”"
bool InitializationData::ExtractHlsAttributes(const std::string& attribute_list,
                                              CdmHlsMethod* method,
                                              std::vector<uint8_t>* iv,
                                              std::string* uri) {
  std::string value;
  if (!ExtractQuotedAttribute(attribute_list, HLS_KEYFORMAT_ATTRIBUTE,
                              &value)) {
    LOGV(
        "InitializationData::ExtractHlsInitDataAtttribute: Unable to read HLS "
        "keyformat value");
    return false;
  }

  if (value.compare(0, sizeof(KEY_SYSTEM) - 1, KEY_SYSTEM) != 0) {
    LOGV(
        "InitializationData::ExtractHlsInitDataAtttribute: unrecognized HLS "
        "keyformat value: %s",
        value.c_str());
    return false;
  }

  // KEYFORMATVERSIONS is an optional parameter. If absent its
  // value defaults to "1"
  if (ExtractQuotedAttribute(attribute_list, HLS_KEYFORMAT_VERSIONS_ATTRIBUTE,
                             &value)) {
    std::vector<std::string> versions = ExtractKeyFormatVersions(value);
    bool supported = false;
    for (size_t i = 0; i < versions.size(); ++i) {
      if (versions[i].compare(HLS_KEYFORMAT_VERSION_VALUE_1) == 0) {
        supported = true;
        break;
      }
    }
    if (!supported) {
      LOGV(
          "InitializationData::ExtractHlsInitDataAtttribute: HLS keyformat "
          "version is not supported: %s",
          value.c_str());
      return false;
    }
  }

  if (!ExtractAttribute(attribute_list, HLS_METHOD_ATTRIBUTE, &value)) {
    LOGV(
        "InitializationData::ExtractHlsInitDataAtttribute: Unable to read HLS "
        "method");
    return false;
  }

  if (value.compare(HLS_METHOD_AES_128) == 0) {
    *method = kHlsMethodAes128;
  } else if (value.compare(HLS_METHOD_SAMPLE_AES) == 0) {
    *method = kHlsMethodSampleAes;
  } else if (value.compare(HLS_METHOD_NONE) == 0) {
    *method = kHlsMethodNone;
  } else {
    LOGV(
        "InitializationData::ExtractHlsInitDataAtttribute: HLS method "
        "unrecognized: %s",
        value.c_str());
    return false;
  }

  if (!ExtractHexAttribute(attribute_list, HLS_IV_ATTRIBUTE, iv)) {
    LOGV(
        "InitializationData::ExtractHlsInitDataAtttribute: HLS IV attribute "
        "not present");
    return false;
  }

  if (!ExtractQuotedAttribute(attribute_list, HLS_URI_ATTRIBUTE, uri)) {
    LOGV(
        "InitializationData::ExtractHlsInitDataAtttribute: HLS URI attribute "
        "not present");
    return false;
  }

  return true;
}

// Extracts a base64 encoded string from URI data. This is then base64 decoded
// and the Json formatted init data is then parsed. The information is used
// to generate a Widevine init data protobuf (WidevineCencHeader).
//
// An example of a widevine supported json formatted init data string is,
//
// {
//   "provider":"mlbamhbo",
//   "content_id":"MjAxNV9UZWFycw==",
//   "key_ids":
//   [
//     "371e135e1a985d75d198a7f41020dc23"
//   ]
// }
bool InitializationData::ConstructWidevineInitData(
    CdmHlsMethod method, const std::string& uri, CdmInitData* init_data_proto) {
  if (!init_data_proto) {
    LOGV("InitializationData::ConstructWidevineInitData: Invalid parameter");
    return false;
  }
  if (method != kHlsMethodAes128 && method != kHlsMethodSampleAes) {
    LOGV(
        "InitializationData::ConstructWidevineInitData: Invalid method"
        " parameter");
    return false;
  }

  size_t pos = uri.find(kBase64String);
  if (pos == std::string::npos) {
    LOGV(
        "InitializationData::ConstructWidevineInitData: URI attribute "
        "unexpected format: %s",
        uri.c_str());
    return false;
  }

  std::vector<uint8_t> json_init_data =
      Base64Decode(uri.substr(pos + kBase64String.size()));
  if (json_init_data.size() == 0) {
    LOGV(
        "InitializationData::ConstructWidevineInitData: Base64 decode of json "
        "data failed");
    return false;
  }
  std::string json_string((const char*)(&json_init_data[0]),
                          json_init_data.size());

  // Parse the Json string using jsmn
  jsmn_parser parser;
  jsmntok_t tokens[kDefaultNumJsonTokens];
  jsmn_init(&parser);
  int num_of_tokens =
      jsmn_parse(&parser, json_string.c_str(), json_string.size(), tokens,
                 kDefaultNumJsonTokens);

  if (num_of_tokens <= 0) {
    LOGV(
        "InitializationData::ConstructWidevineInitData: Json parsing failed: "
        "%d",
        num_of_tokens);
    return false;
  }

  std::string provider;
  std::string content_id;
  std::vector<std::string> key_ids;

  enum JsmnParserState {
    kParseState,
    kProviderState,
    kContentIdState,
    kKeyIdsState,
  } state = kParseState;

  int number_of_key_ids = 0;

  // Extract the provider, content_id and key_ids
  for (int i = 0; i < num_of_tokens; ++i) {
    if (tokens[i].start < 0 || tokens[i].end < 0) {
      LOGV(
          "InitializationData::ConstructWidevineInitData: Invalid start or end "
          "of token");
      return false;
    }

    switch (state) {
      case kParseState:
        if (tokens[i].type == JSMN_STRING) {
          std::string token(json_string, tokens[i].start,
                            tokens[i].end - tokens[i].start);
          if (token == kProvider) {
            state = kProviderState;
          } else if (token == kContentId) {
            state = kContentIdState;
          } else if (token == kKeyIds) {
            state = kKeyIdsState;
          }
        }
        break;
      case kProviderState:
        if (tokens[i].type == JSMN_STRING) {
          provider.assign(json_string, tokens[i].start,
                          tokens[i].end - tokens[i].start);
        }
        state = kParseState;
        break;
      case kContentIdState:
        if (tokens[i].type == JSMN_STRING) {
          std::string base64_content_id(json_string, tokens[i].start,
                                        tokens[i].end - tokens[i].start);
          std::vector<uint8_t> content_id_data =
              Base64Decode(base64_content_id);
          content_id.assign(reinterpret_cast<const char*>(&content_id_data[0]),
                            content_id_data.size());
        }
        state = kParseState;
        break;
      case kKeyIdsState:
        if (tokens[i].type == JSMN_ARRAY) {
          number_of_key_ids = tokens[i].size;
        } else if (tokens[i].type == JSMN_STRING) {
          std::string key_id(a2bs_hex(json_string.substr(
              tokens[i].start, tokens[i].end - tokens[i].start)));
          if (key_id.size() == 16) key_ids.push_back(key_id);
          --number_of_key_ids;
        } else {
          state = kParseState;
        }
        if (number_of_key_ids <= 0) state = kParseState;
        break;
    }
  }

  if (provider.size() == 0) {
    LOGV("InitializationData::ConstructWidevineInitData: Invalid provider");
    return false;
  }

  if (content_id.size() == 0) {
    LOGV("InitializationData::ConstructWidevineInitData: Invalid content_id");
    return false;
  }

  if (key_ids.size() == 0) {
    LOGV("InitializationData::ConstructWidevineInitData: No key_ids present");
    return false;
  }

  // Now format as Widevine init data protobuf
  WidevinePsshData cenc_header;
  // TODO(rfrias): The algorithm is a deprecated field, but proto changes
  // have not yet been pushed to production. Set until then.
  cenc_header.set_algorithm(WidevinePsshData_Algorithm_AESCTR);
  for (size_t i = 0; i < key_ids.size(); ++i) {
    cenc_header.add_key_ids(key_ids[i]);
  }
  cenc_header.set_provider(provider);
  cenc_header.set_content_id(content_id);
  if (method == kHlsMethodAes128)
    cenc_header.set_protection_scheme(kFourCcCbc1);
  else
    cenc_header.set_protection_scheme(kFourCcCbcs);
  cenc_header.SerializeToString(init_data_proto);
  return true;
}

bool InitializationData::ExtractQuotedAttribute(
    const std::string& attribute_list, const std::string& key,
    std::string* value) {
  bool result = ExtractAttribute(attribute_list, key, value);
  if (value->size() < 2 || value->at(0) != '\"' ||
      value->at(value->size() - 1) != '\"')
    return false;
  *value = value->substr(1, value->size() - 2);
  if (value->find('\"') != std::string::npos) return false;
  return result;
}

bool InitializationData::ExtractHexAttribute(const std::string& attribute_list,
                                             const std::string& key,
                                             std::vector<uint8_t>* value) {
  std::string val;
  bool result = ExtractAttribute(attribute_list, key, &val);
  if (!result || val.size() <= 2 || val.size() % 2 != 0 || val[0] != '0' ||
      ((val[1] != 'x') && (val[1] != 'X')))
    return false;
  for (size_t i = 2; i < val.size(); ++i) {
    if (!isxdigit(val[i])) return false;
  }
  *value = a2b_hex(val.substr(2, val.size() - 2));
  return result;
}

bool InitializationData::ExtractAttribute(const std::string& attribute_list,
                                          const std::string& key,
                                          std::string* value) {
  // validate the key
  for (size_t i = 0; i < key.size(); ++i)
    if (!isupper(key[i]) && !isdigit(key[i]) && key[i] != '-') return false;

  bool found = false;
  size_t pos = 0;
  // Find the key followed by '='
  while (!found) {
    pos = attribute_list.find(key, pos);
    if (pos == std::string::npos) return false;
    pos += key.size();
    if (attribute_list[pos] != '=') continue;
    found = true;
  }

  if (attribute_list.size() <= ++pos) return false;

  size_t end_pos = pos;
  found = false;
  // Find the next comma outside the quote or end of string
  while (!found) {
    end_pos = attribute_list.find(',', end_pos);
    if (end_pos != std::string::npos && attribute_list[pos] == '\"' &&
        attribute_list[end_pos - 1] != '\"') {
      ++end_pos;
      continue;
    }

    if (end_pos == std::string::npos)
      end_pos = attribute_list.size() - 1;
    else
      --end_pos;
    found = true;
  }

  *value = attribute_list.substr(pos, end_pos - pos + 1);

  // validate the value
  for (size_t i = 0; i < value->size(); ++i)
    if (!isgraph(value->at(i))) return false;

  return true;
}

// Key format versions are individual values or multiple versions
// separated by '/'. "1" or "1/2/5"
std::vector<std::string> InitializationData::ExtractKeyFormatVersions(
    const std::string& key_format_versions) {
  std::vector<std::string> versions;
  size_t pos = 0;
  while (pos < key_format_versions.size()) {
    size_t next_pos =
        key_format_versions.find(kKeyFormatVersionsSeparator, pos);
    if (next_pos == std::string::npos) {
      versions.push_back(key_format_versions.substr(pos));
      break;
    } else {
      versions.push_back(key_format_versions.substr(pos, next_pos - pos));
      pos = next_pos + 1;
    }
  }
  return versions;
}

bool InitializationData::DetectEntitlementPreference(
    const std::string& oec_version_string) {
  if (oec_version_string.empty()) {
    return false;
  }

  uint32_t oec_version_int = 0;
  std::istringstream parse_int;
  parse_int.str(oec_version_string);
  parse_int >> oec_version_int;

  return oec_version_int >= 14;
}

}  // namespace wvcdm
