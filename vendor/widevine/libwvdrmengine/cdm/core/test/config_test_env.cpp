// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include "config_test_env.h"
#include "string_conversions.h"

// Holds the data needed to talk to the various provisioning and
// license servers.
//
// Define a series of configurations, and specify the specific
// data items needed for that configuration.

namespace wvcdm {

namespace {

const std::string kWidevineKeySystem = "com.widevine.alpha";

// -----------------------------------------------------------------------------
// Below are two choices for provisioning servers: production and staging.
// -----------------------------------------------------------------------------

// Production Provisioning Server
const std::string kCpProductionProvisioningServerUrl =
    "https://www.googleapis.com/"
    "certificateprovisioning/v1/devicecertificates/create"
    "?key=AIzaSyB-5OLKTx2iU5mko18DfdwK5611JIjbUhE";
// NOTE: Provider ID = widevine.com
const std::string kCpProductionProvisioningServiceCertificate =
    "0ab9020803121051434fe2a44c763bcc2c826a2d6ef9a718f7d793d005228e02"
    "3082010a02820101009e27088659dbd9126bc6ed594caf652b0eaab82abb9862"
    "ada1ee6d2cb5247e94b28973fef5a3e11b57d0b0872c930f351b5694354a8c77"
    "ed4ee69834d2630372b5331c5710f38bdbb1ec3024cfadb2a8ac94d977d391b7"
    "d87c20c5c046e9801a9bffaf49a36a9ee6c5163eff5cdb63bfc750cf4a218618"
    "984e485e23a10f08587ec5d990e9ab0de71460dfc334925f3fb9b55761c61e28"
    "8398c387a0925b6e4dcaa1b36228d9feff7e789ba6e5ef6cf3d97e6ae05525db"
    "38f826e829e9b8764c9e2c44530efe6943df4e048c3c5900ca2042c5235dc80d"
    "443789e734bf8e59a55804030061ed48e7d139b521fbf35524b3000b3e2f6de0"
    "001f5eeb99e9ec635f02030100013a0c7769646576696e652e636f6d12800332"
    "2c2f3fedc47f8b7ba88a135a355466e378ed56a6fc29ce21f0cafc7fb253b073"
    "c55bed253d8650735417aad02afaefbe8d5687902b56a164490d83d590947515"
    "68860e7200994d322b5de07f82ef98204348a6c2c9619092340eb87df26f63bf"
    "56c191dc069b80119eb3060d771afaaeb2d30b9da399ef8a41d16f45fd121e09"
    "a0c5144da8f8eb46652c727225537ad65e2a6a55799909bbfb5f45b5775a1d1e"
    "ac4e06116c57adfa9ce0672f19b70b876f88e8b9fbc4f96ccc500c676cfb173c"
    "b6f52601573e2e45af1d9d2a17ef1487348c05cfc6d638ec2cae3fadb655e943"
    "1330a75d2ceeaa54803e371425111e20248b334a3a50c8eca683c448b8ac402c"
    "76e6f76e2751fbefb669f05703cec8c64cf7a62908d5fb870375eb0cc96c508e"
    "26e0c050f3fd3ebe68cef9903ef6405b25fc6e31f93559fcff05657662b3653a"
    "8598ed5751b38694419242a875d9e00d5a5832933024b934859ec8be78adccbb"
    "1ec7127ae9afeef9c5cd2e15bd3048e8ce652f7d8c5d595a0323238c598a28";

// Staging Provisioning Server
const std::string kCpStagingProvisioningServerUrl =
      "https://staging-www.sandbox.googleapis.com/"
      "certificateprovisioning/v1/devicecertificates/create"
      "?key=AIzaSyB-5OLKTx2iU5mko18DfdwK5611JIjbUhE";
// NOTE: This is currently the same as the Production Provisioning Service Cert.
// NOTE: Provider ID = widevine.com
const std::string kCpStagingProvisioningServiceCertificate =
    "0ab9020803121051434fe2a44c763bcc2c826a2d6ef9a718f7d793d005228e02"
    "3082010a02820101009e27088659dbd9126bc6ed594caf652b0eaab82abb9862"
    "ada1ee6d2cb5247e94b28973fef5a3e11b57d0b0872c930f351b5694354a8c77"
    "ed4ee69834d2630372b5331c5710f38bdbb1ec3024cfadb2a8ac94d977d391b7"
    "d87c20c5c046e9801a9bffaf49a36a9ee6c5163eff5cdb63bfc750cf4a218618"
    "984e485e23a10f08587ec5d990e9ab0de71460dfc334925f3fb9b55761c61e28"
    "8398c387a0925b6e4dcaa1b36228d9feff7e789ba6e5ef6cf3d97e6ae05525db"
    "38f826e829e9b8764c9e2c44530efe6943df4e048c3c5900ca2042c5235dc80d"
    "443789e734bf8e59a55804030061ed48e7d139b521fbf35524b3000b3e2f6de0"
    "001f5eeb99e9ec635f02030100013a0c7769646576696e652e636f6d12800332"
    "2c2f3fedc47f8b7ba88a135a355466e378ed56a6fc29ce21f0cafc7fb253b073"
    "c55bed253d8650735417aad02afaefbe8d5687902b56a164490d83d590947515"
    "68860e7200994d322b5de07f82ef98204348a6c2c9619092340eb87df26f63bf"
    "56c191dc069b80119eb3060d771afaaeb2d30b9da399ef8a41d16f45fd121e09"
    "a0c5144da8f8eb46652c727225537ad65e2a6a55799909bbfb5f45b5775a1d1e"
    "ac4e06116c57adfa9ce0672f19b70b876f88e8b9fbc4f96ccc500c676cfb173c"
    "b6f52601573e2e45af1d9d2a17ef1487348c05cfc6d638ec2cae3fadb655e943"
    "1330a75d2ceeaa54803e371425111e20248b334a3a50c8eca683c448b8ac402c"
    "76e6f76e2751fbefb669f05703cec8c64cf7a62908d5fb870375eb0cc96c508e"
    "26e0c050f3fd3ebe68cef9903ef6405b25fc6e31f93559fcff05657662b3653a"
    "8598ed5751b38694419242a875d9e00d5a5832933024b934859ec8be78adccbb"
    "1ec7127ae9afeef9c5cd2e15bd3048e8ce652f7d8c5d595a0323238c598a28";

// -----------------------------------------------------------------------------
// Below are several choices for licenseing servers: production, UAT, staging
// and staging and maybe Google Play.  We haven't tested with Google Play in a
// long time.
// -----------------------------------------------------------------------------

// Content Protection license server (Production) data
// Testing should not be directed at this server.
const std::string kCpProductionLicenseServer =
    "https://widevine-proxy.appspot.com/proxy";
// NOTE: Provider ID = staging.google.com
const std::string kCpProductionServiceCertificate =
    "0ABF020803121028703454C008F63618ADE7443DB6C4C8188BE7F9900522"
    "8E023082010A0282010100B52112B8D05D023FCC5D95E2C251C1C649B417"
    "7CD8D2BEEF355BB06743DE661E3D2ABC3182B79946D55FDC08DFE9540781"
    "5E9A6274B322A2C7F5E067BB5F0AC07A89D45AEA94B2516F075B66EF811D"
    "0D26E1B9A6B894F2B9857962AA171C4F66630D3E4C602718897F5E1EF9B6"
    "AAF5AD4DBA2A7E14176DF134A1D3185B5A218AC05A4C41F081EFFF80A3A0"
    "40C50B09BBC740EEDCD8F14D675A91980F92CA7DDC646A06ADAD5101F74A"
    "0E498CC01F00532BAC217850BD905E90923656B7DFEFEF42486767F33EF6"
    "283D4F4254AB72589390BEE55808F1D668080D45D893C2BCA2F74D60A0C0"
    "D0A0993CEF01604703334C3638139486BC9DAF24FD67A07F9AD943020301"
    "00013A1273746167696E672E676F6F676C652E636F6D128003983E303526"
    "75F40BA715FC249BDAE5D4AC7249A2666521E43655739529721FF880E0AA"
    "EFC5E27BC980DAEADABF3FC386D084A02C82537848CC753FF497B011A7DA"
    "97788A00E2AA6B84CD7D71C07A48EBF61602CCA5A3F32030A7295C30DA91"
    "5B91DC18B9BC9593B8DE8BB50F0DEDC12938B8E9E039CDDE18FA82E81BB0"
    "32630FE955D85A566CE154300BF6D4C1BD126966356B287D657B18CE63D0"
    "EFD45FC5269E97EAB11CB563E55643B26FF49F109C2101AFCAF35B832F28"
    "8F0D9D45960E259E85FB5D24DBD2CF82764C5DD9BF727EFBE9C861F86932"
    "1F6ADE18905F4D92F9A6DA6536DB8475871D168E870BB2303CF70C6E9784"
    "C93D2DE845AD8262BE7E0D4E2E4A0759CEF82D109D2592C72429F8C01742"
    "BAE2B3DECADBC33C3E5F4BAF5E16ECB74EADBAFCB7C6705F7A9E3B6F3940"
    "383F9C5116D202A20C9229EE969C2519718303B50D0130C3352E06B014D8"
    "38540F8A0C227C0011E0F5B38E4E298ED2CB301EB4564965F55C5D79757A"
    "250A4EB9C84AB3E6539F6B6FDF56899EA29914";

// Content Protection license server (UAT) data
// Testing should be directed to this server. The only exception
// is when testing against a new server feature that is not yet
// deployed to this server (which would be directed to staging).
const std::string kCpUatLicenseServer =
    "https://proxy.uat.widevine.com/proxy";
// NOTE: Provider ID = staging.google.com
const std::string kCpUatServiceCertificate =
    "0ABF020803121028703454C008F63618ADE7443DB6C4C8188BE7F9900522"
    "8E023082010A0282010100B52112B8D05D023FCC5D95E2C251C1C649B417"
    "7CD8D2BEEF355BB06743DE661E3D2ABC3182B79946D55FDC08DFE9540781"
    "5E9A6274B322A2C7F5E067BB5F0AC07A89D45AEA94B2516F075B66EF811D"
    "0D26E1B9A6B894F2B9857962AA171C4F66630D3E4C602718897F5E1EF9B6"
    "AAF5AD4DBA2A7E14176DF134A1D3185B5A218AC05A4C41F081EFFF80A3A0"
    "40C50B09BBC740EEDCD8F14D675A91980F92CA7DDC646A06ADAD5101F74A"
    "0E498CC01F00532BAC217850BD905E90923656B7DFEFEF42486767F33EF6"
    "283D4F4254AB72589390BEE55808F1D668080D45D893C2BCA2F74D60A0C0"
    "D0A0993CEF01604703334C3638139486BC9DAF24FD67A07F9AD943020301"
    "00013A1273746167696E672E676F6F676C652E636F6D128003983E303526"
    "75F40BA715FC249BDAE5D4AC7249A2666521E43655739529721FF880E0AA"
    "EFC5E27BC980DAEADABF3FC386D084A02C82537848CC753FF497B011A7DA"
    "97788A00E2AA6B84CD7D71C07A48EBF61602CCA5A3F32030A7295C30DA91"
    "5B91DC18B9BC9593B8DE8BB50F0DEDC12938B8E9E039CDDE18FA82E81BB0"
    "32630FE955D85A566CE154300BF6D4C1BD126966356B287D657B18CE63D0"
    "EFD45FC5269E97EAB11CB563E55643B26FF49F109C2101AFCAF35B832F28"
    "8F0D9D45960E259E85FB5D24DBD2CF82764C5DD9BF727EFBE9C861F86932"
    "1F6ADE18905F4D92F9A6DA6536DB8475871D168E870BB2303CF70C6E9784"
    "C93D2DE845AD8262BE7E0D4E2E4A0759CEF82D109D2592C72429F8C01742"
    "BAE2B3DECADBC33C3E5F4BAF5E16ECB74EADBAFCB7C6705F7A9E3B6F3940"
    "383F9C5116D202A20C9229EE969C2519718303B50D0130C3352E06B014D8"
    "38540F8A0C227C0011E0F5B38E4E298ED2CB301EB4564965F55C5D79757A"
    "250A4EB9C84AB3E6539F6B6FDF56899EA29914";
const std::string kCpUatProvisioningServerUrl =
    kCpProductionProvisioningServerUrl;
const std::string kCpUatProvisioningServiceCertificate =
    kCpProductionServiceCertificate;
const std::string kCpClientAuth = "";
const std::string kCpKeyId =
    "00000042"                          // blob size
    "70737368"                          // "pssh"
    "00000000"                          // flags
    "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
    "00000022"                          // pssh data size
    // pssh data:
    "08011a0d7769646576696e655f746573"  // "streaming_clip1"
    "74220f73747265616d696e675f636c69"
    "7031";
const std::string kCpOfflineKeyId =
    "00000040"                          // blob size
    "70737368"                          // "pssh"
    "00000000"                          // flags
    "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
    "00000020"                          // pssh data size
    // pssh data:
    "08011a0d7769646576696e655f746573"  // "offline_clip2"
    "74220d6f66666c696e655f636c697032";

// Content Protection license server (staging) data
// The staging server should be used only when testing against
// a new server. (e.g., the client has a change that requires a
// corresponding change to the server, but the server change has
// not yet propagated to UAT) Normal testing should always be
// directed to UAT.
const std::string kCpStagingLicenseServer =
    "https://proxy.staging.widevine.com/proxy";
// NOTE: Provider ID = license.widevine.com
const std::string kCpStagingServiceCertificate =
    "0ac102080312101705b917cc1204868b06333a2f772a8c1882b482920522"
    "8e023082010a028201010099ed5b3b327dab5e24efc3b62a95b598520ad5"
    "bccb37503e0645b814d876b8df40510441ad8ce3adb11bb88c4e725a5e4a"
    "9e0795291d58584023a7e1af0e38a91279393008610b6f158c878c7e21bf"
    "fbfeea77e1019e1e5781e8a45f46263d14e60e8058a8607adce04fac8457"
    "b137a8d67ccdeb33705d983a21fb4eecbd4a10ca47490ca47eaa5d438218"
    "ddbaf1cade3392f13d6ffb6442fd31e1bf40b0c604d1c4ba4c9520a4bf97"
    "eebd60929afceef55bbaf564e2d0e76cd7c55c73a082b996120b8359edce"
    "24707082680d6f67c6d82c4ac5f3134490a74eec37af4b2f010c59e82843"
    "e2582f0b6b9f5db0fc5e6edf64fbd308b4711bcf1250019c9f5a09020301"
    "00013a146c6963656e73652e7769646576696e652e636f6d128003ae3473"
    "14b5a835297f271388fb7bb8cb5277d249823cddd1da30b93339511eb3cc"
    "bdea04b944b927c121346efdbdeac9d413917e6ec176a10438460a503bc1"
    "952b9ba4e4ce0fc4bfc20a9808aaaf4bfcd19c1dcfcdf574ccac28d1b410"
    "416cf9de8804301cbdb334cafcd0d40978423a642e54613df0afcf96ca4a"
    "9249d855e42b3a703ef1767f6a9bd36d6bf82be76bbf0cba4fde59d2abcc"
    "76feb64247b85c431fbca52266b619fc36979543fca9cbbdbbfafa0e1a55"
    "e755a3c7bce655f9646f582ab9cf70aa08b979f867f63a0b2b7fdb362c5b"
    "c4ecd555d85bcaa9c593c383c857d49daab77e40b7851ddfd24998808e35"
    "b258e75d78eac0ca16f7047304c20d93ede4e8ff1c6f17e6243e3f3da8fc"
    "1709870ec45fba823a263f0cefa1f7093b1909928326333705043a29bda6"
    "f9b4342cc8df543cb1a1182f7c5fff33f10490faca5b25360b76015e9c5a"
    "06ab8ee02f00d2e8d5986104aacc4dd475fd96ee9ce4e326f21b83c70585"
    "77b38732cddabc6a6bed13fb0d49d38a45eb87a5f4";
const CdmInitData kCpStagingSrmOuputProtectionRequired =
    "0000003d"                          // blob size
    "70737368"                          // "pssh"
    "00000000"                          // flags
    "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
    "0000001d"                          // pssh data size
    // pssh data:
    "08011a0d7769646576696e655f746573"
    "74220a74656172735f73726d32";       // "tears_srm2"
const CdmInitData kCpStagingSrmOuputProtectionRequested =
    "0000003d"                          // blob size
    "70737368"                          // "pssh"
    "00000000"                          // flags
    "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
    "0000001d"                          // pssh data size
    // pssh data:
    "08011a0d7769646576696e655f746573"
    "74220a74656172735f73726d32";       // "tears_srm1"
const CdmInitData kEmptyData;

// Google Play license server data
const std::string kGpLicenseServer =
    "https://play.google.com/video-qa/license/GetWhitelistCencLicense";
// Test client authorization string.
// NOTE: Append a userdata attribute to place a unique marker that the
// server team can use to track down specific requests during debugging
// e.g., "<existing-client-auth-string>&userdata=<your-ldap>.<your-tag>"
//       "<existing-client-auth-string>&userdata=jbmr2.dev"
const std::string kGpClientAuth =
    "?source=YOUTUBE&video_id=EGHC6OHNbOo&oauth=ya.gtsqawidevine";
const std::string kGpKeyId =
    "00000034"                          // blob size
    "70737368"                          // "pssh"
    "00000000"                          // flags
    "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
    "00000014"                          // pssh data size
    // pssh data:
    "08011210e02562e04cd55351b14b3d74"
    "8d36ed8e";
const std::string kGpOfflineKeyId = kGpKeyId;

const std::string kGpClientOfflineQueryParameters = "&offline=true";
const std::string kGpClientOfflineRenewalQueryParameters =
    "&offline=true&renewal=true";
const std::string kGpClientOfflineReleaseQueryParameters =
    "&offline=true&release=true";

const ConfigTestEnv::LicenseServerConfiguration license_servers[] = {
    {kGooglePlayServer, kGpLicenseServer, "", kGpClientAuth, kGpKeyId,
     kGpOfflineKeyId, kCpProductionProvisioningServerUrl, ""},

    {kContentProtectionUatServer, kCpUatLicenseServer, kCpUatServiceCertificate,
     kCpClientAuth, kCpKeyId, kCpOfflineKeyId,
     // TODO(rfrias): replace when b/62880305 is addressed. For now use production
     kCpProductionProvisioningServerUrl,
     kCpProductionProvisioningServiceCertificate},

    {kContentProtectionStagingServer, kCpStagingLicenseServer,
     kCpStagingServiceCertificate, kCpClientAuth, kCpKeyId, kCpOfflineKeyId,
     kCpStagingProvisioningServerUrl, kCpStagingProvisioningServiceCertificate},

    {kContentProtectionProductionServer, kCpProductionLicenseServer,
     kCpProductionServiceCertificate, kCpClientAuth, kCpKeyId, kCpOfflineKeyId,
     kCpProductionProvisioningServerUrl,
     kCpProductionProvisioningServiceCertificate},
};

}  // namespace

ConfigTestEnv::ConfigTestEnv(ServerConfigurationId server_id) {
  Init(server_id);
}

ConfigTestEnv::ConfigTestEnv(ServerConfigurationId server_id, bool streaming) {
  Init(server_id);
  if (!streaming) key_id_ = license_servers[server_id].offline_key_id;
}

ConfigTestEnv::ConfigTestEnv(ServerConfigurationId server_id, bool streaming,
                             bool renew, bool release) {
  Init(server_id);
  if (!streaming) {
    key_id_ = license_servers[server_id].offline_key_id;

    if (kGooglePlayServer == server_id) {
      if (renew) {
        client_auth_.append(kGpClientOfflineRenewalQueryParameters);
      } else if (release) {
        client_auth_.append(kGpClientOfflineReleaseQueryParameters);
      } else {
        client_auth_.append(kGpClientOfflineQueryParameters);
      }
    }
  }
}

ConfigTestEnv& ConfigTestEnv::operator=(const ConfigTestEnv &other) {
  this->server_id_ = other.server_id_;
  this->client_auth_ = other.client_auth_;
  this->key_id_ = other.key_id_;
  this->key_system_ = other.key_system_;
  this->license_server_ = other.license_server_;
  this->provisioning_server_ = other.provisioning_server_;
  this->license_service_certificate_ = other.license_service_certificate_;
  this->provisioning_service_certificate_ =
      other.provisioning_service_certificate_;
  return *this;
}

void ConfigTestEnv::Init(ServerConfigurationId server_id) {
  this->server_id_ = server_id;
  client_auth_ = license_servers[server_id].client_tag;
  key_id_ = license_servers[server_id].key_id;
  key_system_ = kWidevineKeySystem;
  license_server_ = license_servers[server_id].license_server_url;
  provisioning_server_ = license_servers[server_id].provisioning_server_url;
  license_service_certificate_ =
      a2bs_hex(license_servers[server_id].license_service_certificate);
  provisioning_service_certificate_ =
      a2bs_hex(license_servers[server_id].provisioning_service_certificate);
}

const CdmInitData ConfigTestEnv::GetInitData(ContentId content_id) {
  switch (content_id) {
    case kContentIdStreaming:
      return wvcdm::a2bs_hex(kCpKeyId);
    case kContentIdOffline:
      return wvcdm::a2bs_hex(kCpOfflineKeyId);
    case kContentIdStagingSrmOuputProtectionRequested:
      return wvcdm::a2bs_hex(kCpStagingSrmOuputProtectionRequested);
    case kContentIdStagingSrmOuputProtectionRequired:
      return wvcdm::a2bs_hex(kCpStagingSrmOuputProtectionRequired);
    default:
        return kEmptyData;
  }
}

const std::string& ConfigTestEnv::GetLicenseServerUrl(
    ServerConfigurationId server_configuration_id) {
  switch (server_configuration_id) {
    case kGooglePlayServer:
      return kGpLicenseServer;
    case kContentProtectionUatServer:
      return kCpUatLicenseServer;
    case kContentProtectionStagingServer:
      return kCpStagingLicenseServer;
    case kContentProtectionProductionServer:
      return kCpProductionLicenseServer;
    default:
      return kEmptyData;
  }
}

}  // namespace wvcdm
