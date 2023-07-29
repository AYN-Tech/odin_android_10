// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.

#include <errno.h>
#include <time.h>

#include <chrono>
#include <sstream>
#include <thread>

#include <android-base/properties.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cdm_identifier.h"
#include "config_test_env.h"
#include "device_files.h"
#include "device_files.pb.h"
#include "file_store.h"
#include "file_utils.h"
#include "license_protocol.pb.h"
#include "license_request.h"
#include "log.h"
#include "oemcrypto_adapter.h"
#include "OEMCryptoCENC.h"
#include "properties.h"
#include "string_conversions.h"
#include "test_base.h"
#include "test_printers.h"
#include "url_request.h"
#include "wv_cdm_constants.h"
#include "wv_cdm_event_listener.h"
#include "wv_content_decryption_module.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::Each;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAreArray;

namespace {

#define N_ELEM(a) (sizeof(a)/sizeof(a[0]))

// HTTP response codes.
const int kHttpOk = 200;

const wvcdm::CdmIdentifier kExampleIdentifier = {
    wvcdm::EMPTY_SPOID,
    "com.example",
    "com.example",
    7
};

const wvcdm::CdmIdentifier kAlternateCdmIdentifier1 = {
    "alternate_spoid_1",
    "alternate_origin_1",
    "com.alternate1.url",
    8
};

const std::string kEmptyServiceCertificate;
const std::string kComma = ",";

// Protobuf generated classes
using video_widevine::LicenseIdentification;
using video_widevine::LicenseRequest_ContentIdentification;

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

SubSampleInfo clear_sub_sample = {
    true, 1, true, false, false,
    wvcdm::a2bs_hex("371EA35E1A985D75D198A7F41020DC23"),
    wvcdm::a2b_hex(
        "217ce9bde99bd91e9733a1a00b9b557ac3a433dc92633546156817fae26b6e1c"
        "942ac20a89ff79f4c2f25fba99d6a44618a8c0420b27d54e3da17b77c9d43cca"
        "595d259a1e4a8b6d7744cd98c5d3f921adc252eb7d8af6b916044b676a574747"
        "8df21fdc42f166880d97a2225cd5c9ea5e7b752f4cf81bbdbe98e542ee10e1c6"
        "ad868a6ac55c10d564fc23b8acff407daaf4ed2743520e02cda9680d9ea88e91"
        "029359c4cf5906b6ab5bf60fbb3f1a1c7c59acfc7e4fb4ad8e623c04d503a3dd"
        "4884604c8da8a53ce33db9ff8f1c5bb6bb97f37b39906bf41596555c1bcce9ed"
        "08a899cd760ff0899a1170c2f224b9c52997a0785b7fe170805fd3e8b1127659"),
    wvcdm::a2b_hex(
        "217ce9bde99bd91e9733a1a00b9b557ac3a433dc92633546156817fae26b6e1c"
        "942ac20a89ff79f4c2f25fba99d6a44618a8c0420b27d54e3da17b77c9d43cca"
        "595d259a1e4a8b6d7744cd98c5d3f921adc252eb7d8af6b916044b676a574747"
        "8df21fdc42f166880d97a2225cd5c9ea5e7b752f4cf81bbdbe98e542ee10e1c6"
        "ad868a6ac55c10d564fc23b8acff407daaf4ed2743520e02cda9680d9ea88e91"
        "029359c4cf5906b6ab5bf60fbb3f1a1c7c59acfc7e4fb4ad8e623c04d503a3dd"
        "4884604c8da8a53ce33db9ff8f1c5bb6bb97f37b39906bf41596555c1bcce9ed"
        "08a899cd760ff0899a1170c2f224b9c52997a0785b7fe170805fd3e8b1127659"),
    wvcdm::a2b_hex("f6f4b1e600a5b67813ed2bded913ba9f"), 0,
    OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample};

SubSampleInfo clear_sub_samples[2] = {
  // block 0, key SD, encrypted, 128b
  { true, 1, true, false, false,
    wvcdm::a2bs_hex("371EA35E1A985D75D198A7F41020DC23"),
    wvcdm::a2b_hex(
        "217ce9bde99bd91e9733a1a00b9b557ac3a433dc92633546156817fae26b6e1c"
        "942ac20a89ff79f4c2f25fba99d6a44618a8c0420b27d54e3da17b77c9d43cca"
        "595d259a1e4a8b6d7744cd98c5d3f921adc252eb7d8af6b916044b676a574747"
        "8df21fdc42f166880d97a2225cd5c9ea5e7b752f4cf81bbdbe98e542ee10e1c6"),
    wvcdm::a2b_hex(
        "217ce9bde99bd91e9733a1a00b9b557ac3a433dc92633546156817fae26b6e1c"
        "942ac20a89ff79f4c2f25fba99d6a44618a8c0420b27d54e3da17b77c9d43cca"
        "595d259a1e4a8b6d7744cd98c5d3f921adc252eb7d8af6b916044b676a574747"
        "8df21fdc42f166880d97a2225cd5c9ea5e7b752f4cf81bbdbe98e542ee10e1c6"),
    wvcdm::a2b_hex("f6f4b1e600a5b67813ed2bded913ba9f"), 0,
    OEMCrypto_FirstSubsample},
  // block 1, key SD, encrypted, 128b
  { true, 1, true, false, false,
    wvcdm::a2bs_hex("371EA35E1A985D75D198A7F41020DC23"),
    wvcdm::a2b_hex(
        "ad868a6ac55c10d564fc23b8acff407daaf4ed2743520e02cda9680d9ea88e91"
        "029359c4cf5906b6ab5bf60fbb3f1a1c7c59acfc7e4fb4ad8e623c04d503a3dd"
        "4884604c8da8a53ce33db9ff8f1c5bb6bb97f37b39906bf41596555c1bcce9ed"
        "08a899cd760ff0899a1170c2f224b9c52997a0785b7fe170805fd3e8b1127659"),
    wvcdm::a2b_hex(
        "ad868a6ac55c10d564fc23b8acff407daaf4ed2743520e02cda9680d9ea88e91"
        "029359c4cf5906b6ab5bf60fbb3f1a1c7c59acfc7e4fb4ad8e623c04d503a3dd"
        "4884604c8da8a53ce33db9ff8f1c5bb6bb97f37b39906bf41596555c1bcce9ed"
        "08a899cd760ff0899a1170c2f224b9c52997a0785b7fe170805fd3e8b1127659"),
    wvcdm::a2b_hex("f6f4b1e600a5b67813ed2bded913ba9f"), 0,
    OEMCrypto_LastSubsample}
};

SubSampleInfo clear_sub_sample_no_key = {
    false, 1, false, false, false,
    wvcdm::a2bs_hex("77777777777777777777777777777777"),
    wvcdm::a2b_hex(
        "217ce9bde99bd91e9733a1a00b9b557ac3a433dc92633546156817fae26b6e1c"
        "942ac20a89ff79f4c2f25fba99d6a44618a8c0420b27d54e3da17b77c9d43cca"
        "595d259a1e4a8b6d7744cd98c5d3f921adc252eb7d8af6b916044b676a574747"
        "8df21fdc42f166880d97a2225cd5c9ea5e7b752f4cf81bbdbe98e542ee10e1c6"
        "ad868a6ac55c10d564fc23b8acff407daaf4ed2743520e02cda9680d9ea88e91"
        "029359c4cf5906b6ab5bf60fbb3f1a1c7c59acfc7e4fb4ad8e623c04d503a3dd"
        "4884604c8da8a53ce33db9ff8f1c5bb6bb97f37b39906bf41596555c1bcce9ed"
        "08a899cd760ff0899a1170c2f224b9c52997a0785b7fe170805fd3e8b1127659"),
    wvcdm::a2b_hex(
        "217ce9bde99bd91e9733a1a00b9b557ac3a433dc92633546156817fae26b6e1c"
        "942ac20a89ff79f4c2f25fba99d6a44618a8c0420b27d54e3da17b77c9d43cca"
        "595d259a1e4a8b6d7744cd98c5d3f921adc252eb7d8af6b916044b676a574747"
        "8df21fdc42f166880d97a2225cd5c9ea5e7b752f4cf81bbdbe98e542ee10e1c6"
        "ad868a6ac55c10d564fc23b8acff407daaf4ed2743520e02cda9680d9ea88e91"
        "029359c4cf5906b6ab5bf60fbb3f1a1c7c59acfc7e4fb4ad8e623c04d503a3dd"
        "4884604c8da8a53ce33db9ff8f1c5bb6bb97f37b39906bf41596555c1bcce9ed"
        "08a899cd760ff0899a1170c2f224b9c52997a0785b7fe170805fd3e8b1127659"),
    wvcdm::a2b_hex("f6f4b1e600a5b67813ed2bded913ba9f"), 0,
    OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample};

SubSampleInfo single_encrypted_sub_sample = {
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
    wvcdm::a2b_hex("f6f4b1e600a5b67813ed2bded913ba9f"), 0,
    OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample};

SubSampleInfo single_encrypted_offline_sub_sample = {
    // key SD, encrypted, 256b
    true, 1, true, true, false,
    wvcdm::a2bs_hex("DD003BA34DA3CDA09AA3B6D5CC6C34B2"),
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
        "4E477383EDAFA01D4C31CBCF015AE3D256FF96052F24EB50C37AEC9E798A23AE"
        "58D187C627769F11E72C712BFE269F7BEB04D7E636D508F9B623888416340730"
        "36C0F409B49BD9EB2DBAAF7218527371A40AB9B93A7C3FACA246A9A37C560106"
        "2F6C5C5F7C0F4DA528C70639268602E08D100079A6D8CDBD82C44BFF7FC8D304"
        "277E5638AA275AD1CC08F0D4F850777E0453DEFD927B49D2B5CF0372FC95BEEE"
        "4287F7AEB30E3FECBDEB2981BD0691FED2D7CFACB92E115A44CADD96843F240E"
        "236A9F9B2E3CB075912FE15C5056B21D809538C3C19D5B2F5FA242CD7F550306"
        "6DA2F6A78C5090D9B49F78632FB6F278AC1F680E690BF3AD4933FDE77922CF6A"),
    wvcdm::a2b_hex("f6f4b1e600a5b67813ed2bded913ba9f"), 0,
    OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample};

SubSampleInfo switch_key_encrypted_sub_samples[2] = {
    // block 0, key SD, encrypted, 256b
    {true, 2, true, true, false,
     wvcdm::a2bs_hex("371EA35E1A985D75D198A7F41020DC23"),
     wvcdm::a2b_hex(
         "9efe1b7a3973324525e9b8c516855e554b52a73ce35cd181731b005a6525624f"
         "a03875c89aee02f1da7f556b7e7d9a3eba89fe3061194bc2d1446233ca022892"
         "ab95083f127d6ccb01b1368e6b6fa77e3570d84477a5517f2965ff72f8e0740c"
         "d8282c22e7724ce44d88526dcd2d601002b717d8ca3b97087d28f9e3810efb8e"
         "d4b08ee2da6bdb05a790b6363f8ee65cae8328e86848e4caf9be92db3e5492ad"
         "6363a26051c23cf23b9aee79a8002470c4a5834c6aae956b509a42f4110262e0"
         "565a043befd8ef3a335c9dfedca8d218f364215859d7daf7d040b1f0cb2eda87"
         "c1be18f323fb0235dd9a6e7b3b2fea1cb9c6e5bc2b349962f0b8f0b92e749db2"),
     wvcdm::a2b_hex(
         "38a715e73c9209544c47e5eb089146de8136df5c6ed01e3e8d9cea8ae18a81c9"
         "8c9c8ec67bf379dd80a21f57b0b00575827a240cd11332c5212defe9f1ef8b8e"
         "2399271767bfe81e5a11abf7bca1307578217c4d5f8b942ab04351b4725d6e24"
         "cd171fa3083570f7d7ae2b297224f701fd04d699c12c53e9ce9d3dab64ee6332"
         "5fba183b7a1f3f20acaeabc0c446c9ca0df39fafb1e2891c72500741ad5b7941"
         "4651729e30e9ddbb22f47a5026e09c795ff15a858123a7979e7be716cb8cd075"
         "e8bfb91bc0cc83f7cacd5c4772f7479a1193d9307bc5f837185faed5499e66a7"
         "e27db50b5d018d022279032016862883befd113b6c784889be8f9e6eb0f335f7"),
     wvcdm::a2b_hex("fd38d7f754a97128b78440433e1ef4a8"), 0,
     OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample},
    // block 1, key HD, encrypted, 256b
    {true, 2, true, true, false,
     wvcdm::a2bs_hex("6D665122C01767FC087F340E6C335EA4"),
     wvcdm::a2b_hex(
         "d9392411d15f47de0d7dd854eae5eb5ffbd2d3f86c530d2ef619fc81725df866"
         "2e6267041b947863e5779812da3220824a3d154e7b094c1af70238c65877454e"
         "3c3bdcce836962ba29b69442d5e5b4a4ff32a4b026521f3fa4d442a400081cdd"
         "ba6ed313c43cc34443c4dc2b9cdcc9dbd478bf6afc4d515c07b42d8b151c15cc"
         "165270f6083ecd5c75313c496143068f45966bb53e35906c7568424e93e35989"
         "7da702fb89eb7c970af838d56a64a7a68f7cffe529807765d62540bb06bbc633"
         "6eeec62d20f5b639731e57a0851e23e146cb9502dbde93dc4aca20e471a3fa0b"
         "df01a74ecb48d5f57ac2be98fb21d19de7587d8d1e6e1788726e1544d05137f6"),
     wvcdm::a2b_hex(
         "c48a94d07c34c4315e01010dbcc63a038d50a023b1ff2a07deae6e498cb03f84"
         "57911d8c9d72fa5184c738d81a49999504b7cd4532b465436b7044606a6d40a2"
         "74a653c4b93ebaf8db585d180211a02e5501a8027f2235fe56682390325c88ee"
         "2ada85483eddb955c56f79634a2ceeb36d04b5d6faf7611817577d9b0fda088e"
         "921fbdd7fa594ee4f557f7393f51f3049cd36973f645badf7cc4672ef8d973da"
         "7dae8e59f32bf950c6569845a5261b5ed9cc500706eccf8d41f015b32026e16e"
         "ab274465d880ff99a5eaea603eea66c7b0e6679bfd87145de0ec1a73ebfff092"
         "866346a1d66db2923bca30664f417a6b66c07e91fb491be7872ebe5c9c2d03c2"),
     wvcdm::a2b_hex("f56ab022666de858920e532f19bb32f6"), 0,
     OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample}};

SubSampleInfo partial_encrypted_sub_samples[3] = {
    // block 1, key SD, encrypted, 1-125b, offset 0
    {true, 3, true, true, false,
     wvcdm::a2bs_hex("371EA35E1A985D75D198A7F41020DC23"),
     wvcdm::a2b_hex(
         "53cc758763904ea5870458e6b23d36db1e6d7f7aaa2f3eeebb5393a7264991e7"
         "ce4f57b198326e1a208a821799b2a29c90567ab57321b06e51fc20dc9bc5fc55"
         "10720a8bb1f5e002c3e50ff70d2d806a9432cad237050d09581f5b0d59b00090"
         "b3ad69b4087f5a155b17e13c44d33fa007475d207fc4ac2ef3b571ecb9"),
     wvcdm::a2b_hex(
         "52e65334501acadf78e2b26460def3ac973771ed7c64001a2e82917342a7eab3"
         "047f5e85449692fae8f677be425a47bdea850df5a3ffff17043afb1f2b437ab2"
         "b1d5e0784c4ed8f97fc24b8f565e85ed63fb7d1365980d9aea7b8b58f488f83c"
         "1ce80b6096c60f3b113c988ff185b26e798da8fc6f327e4ff00e4b3fbf"),
     wvcdm::a2b_hex("6ba18dd40f49da7f64c368e4db43fc88"), 0,
     OEMCrypto_FirstSubsample},
    // block 2, key SD, encrypted, 126-187b, offset 5
    {true, 3, true, true, false,
     wvcdm::a2bs_hex("371EA35E1A985D75D198A7F41020DC23"),
     wvcdm::a2b_hex(
         "f3c852"
         "ce00dc4806f0c6856ae1732e20308096478e1d822d75c2bb768119565d3bd6e6"
         "901e36164f4802355ee758fc46ef6cf5f852dd5256c7b1e5f96d29"),
     wvcdm::a2b_hex(
         "b1ed0a"
         "a054bce40ccb0ebc70b181d1a12055f46ac55e29c7c2473a29d2a366d240ec48"
         "7cede274f012813a877f99159e7062b6a37cfc9327a7bc2195814e"),
     wvcdm::a2b_hex("6ba18dd40f49da7f64c368e4db43fc8f"), 13, 0},
    // block 3, key SD, encrypted, 188-256b, offset 5
    {true, 3, true, true, false,
     wvcdm::a2bs_hex("371EA35E1A985D75D198A7F41020DC23"),
     wvcdm::a2b_hex(
         "3b20525d5e"
         "78b8e5aa344d5c4e425e67ddf889ea7c4bb1d49af67eba67718b765e0a940402"
         "8d306f4ce693ad6dc0a931d507fa14fff4d293d4170280b3e0fca2d628f722e8"),
     wvcdm::a2b_hex(
         "653b818d1d"
         "4ab9a9128361d8ca6a9d2766df5c096ee29f4f5204febdf217a94a5b560cd692"
         "cc36d3e071df789fdeac2fb7ec6dcd7af94bb1f85c22025b25e702e38212b927"),
     wvcdm::a2b_hex("6ba18dd40f49da7f64c368e4db43fc93"), 11,
     OEMCrypto_LastSubsample}};

SubSampleInfo single_encrypted_sub_sample_short_expiry = {
    // key 1, encrypted, 256b
    true, 1, true, true, false,
    wvcdm::a2bs_hex("9714593E1EEE57859D34ECFA821702BB"),
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
        "5a36c0b633b58faf22156d78fdfb608e54a8095788b2b0463ef78d030b4abf82"
        "eff34b8d9b7b6352e7d72de991b599662aa475da355033620152e2356ebfadee"
        "06172be9e1058fa177e223b9fdd191380cff53c3ea810c6fd852a1df4967b799"
        "415179a2276ec388ef763bab89605b9c6952c28dc8d6bf86b03fabbb46b392a3"
        "1dad15be602eeeeabb45070b3e25d6bb0217073b1fc44c9fe848594121fd6a91"
        "304d605e21f69615e1b57db18312b6b948725724b74e91d8aea7371e99532469"
        "1b358bdee873f1936b63efe83d190a53c2d21754d302d63ff285174023473755"
        "58b938c2e3ca4c2ce48942da97f9e45797f2c074ac6004734e93784a48af6160"),
    wvcdm::a2b_hex("4cca615fc013102892f91efee936639b"), 0,
    OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample};

SubSampleInfo usage_info_sub_samples_icp[] = {
    {true, 1, true, true, false,
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
     wvcdm::a2b_hex("7362b5140c4ce0cd5f863858668d3f1a"), 0,
     OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample},
    {true, 1, true, true, false,
     wvcdm::a2bs_hex("04B2B456405CAD7170BE848CA75AA2DF"),
     wvcdm::a2b_hex(
         "ccb68f97b03e7af1a9e208d91655ba645cc5a5d454f3cb7c3d621e98a7592d90"
         "4ff7023555f0e99bcf3918948f4fca7a430faf17d7d67268d81d8096d7c48809"
         "c14220e634680fbe0c760571dd86a1905835035f4a238c2d7f17bd1363b113c1"
         "91782aebb77a1064142a68b59ecdcc6990ed4244082464d91dbfe09e08744b2f"
         "d1e850a008acbbe129fbd050c8dc1b28cb8cc2c1e2d920ea458f74809297b513"
         "85307b481cbb81d6759385ee782d6c0e101c20ca1937cfd0d6e024da1a0f718a"
         "fb7c4ff3df1ca87e67602d28168233cc2448d44b79f405d4c6e67eb88d705050"
         "2a806cb986423e3b0e7a97738e1d1d143b4f5f926a4e2f37c7fbe65f56d5b690"),
     wvcdm::a2b_hex(
         "fa35aa1f5e5d7b958880d5eed9cc1bb81d36ebd04c0250a8c752ea5f413bbdcf"
         "3785790c8dba7a0b21c71346bb7f946a9b71c0d2fe87d2e2fab14e35ee8400e7"
         "097a7d2d9a25b468e848e8dee2388f890967516c7dab96db4713c7855f717aed"
         "2ae9c2895baaa636e4a610ab26b35d771d62397ba40d78694dab70dcbdfa91c3"
         "6af79ad6b6ebb479b4a5fbc242a8574ebe6717f0813fbd6f726ce2af4d522e66"
         "b36c940fce519c913db56a6372c3636b10c0149b4cd97e74c576765b533abdc2"
         "729f1470dd7f9a60d3572dcc9839582a4606ee17eaced39797daef8f885d3f8f"
         "e14877ae530451c4242bbc3934f85a5bb71b363351894f881896471cfeaf68b2"),
     wvcdm::a2b_hex("4a59e3e5f3e4f7e2f494ad09c12a9e4c"), 0,
     OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample},
    {true, 1, true, true, false,
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
     wvcdm::a2b_hex("b358ab21ac90455bbf60490daad457e3"), 0,
     OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample},
    {true, 1, true, true, false,
     wvcdm::a2bs_hex("5292104C011418973C31235953FC8205"),
     wvcdm::a2b_hex(
         "d433adfd3d892d5c3e7d54ab0218ee0712400a920d4b71d553912451169f9b79"
         "3f103260cf04c34f6a5944bb96da79946a62bdbcd804ca28b17656338edfa700"
         "5c090f2750663a026fd15a0b0e448adbbfd53f613ea3993d9fd504421b575f28"
         "12020bb8cca0ce333eabee0403df9f410c6473d7673d6991caab6ea2ece8f743"
         "5a3ca049fa00c96c9b7c47e3073d25d08f23b47ffc509c48a81a2f98c9ec8a1d"
         "e41764c14a5010df8b4692e8612a45bf0645601d4910119e6268ca4f6d8016a8"
         "3d933d53f44243674b522bae43043c068c8cae43f0ac224198de71315b3a6f82"
         "c1b523bbdcdb3e9f162c308684dd17e364b448ed0e90b0e496b8cf633a982708"),
     wvcdm::a2b_hex(
         "5efb5e5b913785e9935e67e763b8ff29a6687ac6c18d5a7e16951beb704f9c95"
         "f081ca28f54c3e237fb5a7b0444e9a3e17da91e5cf2c0a8f009a873fb079c339"
         "81b0ebc565b2c56d983ee33686fa5057c9891e246b67bb6950400acb06d5ae50"
         "0e61a7e9289ea67ec2e88e8d0cc3c494fd996e93270e9b264a21818987e969c5"
         "1e2955c5a53202e5aec1e2c906e1c006325112eb5c33ee37d0c07ea97d80c17f"
         "d56e0efcf40c8c98981a86c18a159f05d851891236c124641d4584c49ccd7478"
         "4f328a9cacae0f945238d98741b2969fe258903e85f963daba7168f05c18b09f"
         "660dae18de41b1c49769cd38e24b135c37a65b69533f5c7d085898faedfbed5d"),
     wvcdm::a2b_hex("cef7e8aaa6ec1154cb68a988f7c9e803"), 0,
     OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample},
    {true, 1, true, true, false,
     wvcdm::a2bs_hex("D7C01C2F868AE314BCB893E4E9C6AC75"),
     wvcdm::a2b_hex(
         "fa5d28677721de488ffc209321529728c77bc338accd45ccc98ab2063fc8c373"
         "48c7698534175d72bf185690d19474d08c4fd4ed4eb46d858633f05337d70e92"
         "03f7ee6bec0f7003bdf6fa665ba172855a51a82da406348ba651a2f62888c30a"
         "7b4e1355bb94a9ff5c458f397c9a09e5d7785b286ef83142ddad324cc74e1929"
         "60ad1c34c425cdefbedcb62ca9b21ac4f3df7f5922e263cb7798de54b622ab3f"
         "64a0dd6ee1e40be6ecc857e657994ecac02ccfafc9036f382d7dbdf35c903356"
         "40b7c9db088143060b24f24b21c4a7c2faeb3d308e57c5a75955fd704cfe4dee"
         "71a4a7d823102b90eddded795ca6eb36282d777db8cfd783e50e5c2a816ee9ed"),
     wvcdm::a2b_hex(
         "d5db2f50c0f5a39414ddfa5129c2c641836a8c6312b26a210c996988e0c768d5"
         "9a3adff117293b52b0653c0d6e22589edda804fb8caa7442362fe4caf9053b6a"
         "2a34896399259a188f0c805de54b091a7eabff098b28d54584c01dd83301e4ca"
         "a01b226c4541af1592d4440e103eb55bbd08c471efb0856ec9ced43211fc3325"
         "3d402dff0d15f40833dd71259a8d40d527659ef3e5f9fd0826c9471dddb17e1e"
         "fab916abc957fb07d7eac4a368ac92a8fb16d995613af47303034ee57b59b1d7"
         "101aa031f5586b2f6b4c74372c4d7306db02509b5924d52c46a270f427743a85"
         "614f080d83f3b15cbc6600ddda43adff5d2941da13ebe49d80fd0cea5025412b"),
     wvcdm::a2b_hex("964c2dfda920357c668308d52d33c652"), 0,
     OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample}};

SubSampleInfo kEntitlementWithKeyRotationSubSampleSinglePssh[] = {
  {
    true, 1, true, true, false,
    wvcdm::a2bs_hex("c8326486bb5d5c4a958f00b1111afc81"),
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
        "cb468f067baa5634f83947b921fdb37e66bc5925edc4bdc2424db4c33cfd55a1"
        "cd0a96b6634796ddcf5ba8820b83f80d318ff49b7e614779f5e87dd6dfadd8d6"
        "cf95c7b0f21e4174e2e91371ebdb69866340a37a581a7c0713eda5168bcfc003"
        "12a341a83defa754c4601772ed9171526720b6a2b2b084030f21ef13f2a35dec"
        "e93f5c394c56d9ce108c2f5b0e5edb857d322fae24ec22f3ad726496b382306b"
        "4fdf5a0a99efc2db2a9458f0bfd6b21869c9acf6ea222fe942af6cd9b38d9a50"
        "a96db14ad27d368c5753aa5da8d8507603ed08086e2492bdff267ee64862f159"
        "b19d2c72b2f5a39520d5ae2aacae1a192d375d45f3f9ba86d5026cbcacdfecbe"),
    wvcdm::a2b_hex("f6f4b1e600a5b67813ed2bded913ba9f"), 0,
    OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample},
  {
    true, 1, true, true, false,
    wvcdm::a2bs_hex("f8488775a99855ff94b93ec5bd499356"),
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
        "51b0133e2e5f40c22d10ec0562799e5e4118aacaa6a820be976acee4b7689280"
        "541b56836c454414fbaebf9a3142baa2c8009a4f19ac033665bd3495f2ad19f3"
        "3b850d1e8e6957172571e82d3c812d03588c95a1ef49f08b33f21ea4f2d382c9"
        "28704e329847e5fe98966949f39e272ec30126f5d7a4ae21d3a0d25aa8ccf637"
        "d5f880a6733b07bdd33cfdc3c36dece2bffb6049f218162b024df4f800557568"
        "a792e0add16fc388ee13595313c3fbeef28f69737523e449dc2cf893f0566a79"
        "8a83110c8d3aaf5c1f7e8e8fe355a294a9a77b5494704b18e27f1315cb19c104"
        "3ad2061a2a414d40cc768fc4c8f49a3905e3a82095aa4eef6a1ad8af1029fced"),
    wvcdm::a2b_hex("f6f4b1e600a5b67813ed2bded913ba9f"), 0,
    OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample}};

SubSampleInfo kEntitlementWithKeyRotationSubSampleDualPssh[] = {
  {
    true, 1, true, true, false,
    wvcdm::a2bs_hex("1D9B8E13B59951169348FF8D5B9394C0"),
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
        "4bb2ff540e12e4c97248b63abdf30c4474df11ae8f22ba587e5aa9b64d51f8ce"
        "209b13cb24f436ac192060690d13d5a1230fe5207287678a3acbaf59b5381186"
        "92dcdec42c770afc0545407c243a452214d0497f1a044adc56ac1dba5530d5a5"
        "482f9fc67a5e1d1314e864ad85fec9f78657e10f68ae8720b218339c96e878c1"
        "c0f09015172d8a52a85b6f09526b98aad6d7326d3799a418581efadd16f9ba3e"
        "454945428a36959a296aa14fe05cb8ae7b44ae68d82950f0742d38d86f167c36"
        "75e75390d3cc6cd6db267729b2aa81a7e7c4db186e82d4300c4123c0a5de73e9"
        "a6bb238bd351769359d1b46c9702270b756038fd54ef609d985eecde58e9a58e"),
    wvcdm::a2b_hex("f6f4b1e600a5b67813ed2bded913ba9f"), 0,
    OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample},
  {
    true, 1, true, true, false,
    wvcdm::a2bs_hex("A0396729B3795B46A5EA7F9919B96A67"),
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
        "49067453f9fe1b36fb2b37a2bba927bfe7f5f81ce715047beb99675da809b502"
        "a5638f891cfad95c9fefdb32b6e8614ce3d5528032d51644a6cfaaccea2ad0b6"
        "dd8bd36a0fb751bf4b70e1cb02266f373be467d3167aed4f3820eb4af884cc39"
        "f60f83e060c8674b7e53d7ec8934ec07750d4677ed14ad6f6bacf46f46cf3ea8"
        "560f704220bc3e32b9ad21c74aff6dbdbd64f49f38717ab7a05042dfe6fdc56f"
        "47ddc384822b9a3fab3445653fa51f2405fcbcfd6d39a7fb8a99777c41960b94"
        "74f1deb4b9b242bef609c625af791cba63e8c184b0312d624daba3889307b48b"
        "00c362f246c3bc0b36cd41f9ec8eb72eab603f9517c7948f5e317a93ac1a5631"),
    wvcdm::a2b_hex("f6f4b1e600a5b67813ed2bded913ba9f"), 0,
    OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample}};

// License duration and uncertainty window
const uint32_t kSingleEncryptedSubSampleIcpLicenseDurationExpiration = 5;
const uint32_t kSingleEncryptedSubSampleIcpLicenseExpirationWindow = 2;

struct SessionSharingSubSampleInfo {
  SubSampleInfo* sub_sample;
  bool session_sharing_enabled;
  wvcdm::CdmIdentifier cdm_identifier;
};

SessionSharingSubSampleInfo session_sharing_sub_samples[] = {
    {&clear_sub_sample, false, wvcdm::kDefaultCdmIdentifier},
    {&clear_sub_sample, true, wvcdm::kDefaultCdmIdentifier},
    {&clear_sub_sample_no_key, false, wvcdm::kDefaultCdmIdentifier},
    {&clear_sub_sample_no_key, true, wvcdm::kDefaultCdmIdentifier},
    {&single_encrypted_sub_sample, false, wvcdm::kDefaultCdmIdentifier},
    {&single_encrypted_sub_sample, true, wvcdm::kDefaultCdmIdentifier},
    // The last entry simulates session sharing using the non default
    // identifier.
    {&single_encrypted_sub_sample, true, kExampleIdentifier}};

struct UsageInfoSubSampleInfo {
  SubSampleInfo* sub_sample;
  uint32_t usage_info;
  wvcdm::SecurityLevel security_level;
  std::string app_id;
};

void PrintTo(UsageInfoSubSampleInfo* param, std::ostream* os) {
  if (param) {
    *os << "info=" << param->usage_info << ", "
        << "level=" << (param->security_level == wvcdm::kLevel3 ? "L3" : "L1")
        << " app_id=" << param->app_id;
  } else {
    *os << "<null ptr>";
  }
}

UsageInfoSubSampleInfo usage_info_sub_sample_info[] = {
    {&usage_info_sub_samples_icp[0], 1, wvcdm::kLevelDefault, ""},
    {&usage_info_sub_samples_icp[0], 3, wvcdm::kLevelDefault, ""},
    {&usage_info_sub_samples_icp[0], 5, wvcdm::kLevelDefault, ""},
    {&usage_info_sub_samples_icp[0], 3, wvcdm::kLevelDefault, "other app id"},
    {&usage_info_sub_samples_icp[0], 1, wvcdm::kLevel3, ""},
    {&usage_info_sub_samples_icp[0], 3, wvcdm::kLevel3, ""},
    {&usage_info_sub_samples_icp[0], 5, wvcdm::kLevel3, ""},
    {&usage_info_sub_samples_icp[0], 3, wvcdm::kLevel3, "other app id"}};

struct UsageLicenseAndSubSampleInfo {
  std::string pssh;
  SubSampleInfo* sub_sample;
  std::string provider_session_token;
};

std::string kPsshStreamingClip3 = wvcdm::a2bs_hex(
    "000000427073736800000000"                  // blob size and pssh
    "EDEF8BA979D64ACEA3C827DCD51D21ED00000022"  // Widevine system id
    "08011a0d7769646576696e655f74657374220f73"  // pssh data
    "747265616d696e675f636c697033");
std::string kPsshStreamingClip4 = wvcdm::a2bs_hex(
    "000000427073736800000000"                  // blob size and pssh
    "EDEF8BA979D64ACEA3C827DCD51D21ED00000022"  // Widevine system id
    "08011a0d7769646576696e655f74657374220f73"  // pssh data
    "747265616d696e675f636c697034");
std::string kPsshStreamingClip5 = wvcdm::a2bs_hex(
    "000000427073736800000000"                  // blob size and pssh
    "EDEF8BA979D64ACEA3C827DCD51D21ED00000022"  // Widevine system id
    "08011a0d7769646576696e655f74657374220f73"  // pssh data
    "747265616d696e675f636c697035");
std::string kPsshStreamingClip7 = wvcdm::a2bs_hex(
    "000000427073736800000000"                  // blob size and pssh
    "EDEF8BA979D64ACEA3C827DCD51D21ED00000022"  // Widevine system id
    "08011a0d7769646576696e655f74657374220f73"  // pssh data
    "747265616d696e675f636c697037");
std::string kPsshStreamingClip20 = wvcdm::a2bs_hex(
    "000000437073736800000000"                  // blob size and pssh
    "EDEF8BA979D64ACEA3C827DCD51D21ED00000023"  // Widevine system id
    "08011a0d7769646576696e655f746573"          // pssh data
    "74221073747265616d696e675f636c69703230");
std::string kPsshStreamingClip21 = wvcdm::a2bs_hex(
    "000000437073736800000000"                  // blob size and pssh
    "EDEF8BA979D64ACEA3C827DCD51D21ED00000023"  // Widevine system id
    "08011a0d7769646576696e655f746573"          // pssh data
    "74221073747265616d696e675f636c69703231");
const std::string kSinglePsshEntitlementWithKeyRotation[] = {
  wvcdm::a2bs_hex(
    "000001fb7073736800000000"                  // blob size and pssh
    "edef8ba979d64acea3c827dcd51d21ed000001db"  // Widevine system id
    "220b47726f7570563254657374381448"          // pssh data
    "e3dc959b065002580272580a10668093"
    "381a8c5be48a0168ce372726ac1210c8"
    "326486bb5d5c4a958f00b1111afc811a"
    "20082cd9d3aed3ebe6239d30fbcf0b22"
    "1d28cbb0360ea1295c2363973346ec00"
    "512210914781334e864c8eb7f768cf26"
    "49073872580a10f872d11d5b1052f2bd"
    "a94e60a0e383021210450897c987a85c"
    "2e9579f968554a12991a2097e603ceea"
    "f35ed8cef1029eae7a0a54701e3d6db6"
    "80e7da1de3b22a8db347fb2210b41c34"
    "29b7bb96972bbaf6587bc0ddf172580a"
    "10bac58b9fce9e5929a42a180e529f19"
    "4712103f11f22988d25659b145ce4854"
    "3e6b141a20416e22768e5a57b08d155e"
    "5210d00658056947ff06d626668bceb3"
    "5eb01c6b57221081fb2ff3fef79d332f"
    "f98be46233596972580a101261c8036d"
    "ae5c8caa968858aa0ca9cc12106d583c"
    "b37c1456519843a81cf49912221a20c2"
    "1116bb54a226e8d879a4cd41d8879920"
    "2ae85b80d83b1b4447e5d7fcad6f6a22"
    "100b27a4c3f44771d2b0c7c34c66af35"
    "b572580a10ab1c8c259c6b5967991389"
    "65bff5ac0c1210b5b4473658565d3786"
    "efaf4b85d8e6e21a203ce6a9085285c2"
    "ece0b650dc83dd7aa8ac849611a8e3f8"
    "3c8f389223c0f3621522101946f0c2a3"
    "d543101cc842bbec2d0b30"),
  wvcdm::a2bs_hex(
    "000001fb7073736800000000"                  // blob size and pssh
    "edef8ba979d64acea3c827dcd51d21ed000001db"  // Widevine system id
    "220b47726f7570563254657374381548"          // pssh data
    "e3dc959b065002580272580a10668093"
    "381a8c5be48a0168ce372726ac1210f8"
    "488775a99855ff94b93ec5bd4993561a"
    "20d15ba631c20e95da0d4857f6a1d25a"
    "a3bccbd3fde18b3fdc1dd8c4f0ede76f"
    "402210d6dd3675f0d1150052e81b9107"
    "6d7fc172580a10f872d11d5b1052f2bd"
    "a94e60a0e383021210ad1f93ad921e53"
    "b097c415b2bf1ef1c61a20b2087b60a2"
    "d253ac2158a1bfa789b150b79701b29e"
    "c852a2662560f8b8977a4c2210051ed3"
    "2628671fbda58f506ba5ea713972580a"
    "10bac58b9fce9e5929a42a180e529f19"
    "47121027cdda7bfe5e5fd4bff2ebc9c7"
    "c020701a20f2cb1184d648a2404517e6"
    "7a39d698332aae6bb890a69bf7ddb536"
    "75b8ac41c62210a80ed7f9b728fdd566"
    "0b01b173ace26372580a101261c8036d"
    "ae5c8caa968858aa0ca9cc1210769a70"
    "0442a25bf5ae17174c70f4cb8e1a206c"
    "7b2012723fc47c83b003ea214204915f"
    "9a63dc373bf219f36ccf5697589aa422"
    "10bcc3c16e836cca264d5493a0c334d3"
    "4872580a10ab1c8c259c6b5967991389"
    "65bff5ac0c1210894b04aef78557c6a7"
    "e6e8855febbcc91a2025cc545ee3cd0c"
    "c323586610ff6a8f8f22a78f5fade2f2"
    "1083f152c52208f16d2210257aacacec"
    "512a2e769396b10e6d9dfa")
};
const std::string kDualPsshEntitlementWithKeyRotation[] = {
  wvcdm::a2bs_hex(
    "0000003e7073736800000000"                  // blob size and pssh
    "edef8ba979d64acea3c827dcd51d21ed0000001e"  // Widevine system id
    "2210426f6f5261646c6579666137613664393800"  // pssh data
    "48e3dc959b0650126a00000002027073"
    "736800000000edef8ba979d64acea3c8"
    "27dcd51d21ed000001e22210426f6f52"
    "61646c657966613761366439380048e3"
    "dc959b06501258026a0072580a109ab3"
    "fb7eaffc5ee087568b9581ff0e721210"
    "4a2da018480c51358c660c4b1a198721"
    "1a20a00425ef5fe538adba59a6c4ce9a"
    "fbff394351b79552643e08ed3a0cde2c"
    "3ca42210931a2ef95b898d804b9a25ef"
    "6d2805f072580a100fba9bd0f8e055d5"
    "900bbfaabf5033141210f9bfca8cd3b8"
    "593897f0b9283cd768f61a209c1d9a96"
    "573a4dbba68327f03e598adfde1bd29f"
    "1e709f206c2424d82ca14c1c22108428"
    "46cffba58c12742418a702138c3a7258"
    "0a109fec6ad1d36e56cdaf5c232480f6"
    "3f8812101d9b8e13b59951169348ff8d"
    "5b9394c01a2058ee6164ae759802e0b1"
    "f9f7eeeaa3606faf21182777fe716c01"
    "4c2412621543221039c8a6caab12515e"
    "3c744a5447e9b72372580a10cd2e9ebf"
    "89c257e2a3196c82ac4c76ba1210d48f"
    "8f11c2d853289e981f5775db60441a20"
    "3b491980b27587592c86e73da27caa12"
    "d95acda2295f768746090b55b81c9d61"
    "2210dd7f29944aaeb08826ba4fc0dec6"
    "c88d72580a10b3fd79204b9d5cbf8f67"
    "2eca27255e9e121023feaaf517585fde"
    "bdf0d4693f32c1091a20d18a1c0e54b8"
    "fbc189b58ccc0dc5a0c541b628b8fe23"
    "34c862944b5555ad7f7f221091269127"
    "e4feb6a8fa878c6e9781a55f"),
  wvcdm::a2bs_hex(
    "0000003e7073736800000000ed"                // blob size and pssh
    "ef8ba979d64acea3c827dcd51d21ed0000001e"    // Widevine system id
    "2210426f6f5261646c65796661"                // pssh data
    "3761366439380448e3dc959b0650126a"
    "00000002027073736800000000edef8b"
    "a979d64acea3c827dcd51d21ed000001"
    "e22210426f6f5261646c657966613761"
    "366439380448e3dc959b06501258026a"
    "0072580a109ab3fb7eaffc5ee087568b"
    "9581ff0e721210777bc13c94c4568b84"
    "ddaf99de3f03311a2042949649205d0d"
    "f4f4a4eec94263561931066a9fc18b61"
    "8b3929a168b4c2404222101e7aad142f"
    "59edb962816e8d0702356b72580a100f"
    "ba9bd0f8e055d5900bbfaabf50331412"
    "10537cb8f017ec59e7be3c309e6d7f4b"
    "5d1a2089c49c0f0e214c3765cbb37ab5"
    "41dc0625c0087fa94317528ee8265431"
    "71cabe2210dddba401d4eaaa76437638"
    "a29dbdc38472580a109fec6ad1d36e56"
    "cdaf5c232480f63f881210a0396729b3"
    "795b46a5ea7f9919b96a671a209245a1"
    "c821de9c65fb1086d9af8aaca4b7da0e"
    "bbd0850a56ab36c23bb71b507e2210b0"
    "531d2235575ff9ba5af4545bb43fdd72"
    "580a10cd2e9ebf89c257e2a3196c82ac"
    "4c76ba121011647820b33352349942cd"
    "31ce352e571a20277b4392c76a04335d"
    "3eadf22705184eb1adc057a61e372e78"
    "30b7a20361d2472210d954557dc853a7"
    "42283e6dfe16677a6a72580a10b3fd79"
    "204b9d5cbf8f672eca27255e9e1210c4"
    "78fb09c0c6531b92c571972c36098b1a"
    "20bf17c678ac01685e258192eb4d2d49"
    "157c3a07a95342d8be8b2f9f121f596b"
    "8622100d9cfe972bc17003b49ecd5f45"
    "f3bb28")
};

std::string kProviderSessionTokenStreamingClip3 = wvcdm::a2bs_hex(
    "4851305A4A4156485A554936444E4931");
std::string kProviderSessionTokenStreamingClip4 = wvcdm::a2bs_hex(
    "4942524F4355544E5557553145463243");
std::string kProviderSessionTokenStreamingClip7 = wvcdm::a2bs_hex(
    "44434C53524F4E30394C4E5535544B4C");
std::string kProviderSessionTokenStreamingClip20 = wvcdm::a2bs_hex(
    "4851305A4A4156485A554936444E4931");
std::string kProviderSessionTokenStreamingClip21 = wvcdm::a2bs_hex(
    "4851305A4A4156485A554936444E4931");

// playback duration is 10 seconds+uncertainty window
const std::chrono::milliseconds
    kExpirationStreamingClip21PlaybackDurationTimeMs =
        std::chrono::milliseconds(12*1000);

UsageLicenseAndSubSampleInfo kUsageLicenseTestVector1[] = {
    { kPsshStreamingClip3, &usage_info_sub_samples_icp[0],
      kProviderSessionTokenStreamingClip3 },
    { kPsshStreamingClip4, &usage_info_sub_samples_icp[1],
      kProviderSessionTokenStreamingClip4 },
};

UsageLicenseAndSubSampleInfo kUsageLicenseTestVector2[] = {
    { kPsshStreamingClip7, &usage_info_sub_samples_icp[4],
      kProviderSessionTokenStreamingClip7 },
    // TODO(rfrias): Add another streaming usage license. Streaming
    // clip 5 has includes a randomly generated PST, while
    // streaming clip 6 does not include a PST.
};

struct RenewWithClientIdTestConfiguration {
  bool always_include_client_id;
  bool specify_app_parameters;
  bool enable_privacy_mode;
  bool specify_service_certificate;
  std::string test_description;
};

void PrintTo(RenewWithClientIdTestConfiguration* param, std::ostream* os) {
  if (param) {
    *os << param->test_description;
  } else {
    *os << "<null ptr>";
  }
}

RenewWithClientIdTestConfiguration
    streaming_renew_client_id_test_configuration[] = {
        {false, false, false, false,
         "Test: Streaming renewal without client Id"},
        {true, false, false, false, "Test: Streaming renewal with client Id"},
        {true, true, false, false,
         "Test: Streaming renewal with app parameters"},
        {true, false, true, false,
         "Test: Streaming renewal fetch service cert"},
        {true, false, true, true,
         "Test: Streaming renewal, service cert provided"}};

// Note: Offline renewal/release with encrypted client Ids and where a service
// certificate needs to be fetched is not supported.
RenewWithClientIdTestConfiguration
    offline_release_client_id_test_configuration[] = {
        {false, false, false, false,
         "Test: Offline renewal/release without client Id"},
        {true, false, false, false,
         "Test: Offline renewal/release with client Id"},
        {true, true, false, false,
         "Test: Offline renewal/release with app parameters"},
        {true, false, true, true,
         "Test: Offline renewal/release, service cert provided"}};

RenewWithClientIdTestConfiguration
    usage_client_id_test_configuration[] = {
        {false, false, false, false,
         "Test: Usage reporting without client Id"},
        {true, false, false, false,
         "Test: Usage reporting with client Id"}};

struct EntitlementTestConfiguration {
  std::string entitlement_pssh;
  std::string key_rotation_pssh;
  SubSampleInfo* sub_sample_with_initial_keys;
  SubSampleInfo* sub_sample_with_rotated_keys;
};

EntitlementTestConfiguration kEntitlementTestConfiguration[] = {
    {// Single Widevine PSSH containing PSSH data of type ENTITLED_KEY
     kSinglePsshEntitlementWithKeyRotation[0],
     kSinglePsshEntitlementWithKeyRotation[1],
     &kEntitlementWithKeyRotationSubSampleSinglePssh[0],
     &kEntitlementWithKeyRotationSubSampleSinglePssh[1]},
    {// Two Widevine PSSHs containing PSSH data of type SINGLE, ENTITLED_KEY
     kDualPsshEntitlementWithKeyRotation[0],
     kDualPsshEntitlementWithKeyRotation[1],
     &kEntitlementWithKeyRotationSubSampleDualPssh[0],
     &kEntitlementWithKeyRotationSubSampleDualPssh[1]},
};

// provider:"widevine_test",
// content_id":"aGxzX3NhbXBsZV9hZXNfc3RyZWFtaW5n" (hls_sample_aes_streaming)
// key_id:613db35603320eb8e7ea24bdeea3fdb8
// key:78a1dc0646119707e903514d8a00735f
const std::string kAttributeListSampleAes =
    "#EXT-X-KEY:"
    "METHOD=SAMPLE-AES,"
    "KEYFORMAT=\"com.widevine\","
    "KEYFORMATVERSIONS=\"1\","
    "URI=\"data:text/"
    "plain;base64,"
    "ew0KICAgInByb3ZpZGVyIjoid2lkZXZpbmVfdGVzdCIsDQogICAiY29udGVudF9pZCI6ImFHeH"
    "pYM05oYlhCc1pWOWhaWE5mYzNSeVpXRnRhVzVuIiwNCiAgICJrZXlfaWRzIjoNCiAgIFsNCiAg"
    "ICAgICI2MTNkYjM1NjAzMzIwZWI4ZTdlYTI0YmRlZWEzZmRiOCINCiAgIF0NCn0=\","
    "IV=0x9FBE45DD47DA7EBA09A3E24CBA95C9AF";

// provider:"widevine_test",
// content_id":"aGxzX2Flc18xMjhfc3RyZWFtaW5n" (hls_aes_128_streaming)
// key_id:ab6531ff6e6ea15e387b019e59c2de0a
// key:78a1dc0646119707e903514d8a00735f
const std::string kAttributeListAes128 =
    "#EXT-X-KEY:"
    "METHOD=AES-128,"
    "KEYFORMAT=\"com.widevine\","
    "KEYFORMATVERSIONS=\"1\","
    "URI=\"data:text/"
    "plain;base64,"
    "ew0KICAgInByb3ZpZGVyIjoid2lkZXZpbmVfdGVzdCIsDQogICAiY29udGVudF9pZCI6ImFHeH"
    "pYMkZsYzE4eE1qaGZjM1J5WldGdGFXNW4iLA0KICAgImtleV9pZHMiOg0KICAgWw0KICAgICAg"
    "ImFiNjUzMWZmNmU2ZWExNWUzODdiMDE5ZTU5YzJkZTBhIg0KICAgXQ0KfQ==\","
    "IV=0x9FBE45DD47DA7EBA09A3E24CBA95C9AF";

const std::string kAttributeListKeyFormatNonWidevine =
    "#EXT-X-KEY:METHOD=SAMPLE-AES,KEYFORMAT=\"com.example\",KEYFORMATVERSIONS="
    "\"1\",URI=\"data:text/"
    "plain;base64,"
    "eyAKICAgInByb3ZpZGVyIjoiY2FzdCIsCiAgICJjb250ZW50X2lkIjoiU0VKUElGTkJUVkJNUl"
    "NBeCIsCiAgICJrZXlfaWRzIjoKICAgWwogICAgICAiZDhlMzQ0ODM1ZTM1NzA3MjUzZDU3MWYz"
    "NjE0ZTdmMmYiCiAgIF0KfQ==\",IV=0x9FBE45DD47DA7EBA09A3E24CBA95C9AF";

const std::string kAttributeListKeyFormatVersionUnregonized =
    "#EXT-X-KEY:METHOD=SAMPLE-AES,KEYFORMAT=\"com.widevine\",KEYFORMATVERSIONS="
    "\"2\",URI=\"data:text/"
    "plain;base64,"
    "eyAKICAgInByb3ZpZGVyIjoiY2FzdCIsCiAgICJjb250ZW50X2lkIjoiU0VKUElGTkJUVkJNUl"
    "NBeCIsCiAgICJrZXlfaWRzIjoKICAgWwogICAgICAiZDhlMzQ0ODM1ZTM1NzA3MjUzZDU3MWYz"
    "NjE0ZTdmMmYiCiAgIF0KfQ==\",IV=0x9FBE45DD47DA7EBA09A3E24CBA95C9AF";

const std::string kAttributeListUnspecifiedIv =
    "#EXT-X-KEY:"
    "METHOD=SAMPLE-AES,"
    "KEYFORMAT=\"com.widevine\","
    "KEYFORMATVERSIONS=\"1\","
    "URI=\"data:text/plain;base64,ew0KICAgInByb3ZpZGVyIjoid2lkZXZpbmVfdGVzdCIsD"
    "QogICAiY29udGVudF9pZCI6ImFHeHpYM05oYlhCc1pWOWhaWE5mYzNSeVpXRnRhVzVuIiwNCiA"
    "gICJrZXlfaWRzIjoNCiAgIFsNCiAgICAgICI3OGExZGMwNjQ2MTE5NzA3ZTkwMzUxNGQ4YTAwN"
    "zM1ZiINCiAgIF0NCn0=\",";

const std::string kAttributeListUnspecifiedMethod =
    "#EXT-X-KEY:"
    "KEYFORMAT=\"com.widevine\","
    "KEYFORMATVERSIONS=\"1\","
    "URI=\"data:text/plain;base64,ew0KICAgInByb3ZpZGVyIjoid2lkZXZpbmVfdGVzdCIsD"
    "QogICAiY29udGVudF9pZCI6ImFHeHpYM05oYlhCc1pWOWhaWE5mYzNSeVpXRnRhVzVuIiwNCiA"
    "gICJrZXlfaWRzIjoNCiAgIFsNCiAgICAgICI3OGExZGMwNjQ2MTE5NzA3ZTkwMzUxNGQ4YTAwN"
    "zM1ZiINCiAgIF0NCn0=\","
    "IV=0x9FBE45DD47DA7EBA09A3E24CBA95C9AF";

struct HlsSegmentInfo {
  bool start_segment;
  std::string iv;
  std::string clear_data;
  std::string encrypted_data;
  uint8_t subsample_flags;
};

HlsSegmentInfo kAes128SingleSegmentInfo[] = {
    {
        true, wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
        wvcdm::a2bs_hex(
            "AD868A6AC55C10D564FC23B8ACFF407DAAF4ED2743520E02CDA9680D9EA88E9148"
            "84604C8DA8A53CE33DB9FF8F1C5BB6BB97F37B39906BF41596555C1BCCE9ED0293"
            "59C4CF5906B6AB5BF60FBB3F1A1C7C59ACFC7E4FB4AD8E623C04D503A3DDAD868A"
            "6AC55C10D564FC23B8ACFF407DAAF4ED2743520E02CDA9680D9EA88E918DF21FDC"
            "42F166880D97A2225CD5C9EA5E7B752F4CF81BBDBE98E542EE10E1C6595D259A1E"
            "4A8B6D7744CD98C5D3F921ADC252EB7D8AF6B916044B676A574747942AC20A89FF"
            "79F4C2F25FBA99D6A44618A8C0420B27D54E3DA17B77C9D43CCA217CE9BDE99BD9"
            "1E9733A1A00B9B557AC3A433DC92633546156817FAE26B6E1C"),
        wvcdm::a2bs_hex(
            "E7C566D86E98C36D2DCA54A966E7B469B6094B9201F200932F8EB5E1A94FB0B977"
            "FAB8DFDAD57C677E279615F4EAFA872CA8EFF83179E4DE2AB78E6B41A860C42203"
            "4B875AC282406E03AC01F2E407A55DE38C6C35707F34B3319646FA016A01CE9056"
            "E55D28C48ED72F10FA6625656ED62B758CBADA757DDC52533C9CBD54FC1A46F827"
            "CC7B69BA66AE19A15D725FCBB972B23C83F95C0F00F481A7C9AC8687B3A4AD15AD"
            "A2ABBB84D8E3CBEC3E8771720A35234070799173B37219127141922CBA8CB2DC48"
            "EC2477832D1AE477942DCDA93C0886AF72D71E56DA3D7737E49670B024639A195B"
            "7377C7F45A797C6E5DBB1BB2843DA3FC76043E33687BEF3172"),
        OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    },
};

HlsSegmentInfo kAes128PartialSegmentInfo[] = {
    {
        true, wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
        wvcdm::a2bs_hex(
            "AD868A6AC55C10D564FC23B8ACFF407DAAF4ED2743520E02CDA9680D9EA88E9148"
            "84604C8DA8A53CE33DB9FF8F1C5BB6BB97F37B39906BF41596555C1BCCE9ED0293"
            "59C4CF5906B6AB5BF60FBB3F1A1C7C59ACFC7E4FB4AD8E623C04D503A3DDAD868A"
            "6AC55C10D564FC23B8ACFF407DAAF4ED2743520E02CDA9680D9EA88E918DF21FDC"
            "42F166880D97A2225CD5C9EA5E7B752F4CF81BBDBE98E542EE10E1C6595D259A1E"
            "4A8B6D7744CD98C5D3F921ADC252EB7D8AF6B916044B676A574747942AC20A89FF"
            "79F4C2F25FBA99D6A44618A8C0420B27D54E3DA17B77C9D43CCA217CE9BDE99BD9"
            "1E9733A1A00B9B557AC3A433DC92633546156817FAE26B6E1C"),
        wvcdm::a2bs_hex(
            "E7C566D86E98C36D2DCA54A966E7B469B6094B9201F200932F8EB5E1A94FB0B977"
            "FAB8DFDAD57C677E279615F4EAFA872CA8EFF83179E4DE2AB78E6B41A860C42203"
            "4B875AC282406E03AC01F2E407A55DE38C6C35707F34B3319646FA016A01CE9056"
            "E55D28C48ED72F10FA6625656ED62B758CBADA757DDC52533C9CBD54FC1A46F827"
            "CC7B69BA66AE19A15D725FCBB972B23C83F95C0F00F481A7C9AC8687B3A4AD15AD"
            "A2ABBB84D8E3CBEC3E8771720A35234070799173B37219127141922CBA8CB2DC48"
            "EC2477832D1AE477942DCDA93C0886AF72D71E56DA3D7737E49670B024639A195B"
            "7377C7F45A797C6E5DBB1BB2843DA3FC76043E33687BEF3172"),
        OEMCrypto_FirstSubsample,
    },
    {
        false, wvcdm::a2bs_hex("BB1BB2843DA3FC76043E33687BEF3172"),
        wvcdm::a2bs_hex(
            "64ab17b3e3dfab47245c7cce4543d4fc7a26dcf248f19f9b59f3c92601440b3617"
            "c8ed0c96c656549e461f38708cd47a434066f8df28ccc28b79252eee3f9c2d7f6c"
            "68ebe40141fe818fe082ca523c03d69ddaf183a93c022327fedc5582c5abca9d34"
            "2b71263a67f9cb2336f12108aaaef464f17177e44e9b0c4e56e61da53c2150b440"
            "5cc82d994dfd9bf4087c761956d6688a9705db4cf350381085f383c49666d4aed1"
            "35c519c1f0b5cba06e287feea96ea367bf54e7368dcf998276c6e46497e0c50e20"
            "fef74e42cb518fe7f22ef27202428688f86404e8278587017012c1d65537c6cbd7"
            "dde04aae338d68115a9f430afc100ab83cdadf45dca39db685"),
        wvcdm::a2bs_hex(
            "5E61E95F918560A046682BB03CF3DBAB6E052150AEB3D46F458CF7480BF8F875E2"
            "833367415656E4BA2B01421A0B18C18F802657573BF0B239173114AFE992BCD852"
            "955301FAE562FC6BCC3AC89A4CA58BEC1BCCD07C89165DF77F29F42AE052858BA3"
            "AC59A82652E6B432CB4993C707755C32EEF137111160A086CFC4FB15E5F76B1DAF"
            "2E8217C51BDB4DE0B987288A68F88824DBD7BF4D2978147A4AE4DA019AAA2A59E9"
            "94C294768D86FDADED52931E7ACEBC915C8124785E4BE9CE89BFFA631C3F5E67C4"
            "EE5BA77A83B6E582DF2C1907FA572B61D656DCF9B8DEAE4B81508378732F952152"
            "CF7DCAB0A3CA3EE8E5F72D24E96647EBBAAA2394BD6EC458EF"),
        OEMCrypto_LastSubsample,
    },
};

HlsSegmentInfo kAes128MultiSegmentInfo[] = {
    {
        true, wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
        wvcdm::a2bs_hex(
            "AD868A6AC55C10D564FC23B8ACFF407DAAF4ED2743520E02CDA9680D9EA88E9148"
            "84604C8DA8A53CE33DB9FF8F1C5BB6BB97F37B39906BF41596555C1BCCE9ED0293"
            "59C4CF5906B6AB5BF60FBB3F1A1C7C59ACFC7E4FB4AD8E623C04D503A3DDAD868A"
            "6AC55C10D564FC23B8ACFF407DAAF4ED2743520E02CDA9680D9EA88E918DF21FDC"
            "42F166880D97A2225CD5C9EA5E7B752F4CF81BBDBE98E542EE10E1C6595D259A1E"
            "4A8B6D7744CD98C5D3F921ADC252EB7D8AF6B916044B676A574747942AC20A89FF"
            "79F4C2F25FBA99D6A44618A8C0420B27D54E3DA17B77C9D43CCA217CE9BDE99BD9"
            "1E9733A1A00B9B557AC3A433DC92633546156817FAE26B6E1C"),
        wvcdm::a2bs_hex(
            "E7C566D86E98C36D2DCA54A966E7B469B6094B9201F200932F8EB5E1A94FB0B977"
            "FAB8DFDAD57C677E279615F4EAFA872CA8EFF83179E4DE2AB78E6B41A860C42203"
            "4B875AC282406E03AC01F2E407A55DE38C6C35707F34B3319646FA016A01CE9056"
            "E55D28C48ED72F10FA6625656ED62B758CBADA757DDC52533C9CBD54FC1A46F827"
            "CC7B69BA66AE19A15D725FCBB972B23C83F95C0F00F481A7C9AC8687B3A4AD15AD"
            "A2ABBB84D8E3CBEC3E8771720A35234070799173B37219127141922CBA8CB2DC48"
            "EC2477832D1AE477942DCDA93C0886AF72D71E56DA3D7737E49670B024639A195B"
            "7377C7F45A797C6E5DBB1BB2843DA3FC76043E33687BEF3172"),
        OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    },
    {
        true, wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
        wvcdm::a2bs_hex(
            "64AB17B3E3DFAB47245C7CCE4543D4FC7A26DCF248F19F9B59F3C92601440B3617"
            "C8ED0C96C656549E461F38708CD47A434066F8DF28CCC28B79252EEE3F9C2D7F6C"
            "68EBE40141FE818FE082CA523C03D69DDAF183A93C022327FEDC5582C5ABCA9D34"
            "2B71263A67F9CB2336F12108AAAEF464F17177E44E9B0C4E56E61DA53C2150B440"
            "5CC82D994DFD9BF4087C761956D6688A9705DB4CF350381085F383C49666D4AED1"
            "35C519C1F0B5CBA06E287FEEA96EA367BF54E7368DCF998276C6E46497E0C50E20"
            "FEF74E42CB518FE7F22EF27202428688F86404E8278587017012C1D65537C6CBD7"
            "DDE04AAE338D68115A9F430AFC100AB83CDADF45DCA39DB685"),
        wvcdm::a2bs_hex(
            "BDA1AF662A108B3D34421B001E589A64E378CBE5386C17999F5C9E7079F52F56E3"
            "EEC51C75BCD1C44FFE38709218C66B61A1CB8A7274E89F210D2D6C52905FC963DC"
            "3F8D79B4EC016642AE3C8AF8D3E322A6EABE4761902D4D79FA3D2D7B827FC5797B"
            "D909B5C14325643164915D528557F48A95EB3FD2437A7D553197559822AADE0A6A"
            "A617729FCC58AAF581E78F7C332ED85F3470707E29AFF7BD9D8E785798311CA74C"
            "C9FB1226EDF74A803B171A891DD56393FB1322D47988E1055095C5F35CF83DB5A3"
            "940D329B51DDC89EC463712E9B32AEA8877AFE8C5BED67AE700FFC4CCEFFBC4021"
            "0E7ECA01DFF357ADF4D73A80D2B0C46867B33A5605DFDBD06E"),
        OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    },
};

HlsSegmentInfo kSampleAes10ByteSegmentInfo[] = {
    {
        true, wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
        wvcdm::a2bs_hex("4884604C8DA8A53CE33D"),
        wvcdm::a2bs_hex("4884604C8DA8A53CE33D"),
        OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    },
};

HlsSegmentInfo kSampleAes16ByteSegmentInfo[] = {
    {
        true, wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
        wvcdm::a2bs_hex("4884604C8DA8A53CE33DB9FF8F1C5BB6"),
        wvcdm::a2bs_hex("8E6B884AB604495C675CA38F0425E23E"),
        OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    },
};

HlsSegmentInfo kSampleAes18ByteSegmentInfo[] = {
    {
        true, wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
        wvcdm::a2bs_hex("4884604C8DA8A53CE33DB9FF8F1C5BB6BB97"),
        wvcdm::a2bs_hex("8E6B884AB604495C675CA38F0425E23EBB97"),
        OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    },
};

HlsSegmentInfo kSampleAes160ByteSegmentInfo[] = {
    {
        true, wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
        wvcdm::a2bs_hex("4884604C8DA8A53CE33DB9FF8F1C5BB6BB97F37B39906BF4159655"
                        "5C1BCCE9ED029359C4CF5906B6AB5BF60FBB3F1A1C7C59ACFC7E4F"
                        "B4AD8E623C04D503A3DDAD868A6AC55C10D564FC23B8ACFF407DAA"
                        "F4ED2743520E02CDA9680D9EA88E918DF21FDC42F166880D97A222"
                        "5CD5C9EA5E7B752F4CF81BBDBE98E542EE10E1C6595D259A1E4A8B"
                        "6D7744CD98C5D3F921ADC252EB7D8AF6B916044B676A574747"),
        wvcdm::a2bs_hex("8E6B884AB604495C675CA38F0425E23EBB97F37B39906BF4159655"
                        "5C1BCCE9ED029359C4CF5906B6AB5BF60FBB3F1A1C7C59ACFC7E4F"
                        "B4AD8E623C04D503A3DDAD868A6AC55C10D564FC23B8ACFF407DAA"
                        "F4ED2743520E02CDA9680D9EA88E918DF21FDC42F166880D97A222"
                        "5CD5C9EA5E7B752F4CF81BBDBE98E542EE10E1C6595D259A1E4A8B"
                        "6D7744CD98C5D3F921ADC252EB7D8AF6B916044B676A574747"),
        OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    },
};

HlsSegmentInfo kSampleAes175ByteSegmentInfo[] = {
    {
        true, wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
        wvcdm::a2bs_hex(
              "4884604C8DA8A53CE33DB9FF8F1C5BB6BB97F37B39906BF41596555C1BCC"
              "E9ED029359C4CF5906B6AB5BF60FBB3F1A1C7C59ACFC7E4FB4AD8E623C04"
              "D503A3DDAD868A6AC55C10D564FC23B8ACFF407DAAF4ED2743520E02CDA9"
              "680D9EA88E918DF21FDC42F166880D97A2225CD5C9EA5E7B752F4CF81BBD"
              "BE98E542EE10E1C6595D259A1E4A8B6D7744CD98C5D3F921ADC252EB7D8A"
              "F6B916044B676A574747942AC20A89FF79F4C2F25FBA99D6A4"),
        wvcdm::a2bs_hex("8E6B884AB604495C675CA38F0425E23EBB97F37B39906BF4159655"
                        "5C1BCCE9ED029359C4CF5906B6AB5BF60FBB3F1A1C7C59ACFC7E4F"
                        "B4AD8E623C04D503A3DDAD868A6AC55C10D564FC23B8ACFF407DAA"
                        "F4ED2743520E02CDA9680D9EA88E918DF21FDC42F166880D97A222"
                        "5CD5C9EA5E7B752F4CF81BBDBE98E542EE10E1C6595D259A1E4A8B"
                        "6D7744CD98C5D3F921ADC252EB7D8AF6B916044B676A574747942A"
                        "C20A89FF79F4C2F25FBA99D6A4"),
        OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    },
};

HlsSegmentInfo kSampleAes176ByteSegmentInfo[] = {
    {
        true, wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
        wvcdm::a2bs_hex(
                  "4884604C8DA8A53CE33DB9FF8F1C5BB6BB97F37B39906BF41596555C1BCC"
                  "E9ED029359C4CF5906B6AB5BF60FBB3F1A1C7C59ACFC7E4FB4AD8E623C04"
                  "D503A3DDAD868A6AC55C10D564FC23B8ACFF407DAAF4ED2743520E02CDA9"
                  "680D9EA88E918DF21FDC42F166880D97A2225CD5C9EA5E7B752F4CF81BBD"
                  "BE98E542EE10E1C6595D259A1E4A8B6D7744CD98C5D3F921ADC252EB7D8A"
                  "F6B916044B676A574747942AC20A89FF79F4C2F25FBA99D6A446"),
        wvcdm::a2bs_hex("8E6B884AB604495C675CA38F0425E23EBB97F37B39906BF4159655"
                        "5C1BCCE9ED029359C4CF5906B6AB5BF60FBB3F1A1C7C59ACFC7E4F"
                        "B4AD8E623C04D503A3DDAD868A6AC55C10D564FC23B8ACFF407DAA"
                        "F4ED2743520E02CDA9680D9EA88E918DF21FDC42F166880D97A222"
                        "5CD5C9EA5E7B752F4CF81BBDBE98E542EE10E1C6595D259A1E4A8B"
                        "6D7744CD98C5D3F921ADC252EB7D8AF6B916044B676A574747FBDC"
                        "189C42C3C213A71DBCF73A05A1A1"),
        OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    },
};

HlsSegmentInfo kSampleAes192ByteSegmentInfo[] = {
    {
        true, wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
        wvcdm::a2bs_hex(
            "4884604C8DA8A53CE33DB9FF8F1C5BB6BB97F37B39906BF41596555C1BCCE9ED02"
            "9359C4CF5906B6AB5BF60FBB3F1A1C7C59ACFC7E4FB4AD8E623C04D503A3DDAD86"
            "8A6AC55C10D564FC23B8ACFF407DAAF4ED2743520E02CDA9680D9EA88E918DF21F"
            "DC42F166880D97A2225CD5C9EA5E7B752F4CF81BBDBE98E542EE10E1C6595D259A"
            "1E4A8B6D7744CD98C5D3F921ADC252EB7D8AF6B916044B676A574747942AC20A89"
            "FF79F4C2F25FBA99D6A44618A8C0420B27D54E3DA17B77C9D43CCA"),
        wvcdm::a2bs_hex(
            "8E6B884AB604495C675CA38F0425E23EBB97F37B39906BF41596555C1BCCE9ED02"
            "9359C4CF5906B6AB5BF60FBB3F1A1C7C59ACFC7E4FB4AD8E623C04D503A3DDAD86"
            "8A6AC55C10D564FC23B8ACFF407DAAF4ED2743520E02CDA9680D9EA88E918DF21F"
            "DC42F166880D97A2225CD5C9EA5E7B752F4CF81BBDBE98E542EE10E1C6595D259A"
            "1E4A8B6D7744CD98C5D3F921ADC252EB7D8AF6B916044B676A574747FBDC189C42"
            "C3C213A71DBCF73A05A1A118A8C0420B27D54E3DA17B77C9D43CCA"),
        OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    },
};

HlsSegmentInfo kSampleAes338ByteSegmentInfo[] = {
    {
        true, wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
        wvcdm::a2bs_hex(
            "4884604C8DA8A53CE33DB9FF8F1C5BB6BB97F37B39906BF41596555C1BCCE9ED02"
            "9359C4CF5906B6AB5BF60FBB3F1A1C7C59ACFC7E4FB4AD8E623C04D503A3DDAD86"
            "8A6AC55C10D564FC23B8ACFF407DAAF4ED2743520E02CDA9680D9EA88E918DF21F"
            "DC42F166880D97A2225CD5C9EA5E7B752F4CF81BBDBE98E542EE10E1C6595D259A"
            "1E4A8B6D7744CD98C5D3F921ADC252EB7D8AF6B916044B676A574747942AC20A89"
            "FF79F4C2F25FBA99D6A44618A8C0420B27D54E3DA17B77C9D43CCA217CE9BDE99B"
            "D91E9733A1A00B9B557AC3A433DC92633546156817FAE26B6E1C942AC20A89FF79"
            "F4C2F25FBA99D6A44618A8C0420B27D54E3DA17B77C9D43CCAE7C566D86E98C36D"
            "2DCA54A966E7B469B6094B9201F200932F8EB5E1A94FB0B977FAB8DFDAD57C677E"
            "279615F4EAFA872CA8EFF83179E4DE2AB78E6B41A860C422034B875AC282406E03"
            "AC01F2E407A55DE3"),
        wvcdm::a2bs_hex(
            "8E6B884AB604495C675CA38F0425E23EBB97F37B39906BF41596555C1BCCE9ED02"
            "9359C4CF5906B6AB5BF60FBB3F1A1C7C59ACFC7E4FB4AD8E623C04D503A3DDAD86"
            "8A6AC55C10D564FC23B8ACFF407DAAF4ED2743520E02CDA9680D9EA88E918DF21F"
            "DC42F166880D97A2225CD5C9EA5E7B752F4CF81BBDBE98E542EE10E1C6595D259A"
            "1E4A8B6D7744CD98C5D3F921ADC252EB7D8AF6B916044B676A574747FBDC189C42"
            "C3C213A71DBCF73A05A1A118A8C0420B27D54E3DA17B77C9D43CCA217CE9BDE99B"
            "D91E9733A1A00B9B557AC3A433DC92633546156817FAE26B6E1C942AC20A89FF79"
            "F4C2F25FBA99D6A44618A8C0420B27D54E3DA17B77C9D43CCAE7C566D86E98C36D"
            "2DCA54A966E7B469B6094B9201F200932F8EB5E1A94FB0B977FAB8DFDAD57C677E"
            "279615F4EAFA872CA8EFF83179E4DE2AB78E6B41A860C44A27BD03EAB2D3CFCB2C"
            "F4F5AA93E7025DE3"),
        OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    },
};

struct HlsDecryptionInfo {
  bool sample_aes;
  size_t number_of_segments;
  HlsSegmentInfo* segment_info;
  const std::string& attribute_list;
};

HlsDecryptionInfo kHlsDecryptionTestVectors[] = {
    {false, 1, &kAes128SingleSegmentInfo[0], kAttributeListAes128},
    {false, 2, &kAes128PartialSegmentInfo[0], kAttributeListAes128},
    {false, 2, &kAes128MultiSegmentInfo[0], kAttributeListAes128},
    {true, 1, &kSampleAes10ByteSegmentInfo[0], kAttributeListSampleAes},
    {true, 1, &kSampleAes16ByteSegmentInfo[0], kAttributeListSampleAes},
    {true, 1, &kSampleAes18ByteSegmentInfo[0], kAttributeListSampleAes},
    {true, 1, &kSampleAes160ByteSegmentInfo[0], kAttributeListSampleAes},
    {true, 1, &kSampleAes175ByteSegmentInfo[0], kAttributeListSampleAes},
    {true, 1, &kSampleAes176ByteSegmentInfo[0], kAttributeListSampleAes},
    {true, 1, &kSampleAes192ByteSegmentInfo[0], kAttributeListSampleAes},
    {true, 1, &kSampleAes338ByteSegmentInfo[0], kAttributeListSampleAes},
};

const std::string kHlsPsshAes128LittleEndianFourCC = wvcdm::a2bs_hex(
    "00000060"                          // blob size
    "70737368"                          // "pssh"
    "00000000"                          // flags
    "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
    "00000040"                          // pssh data size
    // pssh data:
    "08011210AB6531FF6E6EA15E387B019E"
    "59C2DE0A1A0D7769646576696E655F74"
    "6573742215686C735F6165735F313238"
    "5F73747265616D696E6748E3C48D8B03");
const std::string kHlsPsshSampleAesLittleEndianFourCC = wvcdm::a2bs_hex(
    "00000063"                          // blob size
    "70737368"                          // "pssh"
    "00000000"                          // flags
    "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
    "00000043"                          // pssh data size
    // pssh data:
    "08011210613DB35603320EB8E7EA24BD"
    "EEA3FDB81A0D7769646576696E655F74"
    "6573742218686C735F73616D706C655F"
    "6165735F73747265616D696E6748E3C4"
    "8D9B07");
const std::string kHlsPsshAes128FourCC = wvcdm::a2bs_hex(
    "00000060"                          // blob size
    "70737368"                          // "pssh"
    "00000000"                          // flags
    "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
    "00000040"                          // pssh data size
    // pssh data:
    "08011210AB6531FF6E6EA15E387B019E"
    "59C2DE0A1A0D7769646576696E655F74"
    "6573742215686C735F6165735F313238"
    "5F73747265616D696E6748B1C6899B06");
const std::string kHlsPsshSampleAesFourCC = wvcdm::a2bs_hex(
    "00000063"                          // blob size
    "70737368"                          // "pssh"
    "00000000"                          // flags
    "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
    "00000043"                          // pssh data size
    // pssh data:
    "08011210613DB35603320EB8E7EA24BD"
    "EEA3FDB81A0D7769646576696E655F74"
    "6573742218686C735F73616D706C655F"
    "6165735F73747265616D696E6748F3C6"
    "899B06");

HlsDecryptionInfo kHlsFourCCBackwardCompatibilityTestVectors[] = {
    {false, 2, &kAes128MultiSegmentInfo[0], kHlsPsshAes128LittleEndianFourCC},
    {true, 1, &kSampleAes160ByteSegmentInfo[0],
     kHlsPsshSampleAesLittleEndianFourCC},
    {false, 2, &kAes128MultiSegmentInfo[0], kHlsPsshAes128FourCC},
    {true, 1, &kSampleAes160ByteSegmentInfo[0], kHlsPsshSampleAesFourCC},
};

const std::string kFullCencPssh = wvcdm::a2bs_hex(
    "00000044"                          // blob size
    "70737368"                          // "pssh"
    "00000000"                          // flags
    "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
    "00000024"                          // pssh data size
    "08011201321a0d7769646576696e655f"  // pssh data
    "74657374220a323031355f7465617273"
    "2a024844");

const std::string kFullCensPssh = wvcdm::a2bs_hex(
    "00000053"                          // blob size
    "70737368"                          // "pssh"
    "00000000"                          // flags
    "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
    "00000033"                          // pssh data size
    "12103030303030303030303030303030"  // pssh data
    "30321a0d7769646576696e655f746573"
    "74220a323031355f746561727348f3dc"
    "959b06");

const std::string kFullCbc1Pssh = wvcdm::a2bs_hex(
    "00000053"                         // blob size
    "70737368"                         // "pssh"
    "00000000"                         // flags
    "edef8ba979d64acea3c827dcd51d21ed" // Widevine system id
    "00000033"                         // pssh data size
    "12103030303030303030303030303030" // pssh data
    "30321a0d7769646576696e655f746573"
    "74220a323031355f746561727348b1c6"
    "899b06");


const std::string kFullCbcsPssh = wvcdm::a2bs_hex(
    "00000053"                          // blob size
    "70737368"                          // "pssh"
    "00000000"                          // flags
    "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
    "00000033"                          // pssh data size
    "12103030303030303030303030303030"
    "30321a0d7769646576696e655f746573"
    "74220a323031355f746561727348f3c6"
    "899b06");

struct Cenc30SampleInfo {
  bool is_encrypted;
  wvcdm::KeyId key_id;
  std::string iv;
  std::string clear_data;
  std::string encrypted_data;
  uint8_t subsample_flags;
  wvcdm::CdmCipherMode cipher_mode;
};

Cenc30SampleInfo kCenc30CencKey33Sample = {
    true, wvcdm::a2bs_hex("30303030303030303030303030303033"),
    wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
    wvcdm::a2bs_hex(
        "011E88387D58EBB8E5CDCC38D431EEF4B6094B9201F200932F8EB5E1A94FB0B977"
        "FAB8DFDAD57C677E279615F4EAFA872CA8EFF83179E4DE2AB78E6B41A860C42203"
        "4B875AC282406E03AC01F2E407A55DE38C6C35707F34B3319646FA016A01CE9056"
        "E55D28C48ED72F10FA6625656ED62B758CBADA757DDC52533C9CBD54FC1A46F827"
        "CC7B69BA66AE19A15D725FCBB972B23C83F95C0F00F481A7C9AC868701CB8BE038"
        "15BBFFB95FD3A86F142127720A35234070799173B37219127141922CBA8CB2DC48"
        "EC2477832D1AE477942DCDA93C0886AF72D71E56DA3D7737E49670B024639A195B"
        "7377C7F45A797C6E5DBB1BB2843DA3FC76043E33687BEF3172"),
    wvcdm::a2bs_hex(
        "E7C566D86E98C36D2DCA54A966E7B469B6094B9201F200932F8EB5E1A94FB0B977"
        "FAB8DFDAD57C677E279615F4EAFA872CA8EFF83179E4DE2AB78E6B41A860C42203"
        "4B875AC282406E03AC01F2E407A55DE38C6C35707F34B3319646FA016A01CE9056"
        "E55D28C48ED72F10FA6625656ED62B758CBADA757DDC52533C9CBD54FC1A46F827"
        "CC7B69BA66AE19A15D725FCBB972B23C83F95C0F00F481A7C9AC8687B3A4AD15AD"
        "A2ABBB84D8E3CBEC3E8771720A35234070799173B37219127141922CBA8CB2DC48"
        "EC2477832D1AE477942DCDA93C0886AF72D71E56DA3D7737E49670B024639A195B"
        "7377C7F45A797C6E5DBB1BB2843DA3FC76043E33687BEF3172"),
    OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    wvcdm::kCipherModeCtr,
};

Cenc30SampleInfo kCenc30CencKey32Sample = {
    true, wvcdm::a2bs_hex("30303030303030303030303030303032"),
    wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
    wvcdm::a2bs_hex(
        "1B605E32B31D6245BCCC01C4E7720725B6094B9201F200932F8EB5E1A94FB0B977"
        "FAB8DFDAD57C677E279615F4EAFA872CA8EFF83179E4DE2AB78E6B41A860C42203"
        "4B875AC282406E03AC01F2E407A55DE38C6C35707F34B3319646FA016A01CE9056"
        "E55D28C48ED72F10FA6625656ED62B758CBADA757DDC52533C9CBD54FC1A46F827"
        "CC7B69BA66AE19A15D725FCBB972B23C83F95C0F00F481A7C9AC8687DBFDF7F684"
        "3A552DCB7C38E461EDF5F3720A35234070799173B37219127141922CBA8CB2DC48"
        "EC2477832D1AE477942DCDA93C0886AF72D71E56DA3D7737E49670B024639A195B"
        "7377C7F45A797C6E5DBB1BB2843DA3FC76043E33687BEF3172"),
    wvcdm::a2bs_hex(
        "E7C566D86E98C36D2DCA54A966E7B469B6094B9201F200932F8EB5E1A94FB0B977"
        "FAB8DFDAD57C677E279615F4EAFA872CA8EFF83179E4DE2AB78E6B41A860C42203"
        "4B875AC282406E03AC01F2E407A55DE38C6C35707F34B3319646FA016A01CE9056"
        "E55D28C48ED72F10FA6625656ED62B758CBADA757DDC52533C9CBD54FC1A46F827"
        "CC7B69BA66AE19A15D725FCBB972B23C83F95C0F00F481A7C9AC8687B3A4AD15AD"
        "A2ABBB84D8E3CBEC3E8771720A35234070799173B37219127141922CBA8CB2DC48"
        "EC2477832D1AE477942DCDA93C0886AF72D71E56DA3D7737E49670B024639A195B"
        "7377C7F45A797C6E5DBB1BB2843DA3FC76043E33687BEF3172"),
    OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    wvcdm::kCipherModeCtr,
};

Cenc30SampleInfo kCenc30CensKey33Sample = {
    true, wvcdm::a2bs_hex("30303030303030303030303030303033"),
    wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
    wvcdm::a2bs_hex(
        "011E88387D58EBB8E5CDCC38D431EEF4B6094B9201F200932F8EB5E1A94FB0B977"
        "FAB8DFDAD57C677E279615F4EAFA872CA8EFF83179E4DE2AB78E6B41A860C42203"
        "4B875AC282406E03AC01F2E407A55DE38C6C35707F34B3319646FA016A01CE9056"
        "E55D28C48ED72F10FA6625656ED62B758CBADA757DDC52533C9CBD54FC1A46F827"
        "CC7B69BA66AE19A15D725FCBB972B23C83F95C0F00F481A7C9AC868701CB8BE038"
        "15BBFFB95FD3A86F142127720A35234070799173B37219127141922CBA8CB2DC48"
        "EC2477832D1AE477942DCDA93C0886AF72D71E56DA3D7737E49670B024639A195B"
        "7377C7F45A797C6E5DBB1BB2843DA3FC76043E33687BEF3172"),
    wvcdm::a2bs_hex(
        "E7C566D86E98C36D2DCA54A966E7B469B6094B9201F200932F8EB5E1A94FB0B977"
        "FAB8DFDAD57C677E279615F4EAFA872CA8EFF83179E4DE2AB78E6B41A860C42203"
        "4B875AC282406E03AC01F2E407A55DE38C6C35707F34B3319646FA016A01CE9056"
        "E55D28C48ED72F10FA6625656ED62B758CBADA757DDC52533C9CBD54FC1A46F827"
        "CC7B69BA66AE19A15D725FCBB972B23C83F95C0F00F481A7C9AC8687B3A4AD15AD"
        "A2ABBB84D8E3CBEC3E8771720A35234070799173B37219127141922CBA8CB2DC48"
        "EC2477832D1AE477942DCDA93C0886AF72D71E56DA3D7737E49670B024639A195B"
        "7377C7F45A797C6E5DBB1BB2843DA3FC76043E33687BEF3172"),
    OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    wvcdm::kCipherModeCtr,
};

Cenc30SampleInfo kCenc30Cbc1Key33Sample = {
    true, wvcdm::a2bs_hex("30303030303030303030303030303033"),
    wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
    wvcdm::a2bs_hex(
        "E300F37FEB0CDD9F276E67B971FF423003F3BF21DCF6100BA453A473A4522A19A8"
        "2E098AA25511011D386FC7092FE3B407DF2BEB3AD57D5E1178F041E3FCABE25193"
        "3F5EE35670CEB96BA3DAF922484F9A37773EF75D4B17FACC80B475004A6229AB4C"
        "DFFA426468E578DE6A0285D942CDE476E06FF907D03F382552C2E14399C3FC2D21"
        "9A59819FFF837EBC88A9F83A42D37F48ED8564EB40AC3BA8A6D2634A81F04FC2F1"
        "379A45869036FD72B39C88222646AB7486A8416D78AB75951EB87ED1E16DF69209"
        "A6966AC93C7BB65B85E429357A732CBC75F6EFB1781859FB771D60D11EB381D9CA"
        "63F793507B02207810773FCABED0240E5BEEAD30116014E481"),
    wvcdm::a2bs_hex(
        "E7C566D86E98C36D2DCA54A966E7B469B6094B9201F200932F8EB5E1A94FB0B977"
        "FAB8DFDAD57C677E279615F4EAFA872CA8EFF83179E4DE2AB78E6B41A860C42203"
        "4B875AC282406E03AC01F2E407A55DE38C6C35707F34B3319646FA016A01CE9056"
        "E55D28C48ED72F10FA6625656ED62B758CBADA757DDC52533C9CBD54FC1A46F827"
        "CC7B69BA66AE19A15D725FCBB972B23C83F95C0F00F481A7C9AC8687B3A4AD15AD"
        "A2ABBB84D8E3CBEC3E8771720A35234070799173B37219127141922CBA8CB2DC48"
        "EC2477832D1AE477942DCDA93C0886AF72D71E56DA3D7737E49670B024639A195B"
        "7377C7F45A797C6E5DBB1BB2843DA3FC76043E33687BEF3172"),
    OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    wvcdm::kCipherModeCbc,
};

Cenc30SampleInfo kCenc30Cbc1Key32Sample = {
    true, wvcdm::a2bs_hex("30303030303030303030303030303032"),
    wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
    wvcdm::a2bs_hex(
        "4392E38BAE263267ED15394DE349AD1577F37B7D906C3A61536EE5A288F66F22F2"
        "F5098964B7F2860A848C3C4FD30E538B3BCD2E700DC3FBC1657A6E9EAE44DE97C4"
        "6F27C82A49198EE185D92931F093C3342FDBF6CF8203E18CCDC4B88E79C95EC052"
        "3FD10F9409945349169FAA8F6A37179D2BEDC04A158A09BCBF742DA05245428788"
        "E972B9B465FED5849AEDDB74B8919673C0C8829B5B062A38B3146CB8D497F03A4D"
        "5C0A1D463504C1F116A811EF32503695B8FF78D9E93CDF7B2F7493E8043D4DE110"
        "FE1D342D1C0175BF1466A544FC0D02DD0E314098256DD65B48098323C3AED9B7E0"
        "CF260DBC5A0F09A46E39AE5E26A66ABFA52CBA26FBA83975E4"),
    wvcdm::a2bs_hex(
        "E7C566D86E98C36D2DCA54A966E7B469B6094B9201F200932F8EB5E1A94FB0B977"
        "FAB8DFDAD57C677E279615F4EAFA872CA8EFF83179E4DE2AB78E6B41A860C42203"
        "4B875AC282406E03AC01F2E407A55DE38C6C35707F34B3319646FA016A01CE9056"
        "E55D28C48ED72F10FA6625656ED62B758CBADA757DDC52533C9CBD54FC1A46F827"
        "CC7B69BA66AE19A15D725FCBB972B23C83F95C0F00F481A7C9AC8687B3A4AD15AD"
        "A2ABBB84D8E3CBEC3E8771720A35234070799173B37219127141922CBA8CB2DC48"
        "EC2477832D1AE477942DCDA93C0886AF72D71E56DA3D7737E49670B024639A195B"
        "7377C7F45A797C6E5DBB1BB2843DA3FC76043E33687BEF3172"),
    OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    wvcdm::kCipherModeCbc,
};

Cenc30SampleInfo kCenc30CbcsKey33Sample = {
    true, wvcdm::a2bs_hex("30303030303030303030303030303033"),
    wvcdm::a2bs_hex("9FBE45DD47DA7EBA09A3E24CBA95C9AF"),
    wvcdm::a2bs_hex(
        "E300F37FEB0CDD9F276E67B971FF423003F3BF21DCF6100BA453A473A4522A19A8"
        "2E098AA25511011D386FC7092FE3B407DF2BEB3AD57D5E1178F041E3FCABE25193"
        "3F5EE35670CEB96BA3DAF922484F9A37773EF75D4B17FACC80B475004A6229AB4C"
        "DFFA426468E578DE6A0285D942CDE476E06FF907D03F382552C2E14399C3FC2D21"
        "9A59819FFF837EBC88A9F83A42D37F48ED8564EB40AC3BA8A6D2634A81F04FC2F1"
        "379A45869036FD72B39C88222646AB7486A8416D78AB75951EB87ED1E16DF69209"
        "A6966AC93C7BB65B85E429357A732CBC75F6EFB1781859FB771D60D11EB381D9CA"
        "63F793507B02207810773FCABED0240E5BEEAD30116014E481"),
    wvcdm::a2bs_hex(
        "E7C566D86E98C36D2DCA54A966E7B469B6094B9201F200932F8EB5E1A94FB0B977"
        "FAB8DFDAD57C677E279615F4EAFA872CA8EFF83179E4DE2AB78E6B41A860C42203"
        "4B875AC282406E03AC01F2E407A55DE38C6C35707F34B3319646FA016A01CE9056"
        "E55D28C48ED72F10FA6625656ED62B758CBADA757DDC52533C9CBD54FC1A46F827"
        "CC7B69BA66AE19A15D725FCBB972B23C83F95C0F00F481A7C9AC8687B3A4AD15AD"
        "A2ABBB84D8E3CBEC3E8771720A35234070799173B37219127141922CBA8CB2DC48"
        "EC2477832D1AE477942DCDA93C0886AF72D71E56DA3D7737E49670B024639A195B"
        "7377C7F45A797C6E5DBB1BB2843DA3FC76043E33687BEF3172"),
    OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample,
    wvcdm::kCipherModeCbc,
};

struct SingleSampleDecryptionInfo {
  const std::string pssh;
  Cenc30SampleInfo sample_info;
};

SingleSampleDecryptionInfo kCenc30DecryptionData[4] = {
    { kFullCencPssh, kCenc30CencKey33Sample },
    { kFullCensPssh, kCenc30CensKey33Sample },
    { kFullCbc1Pssh, kCenc30Cbc1Key33Sample },
    { kFullCbcsPssh, kCenc30CbcsKey33Sample },
};

struct FourSampleDecryptionInfo {
  const std::string pssh;
  Cenc30SampleInfo sample_info[4];
};

FourSampleDecryptionInfo kCenc30SwitchCipherData[8] = {
    // Switch between cipher modes
    { kFullCencPssh, { kCenc30CencKey33Sample, kCenc30Cbc1Key33Sample,
                       kCenc30CencKey33Sample, kCenc30Cbc1Key33Sample, } },
    { kFullCbc1Pssh, { kCenc30Cbc1Key33Sample, kCenc30CencKey33Sample,
                       kCenc30Cbc1Key33Sample, kCenc30CencKey33Sample, } },
    // Switch between cipher modes, but the first sample has a cipher mode
    // that differs with the protection scheme in the pssh
    { kFullCencPssh, { kCenc30Cbc1Key33Sample, kCenc30CencKey33Sample,
                       kCenc30Cbc1Key33Sample, kCenc30CencKey33Sample, } },
    { kFullCbc1Pssh, { kCenc30CencKey33Sample, kCenc30Cbc1Key33Sample,
                       kCenc30CencKey33Sample, kCenc30Cbc1Key33Sample, } },
    // Switch between cipher modes and keys
    { kFullCencPssh, { kCenc30CencKey33Sample, kCenc30CencKey32Sample,
                       kCenc30Cbc1Key33Sample, kCenc30Cbc1Key32Sample, } },
    { kFullCencPssh, { kCenc30Cbc1Key33Sample, kCenc30Cbc1Key32Sample,
                       kCenc30CencKey33Sample, kCenc30CencKey32Sample, } },
    { kFullCbc1Pssh, { kCenc30Cbc1Key33Sample, kCenc30Cbc1Key32Sample,
                       kCenc30CencKey33Sample, kCenc30CencKey32Sample, } },
    { kFullCbc1Pssh, { kCenc30CencKey33Sample, kCenc30CencKey32Sample,
                       kCenc30Cbc1Key33Sample, kCenc30Cbc1Key32Sample, } },
};

}  // namespace

namespace wvcdm {
// Protobuf generated classes
using video_widevine::ClientIdentification;
using video_widevine::ClientIdentification_NameValue;
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
    if (!security_level.compare(QUERY_VALUE_SECURITY_LEVEL_L1) ||
        !security_level.compare(QUERY_VALUE_SECURITY_LEVEL_L3)) {
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

class TestWvCdmEventListener : public WvCdmEventListener {
 public:
  TestWvCdmEventListener() : WvCdmEventListener() {}
  MOCK_METHOD1(OnSessionRenewalNeeded, void(const CdmSessionId& session_id));
  MOCK_METHOD3(OnSessionKeysChange, void(const CdmSessionId& session_id,
                                         const CdmKeyStatusMap& keys_status,
                                         bool has_new_usable_key));
  MOCK_METHOD2(OnExpirationUpdate, void(const CdmSessionId& session_id,
                                        int64_t new_expiry_time_seconds));
};

class DecryptCallbackTester {
 public:
  DecryptCallbackTester(wvcdm::WvContentDecryptionModule* decryptor,
                        SubSampleInfo* sub_sample_info)
   : decryptor_(decryptor),
     sub_sample_info_(sub_sample_info) {}

  void Decrypt(const CdmSessionId& session_id,
               const CdmKeyStatusMap& /* keys_status */,
               bool /* has_new_usable_key */) {
    EXPECT_TRUE(decryptor_ != NULL);
    EXPECT_TRUE(sub_sample_info_ != NULL);

    std::vector<uint8_t> decrypt_buffer(sub_sample_info_->encrypt_data.size());
  CdmDecryptionParameters decryption_parameters(
      &sub_sample_info_->key_id, &sub_sample_info_->encrypt_data.front(),
      sub_sample_info_->encrypt_data.size(), &sub_sample_info_->iv,
      sub_sample_info_->block_offset, &decrypt_buffer[0]);
  decryption_parameters.is_encrypted = sub_sample_info_->is_encrypted;
  decryption_parameters.is_secure = sub_sample_info_->is_secure;
  decryption_parameters.subsample_flags = sub_sample_info_->subsample_flags;
  EXPECT_EQ(NO_ERROR, decryptor_->Decrypt(session_id,
                                          sub_sample_info_->validate_key_id,
                                          decryption_parameters));

  }
 private:
  wvcdm::WvContentDecryptionModule* decryptor_;
  SubSampleInfo* sub_sample_info_;
};

class TestWvCdmHlsEventListener : public WvCdmEventListener {
 public:
  TestWvCdmHlsEventListener() : WvCdmEventListener() {}
  virtual void OnSessionRenewalNeeded(const CdmSessionId&) {}
  virtual void OnSessionKeysChange(const CdmSessionId&,
                                   const CdmKeyStatusMap& keys_status,
                                   bool) {
    key_status_map_ = keys_status;
  }

  virtual void OnExpirationUpdate(const CdmSessionId&,
                                  int64_t) {}

  CdmKeyStatusMap GetKeyStatusMap() { return key_status_map_; }
 private:
  CdmKeyStatusMap key_status_map_;
};

class WvCdmRequestLicenseTest : public WvCdmTestBase {
 public:
  WvCdmRequestLicenseTest() : license_type_(kLicenseTypeStreaming) {}
  ~WvCdmRequestLicenseTest() {}

 protected:
  void GetOfflineConfiguration(std::string* key_id, std::string* client_auth) {
    ConfigTestEnv config(config_.server_id(), false);
    if (binary_key_id().compare(a2bs_hex(config_.key_id())) == 0)
      key_id->assign(wvcdm::a2bs_hex(config.key_id()));
    else
      key_id->assign(binary_key_id());

    client_auth->assign(config.client_auth());
  }

  void GenerateKeyRequest(const std::string& init_data,
                          CdmLicenseType license_type) {
    GenerateKeyRequest(init_data, license_type, NULL);
  }

  void GenerateKeyRequest(const std::string& init_data,
                          CdmLicenseType license_type,
                          const CdmIdentifier& identifier) {
    CdmAppParameterMap app_parameters;
    GenerateKeyRequest(wvcdm::KEY_MESSAGE, "video/mp4", init_data,
                       app_parameters, license_type, identifier, NULL);
  }

  void GenerateKeyRequest(const std::string& init_data,
                          CdmLicenseType license_type,
                          CdmClientPropertySet* property_set) {
    CdmAppParameterMap app_parameters;
    GenerateKeyRequest(init_data, app_parameters, license_type, property_set);
  }

  void GenerateKeyRequest(const std::string& init_data,
                          CdmAppParameterMap& app_parameters,
                          CdmLicenseType license_type,
                          CdmClientPropertySet* property_set) {
    std::string init_data_type = "video/mp4";
    GenerateKeyRequest(wvcdm::KEY_MESSAGE, init_data_type, init_data,
                       app_parameters, license_type, property_set);
  }

  void GenerateKeyRequest(CdmResponseType expected_response,
                          const std::string& init_data_type,
                          const std::string& init_data,
                          CdmAppParameterMap& app_parameters,
                          CdmLicenseType license_type,
                          CdmClientPropertySet* property_set) {
    GenerateKeyRequest(expected_response, init_data_type, init_data,
                       app_parameters, license_type, kDefaultCdmIdentifier,
                       property_set);
  }

  void GenerateKeyRequest(CdmResponseType expected_response,
                          const std::string& init_data_type,
                          const std::string& init_data,
                          CdmAppParameterMap& app_parameters,
                          CdmLicenseType license_type,
                          const CdmIdentifier& cdm_identifier,
                          CdmClientPropertySet* property_set) {
    CdmKeyRequest key_request;
    std::string key_set_id;
    license_type_ = license_type;
    EXPECT_EQ(expected_response,
              decryptor_.GenerateKeyRequest(
                  session_id_, key_set_id, init_data_type, init_data,
                  license_type, app_parameters, property_set,
                  cdm_identifier, &key_request))
        << "session_id_ " << session_id_ << std::endl
        << "init_data (hex) " << wvcdm::b2a_hex(init_data) << std::endl
        << "key_set_id " << key_set_id << std::endl
        << "cdm_identifier.origin " << cdm_identifier.origin << std::endl
        << "cdm_identifier.app_package_name " << cdm_identifier.app_package_name << std::endl
        << "cdm_identifier.unique_id " << cdm_identifier.unique_id << std::endl;
    key_msg_ = key_request.message;
    EXPECT_EQ(0u, key_request.url.size());
  }

  void GenerateRenewalRequest(CdmLicenseType license_type,
                              CdmKeyMessage* key_msg, std::string* server_url) {
    GenerateRenewalRequest(license_type, server_url);
    *key_msg = key_msg_;
  }

  void GenerateRenewalRequest(CdmLicenseType license_type,
                              std::string* server_url) {
    // TODO application makes a license request, CDM will renew the license
    // when appropriate.
    std::string init_data;
    wvcdm::CdmAppParameterMap app_parameters;
    CdmKeyRequest key_request;
    EXPECT_EQ(wvcdm::KEY_MESSAGE,
              decryptor_.GenerateKeyRequest(
                  session_id_, key_set_id_, "video/mp4", init_data,
                  license_type, app_parameters, NULL, kDefaultCdmIdentifier,
                  &key_request));
    key_msg_ = key_request.message;
    *server_url = key_request.url;
    EXPECT_EQ(kKeyRequestTypeRenewal, key_request.type);
    // TODO(edwinwong, rfrias): Add tests cases for when license server url
    // is empty on renewal. Need appropriate key id at the server.
    EXPECT_NE(0u, key_request.url.size());
  }

  bool KeyRotationRequest(CdmLicenseType license_type,
                          const std::string& init_data) {
    wvcdm::CdmAppParameterMap app_parameters;
    CdmKeyRequest key_request;
    CdmResponseType status =
        decryptor_.GenerateKeyRequest(
            session_id_, key_set_id_, "video/mp4", init_data,
            license_type, app_parameters, NULL, kDefaultCdmIdentifier,
            &key_request);
    EXPECT_EQ(wvcdm::KEY_ADDED, status);
    EXPECT_TRUE(key_request.message.empty());
    EXPECT_EQ(kKeyRequestTypeNone, key_request.type);
    EXPECT_TRUE(key_request.url.empty());
    return wvcdm::KEY_ADDED == status && key_request.message.empty()
        && key_request.url.empty();
  }

  void GenerateKeyRelease(CdmKeySetId key_set_id) {
    GenerateKeyRelease(key_set_id, NULL, NULL);
  }

  void GenerateKeyRelease(CdmKeySetId key_set_id,
                          CdmClientPropertySet* property_set,
                          CdmKeyMessage* key_msg) {
    license_type_ = kLicenseTypeRelease;
    CdmSessionId session_id;
    CdmInitData init_data;
    wvcdm::CdmAppParameterMap app_parameters;
    CdmKeyRequest key_request;
    EXPECT_EQ(wvcdm::KEY_MESSAGE,
              decryptor_.GenerateKeyRequest(
                  session_id, key_set_id, "video/mp4", init_data,
                  kLicenseTypeRelease, app_parameters, property_set,
                  kDefaultCdmIdentifier, &key_request));
    key_msg_ = key_request.message;
    EXPECT_EQ(kKeyRequestTypeRelease, key_request.type);
    if (key_msg) *key_msg = key_request.message;
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
    EXPECT_EQ(kHttpOk, http_status_code) << message;

    std::string drm_msg;
    if (kHttpOk == http_status_code) {
      LicenseRequest lic_request;
      lic_request.GetDrmMessage(message, drm_msg);
      LOGV("HTTP response body: (%u bytes)", drm_msg.size());
    }
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
    EXPECT_EQ(kHttpOk, http_status_code) << "message = " << message;
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
                                const std::string& client_auth) {
    VerifyKeyRequestResponse(server_url, client_auth, false);
  }

  void VerifyUsageKeyRequestResponse(const std::string& server_url,
                                     const std::string& client_auth) {
    VerifyKeyRequestResponse(server_url, client_auth, true);
  }

  void VerifyKeyRequestResponse(const std::string& server_url,
                                const std::string& client_auth,
                                bool is_usage) {
    std::string response;
    VerifyKeyRequestResponse(server_url, client_auth, is_usage, &response);
  }

  void VerifyKeyRequestResponse(const std::string& server_url,
                                const std::string& client_auth,
                                bool is_usage,
                                std::string* response) {
    VerifyKeyRequestResponse(wvcdm::KEY_ADDED, server_url, client_auth,
                             is_usage, response);
  }

  void VerifyKeyRequestResponse(CdmResponseType expected_response,
                                const std::string& server_url,
                                const std::string& client_auth,
                                bool is_usage,
                                std::string* response) {
    *response = GetKeyRequestResponse(server_url, client_auth);

    EXPECT_EQ(decryptor_.AddKey(session_id_, *response, &key_set_id_),
              expected_response);
    EXPECT_EQ(is_usage || license_type_ == kLicenseTypeOffline,
              key_set_id_.size() > 0);
  }

  bool VerifyDecryption(const CdmSessionId& session_id,
                        const SubSampleInfo& data) {
    return VerifyDecryption(session_id, data, NO_ERROR);
  }

  bool VerifyDecryption(const CdmSessionId& session_id,
                        const SubSampleInfo& data,
                        CdmResponseType expected_response) {
    std::vector<uint8_t> decrypt_buffer(data.encrypt_data.size());
    CdmDecryptionParameters decryption_parameters(
        &data.key_id, &data.encrypt_data.front(), data.encrypt_data.size(),
        &data.iv, data.block_offset, &decrypt_buffer[0]);
    decryption_parameters.is_encrypted = data.is_encrypted;
    decryption_parameters.is_secure = data.is_secure;
    decryption_parameters.subsample_flags = data.subsample_flags;
    CdmResponseType status = decryptor_.Decrypt(session_id,
                                                data.validate_key_id,
                                                decryption_parameters);
    EXPECT_EQ(expected_response, status);
    if (status != NO_ERROR)
      return false;

    bool result = std::equal(data.decrypt_data.begin(), data.decrypt_data.end(),
                             decrypt_buffer.begin());
    EXPECT_TRUE(result);
    return result;
  }

  void Unprovision() {
    EXPECT_EQ(NO_ERROR,
              decryptor_.Unprovision(kSecurityLevelL1, kDefaultCdmIdentifier));
    EXPECT_EQ(NO_ERROR,
              decryptor_.Unprovision(kSecurityLevelL3, kDefaultCdmIdentifier));
  }

  void Provision(SecurityLevel level) {
    Provision(kDefaultCdmIdentifier, level);
  }

  void Provision(const CdmIdentifier& identifier, SecurityLevel level) {
    TestWvCdmClientPropertySet property_set_L3;
    TestWvCdmClientPropertySet* property_set = NULL;

    if (kLevel3 == level) {
      property_set_L3.set_security_level(QUERY_VALUE_SECURITY_LEVEL_L3);
      property_set = &property_set_L3;
    }

    CdmResponseType status = decryptor_.OpenSession(
        config_.key_system(), property_set, identifier, NULL, &session_id_);
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

    std::string provisioning_server;
    CdmCertificateType cert_type = kCertificateWidevine;
    std::string cert_authority, cert, wrapped_key;

    status = decryptor_.GetProvisioningRequest(cert_type, cert_authority,
                                               identifier,
                                               kEmptyServiceCertificate,
                                               &key_msg_, &provisioning_server);
    EXPECT_EQ(wvcdm::NO_ERROR, status);
    if (NO_ERROR != status) return;
    EXPECT_EQ(provisioning_server, config_.provisioning_server());

    std::string response =
        GetCertRequestResponse(config_.provisioning_server());
    EXPECT_NE(0, static_cast<int>(response.size()));
    EXPECT_EQ(wvcdm::NO_ERROR,
              decryptor_.HandleProvisioningResponse(identifier, response,
                                                    &cert, &wrapped_key));
    EXPECT_EQ(0, static_cast<int>(cert.size()));
    EXPECT_EQ(0, static_cast<int>(wrapped_key.size()));
    decryptor_.CloseSession(session_id_);
    return;
  }

  std::string GetSecurityLevel(TestWvCdmClientPropertySet* property_set) {
    EXPECT_EQ(NO_ERROR,
              decryptor_.OpenSession(config_.key_system(), property_set,
                                     kDefaultCdmIdentifier, NULL,
                                     &session_id_));
    CdmQueryMap query_info;
    EXPECT_EQ(wvcdm::NO_ERROR,
              decryptor_.QuerySessionStatus(session_id_, &query_info));
    CdmQueryMap::iterator itr =
        query_info.find(wvcdm::QUERY_KEY_SECURITY_LEVEL);
    EXPECT_TRUE(itr != query_info.end());
    decryptor_.CloseSession(session_id_);
    if (itr != query_info.end()) {
      return itr->second;
    } else {
      return "ERROR";
    }
  }

  CdmSecurityLevel GetDefaultSecurityLevel() {
    std::string level = GetSecurityLevel(NULL).c_str();
    CdmSecurityLevel security_level = kSecurityLevelUninitialized;
    if (level.compare(wvcdm::QUERY_VALUE_SECURITY_LEVEL_L1) == 0) {
      security_level = kSecurityLevelL1;
    } else if (level.compare(wvcdm::QUERY_VALUE_SECURITY_LEVEL_L3) == 0) {
      security_level = kSecurityLevelL3;
    } else {
      EXPECT_TRUE(false) << "Default Security level is undefined: " << level;
    }
    return security_level;
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

  wvcdm::WvContentDecryptionModule decryptor_;
  CdmKeyMessage key_msg_;
  CdmSessionId session_id_;
  CdmKeySetId key_set_id_;
  CdmLicenseType license_type_;
};

TEST_F(WvCdmRequestLicenseTest, ProvisioningTest) {
  Unprovision();
  EXPECT_EQ(NEED_PROVISIONING,
              decryptor_.OpenSession(config_.key_system(), NULL,
                                     kDefaultCdmIdentifier, NULL,
                                     &session_id_));
  std::string provisioning_server;
  CdmCertificateType cert_type = kCertificateWidevine;
  std::string cert_authority, cert, wrapped_key;

  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.GetProvisioningRequest(
                                 cert_type, cert_authority,
                                 kDefaultCdmIdentifier,
                                 kEmptyServiceCertificate,
                                 &key_msg_, &provisioning_server));
  EXPECT_EQ(provisioning_server, config_.provisioning_server());

  std::string response =
      GetCertRequestResponse(config_.provisioning_server());
  EXPECT_NE(0, static_cast<int>(response.size()));
  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.HandleProvisioningResponse(
                                 kDefaultCdmIdentifier, response, &cert,
                                 &wrapped_key));
  EXPECT_EQ(0, static_cast<int>(cert.size()));
  EXPECT_EQ(0, static_cast<int>(wrapped_key.size()));
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, ProvisioningTestWithServiceCertificate) {
  CdmResponseType status =
      decryptor_.OpenSession(config_.key_system(), NULL,
                             kDefaultCdmIdentifier, NULL,
                             &session_id_);
  EXPECT_TRUE(status == NEED_PROVISIONING || status == NO_ERROR)
      << "Failure to open session. error: " << status;
  std::string provisioning_server;
  CdmCertificateType cert_type = kCertificateWidevine;
  std::string cert_authority, cert, wrapped_key;

  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.GetProvisioningRequest(
                                 cert_type, cert_authority,
                                 kDefaultCdmIdentifier,
                                 config_.provisioning_service_certificate(),
                                 &key_msg_, &provisioning_server));
  EXPECT_EQ(provisioning_server, config_.provisioning_server());

  std::string response =
      GetCertRequestResponse(config_.provisioning_server());
  EXPECT_NE(0, static_cast<int>(response.size()));
  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.HandleProvisioningResponse(
                                 kDefaultCdmIdentifier, response, &cert,
                                 &wrapped_key));
  EXPECT_EQ(0, static_cast<int>(cert.size()));
  EXPECT_EQ(0, static_cast<int>(wrapped_key.size()));
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, L3ProvisioningTest) {
  TestWvCdmClientPropertySet property_set_L3;
  property_set_L3.set_security_level(QUERY_VALUE_SECURITY_LEVEL_L3);
  CdmResponseType status =
      decryptor_.OpenSession(config_.key_system(), &property_set_L3,
                             kDefaultCdmIdentifier, NULL, &session_id_);
  EXPECT_TRUE(status == NEED_PROVISIONING || status == NO_ERROR)
      << "Failure to open session. error: " << status;
  std::string provisioning_server;
  CdmCertificateType cert_type = kCertificateWidevine;
  std::string cert_authority, cert, wrapped_key;

  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.GetProvisioningRequest(
                                 cert_type, cert_authority,
                                 kDefaultCdmIdentifier,
                                 kEmptyServiceCertificate, &key_msg_,
                                 &provisioning_server));
  EXPECT_EQ(provisioning_server, config_.provisioning_server());

  std::string response =
      GetCertRequestResponse(config_.provisioning_server());
  EXPECT_NE(0, static_cast<int>(response.size()));
  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.HandleProvisioningResponse(
                                 kDefaultCdmIdentifier, response, &cert,
                                 &wrapped_key));
  EXPECT_EQ(0, static_cast<int>(cert.size()));
  EXPECT_EQ(0, static_cast<int>(wrapped_key.size()));
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, PerOriginProvisioningTest) {
  EXPECT_EQ(NO_ERROR,
            decryptor_.Unprovision(kSecurityLevelL3, kExampleIdentifier));

  // Verify the global identifier is provisioned.
  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                                   NULL, &session_id_));
  decryptor_.CloseSession(session_id_);

  // The other identifier should not be provisioned.
  EXPECT_EQ(
      wvcdm::NEED_PROVISIONING,
      decryptor_.OpenSession(config_.key_system(), NULL, kExampleIdentifier, NULL,
                             &session_id_));
}

TEST_F(WvCdmRequestLicenseTest, PerOriginProvisioningSupportsOldPaths) {
  // This is the current file name scheme.  This test exists to ensure we do
  // not break existing clients if we decide to change the naming/paths of
  // certificates.
  const char kOldFileName[] = "cert0LF0GfM75iqlNA_sByaQMA==.bin";
  ASSERT_EQ("com.example", kExampleIdentifier.origin);

  // Unprovision the device and delete all files.
  EXPECT_EQ(NO_ERROR,
            decryptor_.Unprovision(kSecurityLevelL3, kExampleIdentifier));
  EXPECT_EQ(NO_ERROR,
            decryptor_.Unprovision(kSecurityLevelL3, kDefaultCdmIdentifier));
  Provision(kExampleIdentifier, kLevel3);

  std::string base_path;
  ASSERT_TRUE(Properties::GetDeviceFilesBasePath(kSecurityLevelL3, &base_path));

  // Make sure that the cert exists.
  std::vector<std::string> files;
  ASSERT_TRUE(FileUtils::List(base_path, &files));
  ASSERT_LE(1u, files.size());
  bool found_it = false;
  for(std::string file: files) {
    if (file == std::string(kOldFileName)) found_it = true;
  }
  EXPECT_TRUE(found_it);

  // Reprovision the default identifier.
  Provision(kDefaultCdmIdentifier, kLevel3);
}

TEST_F(WvCdmRequestLicenseTest, UnprovisionTest) {
  CdmSecurityLevel security_level = GetDefaultSecurityLevel();
  FileSystem file_system;
  DeviceFiles handle(&file_system);
  EXPECT_TRUE(handle.Init(security_level));
  std::string certificate;
  std::string wrapped_private_key;
  std::string serial_number;
  uint32_t system_id;
  EXPECT_TRUE(handle.RetrieveCertificate(&certificate, &wrapped_private_key,
                                         &serial_number, &system_id));

  EXPECT_EQ(NO_ERROR,
            decryptor_.Unprovision(security_level, kDefaultCdmIdentifier));
  EXPECT_FALSE(handle.RetrieveCertificate(&certificate, &wrapped_private_key,
                                          &serial_number, &system_id));
}

TEST_F(WvCdmRequestLicenseTest, ProvisioningInterposedRetryTest) {
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  std::string provisioning_server;
  CdmCertificateType cert_type = kCertificateWidevine;
  std::string cert_authority, cert, wrapped_key;
  CdmKeyMessage key_msg1, key_msg2;

  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.GetProvisioningRequest(
                                 cert_type, cert_authority,
                                 kDefaultCdmIdentifier,
                                 kEmptyServiceCertificate, &key_msg1,
                                 &provisioning_server));
  EXPECT_EQ(provisioning_server, config_.provisioning_server());

  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.GetProvisioningRequest(
                                 cert_type, cert_authority,
                                 kDefaultCdmIdentifier,
                                 kEmptyServiceCertificate, &key_msg2,
                                 &provisioning_server));
  EXPECT_EQ(provisioning_server, config_.provisioning_server());

  key_msg_ = key_msg2;
  std::string response =
      GetCertRequestResponse(config_.provisioning_server());
  EXPECT_NE(0, static_cast<int>(response.size()));
  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.HandleProvisioningResponse(
                                 kDefaultCdmIdentifier, response, &cert,
                                 &wrapped_key));
  EXPECT_EQ(0, static_cast<int>(cert.size()));
  EXPECT_EQ(0, static_cast<int>(wrapped_key.size()));

  key_msg_ = key_msg1;
  response = GetCertRequestResponse(config_.provisioning_server());
  EXPECT_NE(0, static_cast<int>(response.size()));
  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.HandleProvisioningResponse(kDefaultCdmIdentifier,
                                                  response, &cert,
                                                  &wrapped_key));
  EXPECT_EQ(0, static_cast<int>(cert.size()));
  EXPECT_EQ(0, static_cast<int>(wrapped_key.size()));
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, DISABLED_ProvisioningInterspersedRetryTest) {
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  std::string provisioning_server;
  CdmCertificateType cert_type = kCertificateWidevine;
  std::string cert_authority, cert, wrapped_key;
  std::string key_msg1, key_msg2;

  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.GetProvisioningRequest(
                                 cert_type, cert_authority,
                                 kDefaultCdmIdentifier,
                                 kEmptyServiceCertificate, &key_msg1,
                                 &provisioning_server));
  EXPECT_EQ(provisioning_server, config_.provisioning_server());

  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.GetProvisioningRequest(
                                 cert_type, cert_authority,
                                 kDefaultCdmIdentifier,
                                 kEmptyServiceCertificate, &key_msg2,
                                 &provisioning_server));
  EXPECT_EQ(provisioning_server, config_.provisioning_server());

  key_msg_ = key_msg1;
  std::string response =
      GetCertRequestResponse(config_.provisioning_server());
  EXPECT_NE(0, static_cast<int>(response.size()));
  EXPECT_EQ(wvcdm::REWRAP_DEVICE_RSA_KEY_ERROR,
            decryptor_.HandleProvisioningResponse(kDefaultCdmIdentifier,
                                                  response, &cert,
                                                  &wrapped_key));
  EXPECT_EQ(0, static_cast<int>(cert.size()));
  EXPECT_EQ(0, static_cast<int>(wrapped_key.size()));

  key_msg_ = key_msg2;
  response = GetCertRequestResponse(config_.provisioning_server());
  EXPECT_NE(0, static_cast<int>(response.size()));
  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.HandleProvisioningResponse(
                                 kDefaultCdmIdentifier, response, &cert,
                                 &wrapped_key));
  EXPECT_EQ(0, static_cast<int>(cert.size()));
  EXPECT_EQ(0, static_cast<int>(wrapped_key.size()));

  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, DISABLED_X509ProvisioningTest) {
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  std::string provisioning_server;
  CdmCertificateType cert_type = kCertificateX509;
  // TODO(rfrias): insert appropriate CA here
  std::string cert_authority = "cast.google.com";
  std::string cert, wrapped_key;

  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.GetProvisioningRequest(
                                 cert_type, cert_authority,
                                 kDefaultCdmIdentifier,
                                 kEmptyServiceCertificate, &key_msg_,
                                 &provisioning_server));
  EXPECT_EQ(provisioning_server, config_.provisioning_server());

  std::string response =
      GetCertRequestResponse(config_.provisioning_server());
  EXPECT_NE(0, static_cast<int>(response.size()));
  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.HandleProvisioningResponse(
                                 kDefaultCdmIdentifier, response, &cert,
                                 &wrapped_key));
  EXPECT_NE(0, static_cast<int>(cert.size()));
  EXPECT_NE(0, static_cast<int>(wrapped_key.size()));
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, PropertySetTest) {
  TestWvCdmClientPropertySet property_set_L1;
  TestWvCdmClientPropertySet property_set_L3;
  TestWvCdmClientPropertySet property_set_Ln;
  CdmSessionId session_id_L1;
  CdmSessionId session_id_L3;
  CdmSessionId session_id_Ln;

  property_set_L1.set_security_level(QUERY_VALUE_SECURITY_LEVEL_L1);
  property_set_L1.set_use_privacy_mode(true);
  decryptor_.OpenSession(config_.key_system(), &property_set_L1, kDefaultCdmIdentifier,
                         NULL, &session_id_L1);
  property_set_L3.set_security_level(QUERY_VALUE_SECURITY_LEVEL_L3);
  property_set_L3.set_use_privacy_mode(false);

  CdmResponseType sts = decryptor_.OpenSession(
      config_.key_system(), &property_set_L3, kDefaultCdmIdentifier, NULL,
      &session_id_L3);

  if (NEED_PROVISIONING == sts) {
    std::string provisioning_server;
    CdmCertificateType cert_type = kCertificateWidevine;
    std::string cert_authority, cert, wrapped_key;
    EXPECT_EQ(NO_ERROR, decryptor_.GetProvisioningRequest(
                            cert_type, cert_authority, kDefaultCdmIdentifier,
                            kEmptyServiceCertificate, &key_msg_,
                            &provisioning_server));
    EXPECT_EQ(provisioning_server, config_.provisioning_server());
    std::string response =
        GetCertRequestResponse(config_.provisioning_server());
    EXPECT_NE(0, static_cast<int>(response.size()));
    EXPECT_EQ(NO_ERROR, decryptor_.HandleProvisioningResponse(
                            kDefaultCdmIdentifier, response, &cert,
                            &wrapped_key));
    EXPECT_EQ(NO_ERROR,
              decryptor_.OpenSession(config_.key_system(), &property_set_L3,
                                     kDefaultCdmIdentifier, NULL,
                                     &session_id_L3));
  } else {
    EXPECT_EQ(NO_ERROR, sts);
  }

  property_set_Ln.set_security_level("");
  decryptor_.OpenSession(config_.key_system(), &property_set_Ln, kDefaultCdmIdentifier,
                         NULL, &session_id_Ln);

  CdmQueryMap query_info;
  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QuerySessionStatus(session_id_L1, &query_info));
  CdmQueryMap::iterator itr = query_info.find(wvcdm::QUERY_KEY_SECURITY_LEVEL);
  EXPECT_TRUE(itr != query_info.end());
  std::string security_level = itr->second;
  EXPECT_TRUE(!security_level.compare(QUERY_VALUE_SECURITY_LEVEL_L1) ||
              !security_level.compare(QUERY_VALUE_SECURITY_LEVEL_L3));
  EXPECT_TRUE(Properties::UsePrivacyMode(session_id_L1));

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QuerySessionStatus(session_id_L3, &query_info));
  itr = query_info.find(wvcdm::QUERY_KEY_SECURITY_LEVEL);
  EXPECT_TRUE(itr != query_info.end());
  security_level = itr->second;
  EXPECT_EQ(security_level, QUERY_VALUE_SECURITY_LEVEL_L3);
  EXPECT_FALSE(Properties::UsePrivacyMode(session_id_L3));

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QuerySessionStatus(session_id_Ln, &query_info));
  itr = query_info.find(wvcdm::QUERY_KEY_SECURITY_LEVEL);
  EXPECT_TRUE(itr != query_info.end());
  security_level = itr->second;
  EXPECT_TRUE(!security_level.compare(QUERY_VALUE_SECURITY_LEVEL_L1) ||
              !security_level.compare(QUERY_VALUE_SECURITY_LEVEL_L3));

  std::string app_id = "not empty";
  EXPECT_TRUE(Properties::GetApplicationId(session_id_Ln, &app_id));
  EXPECT_STREQ("", app_id.c_str());

  property_set_Ln.set_app_id("com.unittest.mock.app.id");
  EXPECT_TRUE(Properties::GetApplicationId(session_id_Ln, &app_id));
  EXPECT_STREQ("com.unittest.mock.app.id", app_id.c_str());

  decryptor_.CloseSession(session_id_L1);
  decryptor_.CloseSession(session_id_L3);
  decryptor_.CloseSession(session_id_Ln);
}

TEST_F(WvCdmRequestLicenseTest, ForceL3Test) {
  TestWvCdmClientPropertySet property_set;
  property_set.set_security_level(QUERY_VALUE_SECURITY_LEVEL_L3);

  FileSystem file_system;
  DeviceFiles handle(&file_system);
  EXPECT_TRUE(handle.Init(kSecurityLevelL3));
  EXPECT_TRUE(handle.DeleteAllFiles());

  EXPECT_EQ(NEED_PROVISIONING,
            decryptor_.OpenSession(config_.key_system(), &property_set,
                                   kDefaultCdmIdentifier, NULL, &session_id_));
  std::string provisioning_server;
  CdmCertificateType cert_type = kCertificateWidevine;
  std::string cert_authority, cert, wrapped_key;
  EXPECT_EQ(NO_ERROR, decryptor_.GetProvisioningRequest(
                          cert_type, cert_authority, kDefaultCdmIdentifier,
                          kEmptyServiceCertificate, &key_msg_,
                          &provisioning_server));
  EXPECT_EQ(provisioning_server, config_.provisioning_server());
  std::string response =
      GetCertRequestResponse(config_.provisioning_server());
  EXPECT_NE(0, static_cast<int>(response.size()));
  EXPECT_EQ(NO_ERROR, decryptor_.HandleProvisioningResponse(
                          kDefaultCdmIdentifier, response, &cert,
                          &wrapped_key));

  EXPECT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), &property_set,
                                             kDefaultCdmIdentifier, NULL,
                                             &session_id_));
  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, PrivacyModeTest) {
  TestWvCdmClientPropertySet property_set;

  property_set.set_use_privacy_mode(true);
  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);

  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);
  std::string resp = GetKeyRequestResponse(config_.license_server(), config_.client_auth());
  EXPECT_EQ(decryptor_.AddKey(session_id_, resp, &key_set_id_),
            wvcdm::NEED_KEY);
  const std::string empty_init_data;
  // Verify cached initialization data from previous request is used when
  // empty initialization data is passed.
  GenerateKeyRequest(empty_init_data, kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, PrivacyModeWithServiceCertificateTest) {
  TestWvCdmClientPropertySet property_set;

  property_set.set_use_privacy_mode(true);
  property_set.set_service_certificate(config_.license_service_certificate());
  // TODO: pass config_.service_certificate() into CdmEngine::SetServiceCertificate()
  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, BaseMessageTest) {
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);
  GetKeyRequestResponse(config_.license_server(), config_.client_auth());
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, AddStreamingKeyTest) {
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, AddKeyOfflineTest) {
  Unprovision();
  Provision(kLevelDefault);

  // override default settings unless configured through the command line
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  GenerateKeyRequest(key_id, kLicenseTypeOffline);
  VerifyKeyRequestResponse(config_.license_server(), client_auth);
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, RestoreOfflineKeyTest) {
  Unprovision();
  Provision(kLevelDefault);

  // override default settings unless configured through the command line
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  GenerateKeyRequest(key_id, kLicenseTypeOffline);
  VerifyKeyRequestResponse(config_.license_server(), client_auth);

  CdmKeySetId key_set_id = key_set_id_;
  EXPECT_FALSE(key_set_id_.empty());
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, ReleaseOfflineKeyTest) {
  Unprovision();
  Provision(kLevelDefault);

  // override default settings unless configured through the command line
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  GenerateKeyRequest(key_id, kLicenseTypeOffline);
  VerifyKeyRequestResponse(config_.license_server(), client_auth);

  CdmKeySetId key_set_id = key_set_id_;
  EXPECT_FALSE(key_set_id_.empty());
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  GenerateKeyRelease(key_set_id);
  key_set_id_ = key_set_id;
  VerifyKeyRequestResponse(config_.license_server(), client_auth);
}

TEST_F(WvCdmRequestLicenseTest, ReleaseOfflineKeySessionUsageDisabledTest) {
  Unprovision();
  Provision(kLevelDefault);

  // The default offline asset "offline_clip2" has the session usage table
  // entry enabled in the replay control portion of the key control block.
  // To have it disabled we must use "offline_clip1", so replace the last
  // char in init data with '1'
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);
  key_id[key_id.size()-1] = '1';

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  GenerateKeyRequest(key_id, kLicenseTypeOffline);
  VerifyKeyRequestResponse(config_.license_server(), client_auth);

  CdmKeySetId key_set_id = key_set_id_;
  EXPECT_FALSE(key_set_id_.empty());
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  CdmKeyMessage key_msg;
  GenerateKeyRelease(key_set_id, NULL, &key_msg);
  key_set_id_ = key_set_id;
  VerifyKeyRequestResponse(config_.license_server(), client_auth);

  SignedMessage signed_message;
  EXPECT_TRUE(signed_message.ParseFromString(key_msg));
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
  EXPECT_TRUE(existing_license.license_id().provider_session_token().empty());
  EXPECT_TRUE(existing_license.session_usage_table_entry().empty());
}

TEST_F(WvCdmRequestLicenseTest, ReleaseRetryOfflineKeyTest) {
  Unprovision();
  Provision(kLevelDefault);

  // override default settings unless configured through the command line
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  GenerateKeyRequest(key_id, kLicenseTypeOffline);
  VerifyKeyRequestResponse(config_.license_server(), client_auth);

  CdmKeySetId key_set_id = key_set_id_;
  EXPECT_FALSE(key_set_id_.empty());
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  GenerateKeyRelease(key_set_id);

  session_id_.clear();
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  EXPECT_EQ(wvcdm::GET_RELEASED_LICENSE_ERROR,
            decryptor_.RestoreKey(session_id_, key_set_id));
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  GenerateKeyRelease(key_set_id);
  key_set_id_ = key_set_id;
  VerifyKeyRequestResponse(config_.license_server(), client_auth);
}

TEST_F(WvCdmRequestLicenseTest, ReleaseRetryL3OfflineKeyTest) {
  Unprovision();

  TestWvCdmClientPropertySet property_set;
  property_set.set_security_level(QUERY_VALUE_SECURITY_LEVEL_L3);

  // override default settings unless configured through the command line
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);

  CdmResponseType sts = decryptor_.OpenSession(
      config_.key_system(), &property_set, kDefaultCdmIdentifier, NULL, &session_id_);

  if (NEED_PROVISIONING == sts) {
    std::string provisioning_server;
    CdmCertificateType cert_type = kCertificateWidevine;
    std::string cert_authority, cert, wrapped_key;
    EXPECT_EQ(NO_ERROR, decryptor_.GetProvisioningRequest(
                            cert_type, cert_authority, kDefaultCdmIdentifier,
                            kEmptyServiceCertificate,
                            &key_msg_, &provisioning_server));
    EXPECT_EQ(provisioning_server, config_.provisioning_server());
    std::string response =
        GetCertRequestResponse(config_.provisioning_server());
    EXPECT_NE(0, static_cast<int>(response.size()));
    EXPECT_EQ(NO_ERROR, decryptor_.HandleProvisioningResponse(
                            kDefaultCdmIdentifier, response, &cert,
                            &wrapped_key));
    EXPECT_EQ(NO_ERROR,
              decryptor_.OpenSession(config_.key_system(), &property_set,
                                     kDefaultCdmIdentifier, NULL,
                                     &session_id_));
  } else {
    EXPECT_EQ(NO_ERROR, sts);
  }

  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(key_id, kLicenseTypeOffline, &property_set);
  VerifyKeyRequestResponse(config_.license_server(), client_auth);

  CdmKeySetId key_set_id = key_set_id_;
  EXPECT_FALSE(key_set_id_.empty());
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  GenerateKeyRelease(key_set_id, &property_set, NULL);

  session_id_.clear();
  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  EXPECT_EQ(wvcdm::GET_RELEASED_LICENSE_ERROR,
            decryptor_.RestoreKey(session_id_, key_set_id));
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  GenerateKeyRelease(key_set_id, &property_set, NULL);
  key_set_id_ = key_set_id;
  VerifyKeyRequestResponse(config_.license_server(), client_auth);
}

TEST_F(WvCdmRequestLicenseTest,
       ReleaseRetryL3OfflineKeySessionUsageDisabledTest) {
  Unprovision();

  TestWvCdmClientPropertySet property_set;
  property_set.set_security_level(QUERY_VALUE_SECURITY_LEVEL_L3);

  // The default offline asset "offline_clip2" has the session usage table
  // entry enabled in the replay control portion of the key control block.
  // To have it disabled we must use "offline_clip1", so replace the last
  // char in init data with '1'
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);
  key_id[key_id.size()-1] = '1';

  CdmResponseType sts = decryptor_.OpenSession(
      config_.key_system(), &property_set, kDefaultCdmIdentifier, NULL, &session_id_);

  if (NEED_PROVISIONING == sts) {
    std::string provisioning_server_url;
    CdmCertificateType cert_type = kCertificateWidevine;
    std::string cert_authority, cert, wrapped_key;
    EXPECT_EQ(NO_ERROR, decryptor_.GetProvisioningRequest(
                            cert_type, cert_authority, kDefaultCdmIdentifier,
                            kEmptyServiceCertificate,
                            &key_msg_, &provisioning_server_url));
    EXPECT_EQ(provisioning_server_url, config_.provisioning_server());
    std::string response =
        GetCertRequestResponse(config_.provisioning_server());
    EXPECT_NE(0, static_cast<int>(response.size()));
    EXPECT_EQ(NO_ERROR, decryptor_.HandleProvisioningResponse(
                            kDefaultCdmIdentifier, response, &cert,
                            &wrapped_key));
    EXPECT_EQ(NO_ERROR,
              decryptor_.OpenSession(config_.key_system(), &property_set,
                                     kDefaultCdmIdentifier, NULL,
                                     &session_id_));
  } else {
    EXPECT_EQ(NO_ERROR, sts);
  }

  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(key_id, kLicenseTypeOffline, &property_set);
  VerifyKeyRequestResponse(config_.license_server(), client_auth);

  CdmKeySetId key_set_id = key_set_id_;
  EXPECT_FALSE(key_set_id_.empty());
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  GenerateKeyRelease(key_set_id, &property_set, NULL);

  session_id_.clear();
  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  EXPECT_EQ(wvcdm::GET_RELEASED_LICENSE_ERROR,
            decryptor_.RestoreKey(session_id_, key_set_id));
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  GenerateKeyRelease(key_set_id, &property_set, NULL);
  key_set_id_ = key_set_id;
  VerifyKeyRequestResponse(config_.license_server(), client_auth);
}

// This test verifies that repeated generation of the key release message
// for the same key_set_id results in the previous session being
// deallocated (rather than leaked) and a new one allocated.
TEST_F(WvCdmRequestLicenseTest, AutomatedOfflineSessionReleaseTest) {
  Unprovision();
  Provision(kLevelDefault);

  // override default settings unless configured through the command line
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  GenerateKeyRequest(key_id, kLicenseTypeOffline);
  VerifyKeyRequestResponse(config_.license_server(), client_auth);

  CdmKeySetId key_set_id = key_set_id_;
  EXPECT_FALSE(key_set_id_.empty());
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));
  decryptor_.CloseSession(session_id_);

  uint32_t open_sessions =
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS);

  session_id_.clear();
  key_set_id_.clear();
  GenerateKeyRelease(key_set_id);
  key_set_id_ = key_set_id;

  EXPECT_EQ(
      ++open_sessions,
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS));

  session_id_.clear();
  key_set_id_.clear();
  GenerateKeyRelease(key_set_id);
  key_set_id_ = key_set_id;

  EXPECT_EQ(
      open_sessions,
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS));

  VerifyKeyRequestResponse(config_.license_server(), client_auth);

  EXPECT_EQ(
      --open_sessions,
      QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS));
}

TEST_F(WvCdmRequestLicenseTest, StreamingLicenseRenewal) {
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  std::string license_server;
  GenerateRenewalRequest(kLicenseTypeStreaming, &license_server);
  if (license_server.empty()) license_server = config_.license_server();
  VerifyKeyRequestResponse(license_server, config_.client_auth());
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, StreamingLicenseRenewalProhibited) {
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  std::string key_id = a2bs_hex(                  // streaming_clip11
      "000000437073736800000000"                  // blob size and pssh
      "EDEF8BA979D64ACEA3C827DCD51D21ED00000023"  // Widevine system id
      "08011a0d7769646576696e655f746573"          // pssh data
      "74221073747265616d696e675f636c69703131");
  GenerateKeyRequest(key_id, kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  std::string init_data;
  wvcdm::CdmAppParameterMap app_parameters;
  CdmKeyRequest key_request;
  EXPECT_EQ(wvcdm::LICENSE_RENEWAL_PROHIBITED,
            decryptor_.GenerateKeyRequest(
                session_id_, key_set_id_, "video/mp4", init_data,
                kLicenseTypeStreaming, app_parameters, NULL,
                kDefaultCdmIdentifier, &key_request));
  key_msg_ = key_request.message;
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, OfflineLicenseRenewal) {
  Unprovision();
  Provision(kLevelDefault);

  // override default settings unless configured through the command line
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(key_id, kLicenseTypeOffline);
  VerifyKeyRequestResponse(config_.license_server(), client_auth);

  std::string license_server;
  GenerateRenewalRequest(kLicenseTypeOffline, &license_server);
  if (license_server.empty()) license_server = config_.license_server();
  VerifyKeyRequestResponse(license_server, client_auth);
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, OfflineLicenseRenewalAndRelease) {
  Unprovision();
  Provision(kLevelDefault);

  // override default settings unless configured through the command line
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);

  // Retrieve offline license
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(key_id, kLicenseTypeOffline);
  VerifyKeyRequestResponse(config_.license_server(), client_auth);

  CdmKeySetId key_set_id = key_set_id_;
  EXPECT_FALSE(key_set_id_.empty());

  // Verify that we can decrypt a subsample
  SubSampleInfo* data = &single_encrypted_offline_sub_sample;
  std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
  CdmDecryptionParameters decryption_parameters(
      &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
      &data->iv, data->block_offset, &decrypt_buffer[0]);
  decryption_parameters.is_encrypted = data->is_encrypted;
  decryption_parameters.is_secure = data->is_secure;
  decryption_parameters.subsample_flags = data->subsample_flags;

  EXPECT_EQ(NO_ERROR,
            decryptor_.Decrypt(session_id_, data->validate_key_id,
                               decryption_parameters));
  EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                         decrypt_buffer.begin()));

  decryptor_.CloseSession(session_id_);
  session_id_.clear();

  // Restore offline license and renew it
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));

  std::string license_server;
  GenerateRenewalRequest(kLicenseTypeOffline, &license_server);
  EXPECT_FALSE(license_server.empty());
  VerifyKeyRequestResponse(license_server, client_auth);

  // Verify that we can decrypt a subsample
  decrypt_buffer = data->encrypt_data;
  EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                         decryption_parameters));
  EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                         decrypt_buffer.begin()));

  decryptor_.CloseSession(session_id_);
  session_id_.clear();

  // Restore offline license and test it
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));

  // Verify that we can decrypt a subsample
  decrypt_buffer = data->encrypt_data;
  EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                         decryption_parameters));
  EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                         decrypt_buffer.begin()));

  decryptor_.CloseSession(session_id_);
  session_id_.clear();

  // Restore and release offline license
  key_set_id_.clear();
  GenerateKeyRelease(key_set_id);
  key_set_id_ = key_set_id;
  VerifyKeyRequestResponse(config_.license_server(), client_auth);
}

class WvCdmEntitlementTest
    : public WvCdmRequestLicenseTest,
      public ::testing::WithParamInterface<EntitlementTestConfiguration*> {};

TEST_P(WvCdmEntitlementTest, EntitlementWithKeyRotation) {
  EntitlementTestConfiguration* config = GetParam();
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);

  // Fetch entitlement license
  GenerateKeyRequest(config->entitlement_pssh, kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  // Verify that we can decrypt a subsample
  ASSERT_TRUE(VerifyDecryption(session_id_,
                               *(config->sub_sample_with_initial_keys)));

  // Key rotation
  ASSERT_TRUE(KeyRotationRequest(kLicenseTypeStreaming,
                                 config->key_rotation_pssh));

  // Verify that we can decrypt a subsample
  ASSERT_TRUE(VerifyDecryption(session_id_,
                               *(config->sub_sample_with_rotated_keys)));

  decryptor_.CloseSession(session_id_);
}

INSTANTIATE_TEST_CASE_P(
    Cdm, WvCdmEntitlementTest,
    ::testing::Range(&kEntitlementTestConfiguration[0],
                     &kEntitlementTestConfiguration[2]));

TEST_F(WvCdmRequestLicenseTest, RemoveKeys) {
  ASSERT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), NULL,
                                             kDefaultCdmIdentifier, NULL,
                                             &session_id_));
  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  SubSampleInfo* data = &single_encrypted_sub_sample;
  std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
  CdmDecryptionParameters decryption_parameters(
      &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
      &data->iv, data->block_offset, &decrypt_buffer[0]);
  decryption_parameters.is_encrypted = data->is_encrypted;
  decryption_parameters.is_secure = data->is_secure;
  decryption_parameters.subsample_flags = data->subsample_flags;

  // Verify that we can decrypt a subsample
  EXPECT_EQ(NO_ERROR,
            decryptor_.Decrypt(session_id_, data->validate_key_id,
                               decryption_parameters));
  EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                         decrypt_buffer.begin()));

  // Verify that decryption of a subsample fails after the keys are removed
  ASSERT_EQ(NO_ERROR, decryptor_.RemoveKeys(session_id_));
  decrypt_buffer.assign(data->encrypt_data.size(), 0);
  EXPECT_EQ(NEED_KEY,
            decryptor_.Decrypt(session_id_, data->validate_key_id,
                               decryption_parameters));

  ASSERT_EQ(NO_ERROR, decryptor_.CloseSession(session_id_));
}

class WvCdmStreamingLicenseRenewalTest
    : public WvCdmRequestLicenseTest,
      public ::testing::WithParamInterface<
          RenewWithClientIdTestConfiguration*> {};

TEST_P(WvCdmStreamingLicenseRenewalTest, WithClientId) {
  RenewWithClientIdTestConfiguration* config = GetParam();
  std::string key_id;
  if (config->always_include_client_id) {
    key_id = a2bs_hex(
        "000000427073736800000000"                  // blob size and pssh
        "EDEF8BA979D64ACEA3C827DCD51D21ED00000022"  // Widevine system id
        "08011a0d7769646576696e655f74657374220f73"  // pssh data
        "747265616d696e675f636c697036");
  } else {
    key_id = binary_key_id();
  }

  const uint32_t kNameValueParamSize = 2;
  std::pair<std::string, std::string> name_value_params[kNameValueParamSize] = {
      {"Name1", "Value1"}, {"Name2", "Value2"}};
  wvcdm::CdmAppParameterMap app_parameters;
  if (config->specify_app_parameters) {
    for (uint32_t i = 0; i < kNameValueParamSize; ++i) {
      app_parameters[name_value_params[i].first] = name_value_params[i].second;
    }
  }

  TestWvCdmClientPropertySet property_set;
  if (config->enable_privacy_mode) {
    property_set.set_use_privacy_mode(true);
    if (config->specify_service_certificate)
      property_set.set_service_certificate(
          config_.license_service_certificate());
  }
  // TODO: pass config_.service_certificate() into CdmEngine::SetServiceCertificate()
  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(key_id, app_parameters, kLicenseTypeStreaming,
                     &property_set);
  if (config->enable_privacy_mode && !config->specify_service_certificate) {
    std::string resp = GetKeyRequestResponse(config_.license_server(), config_.client_auth());
    EXPECT_EQ(decryptor_.AddKey(session_id_, resp, &key_set_id_),
              wvcdm::NEED_KEY);
    GenerateKeyRequest(key_id, kLicenseTypeStreaming);
  }
  std::string key_response;
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth(), false,
                           &key_response);

  // Validate signed license
  SignedMessage signed_message;
  EXPECT_TRUE(signed_message.ParseFromString(key_response))
      << config->test_description;
  EXPECT_EQ(SignedMessage::LICENSE, signed_message.type())
      << config->test_description;
  EXPECT_TRUE(signed_message.has_signature()) << config->test_description;
  EXPECT_TRUE(!signed_message.msg().empty()) << config->test_description;

  // Verify license request
  video_widevine::License license;
  EXPECT_TRUE(license.ParseFromString(signed_message.msg()))
      << config->test_description;

  // Verify always_include_client_id
  EXPECT_EQ(config->always_include_client_id,
            license.policy().has_always_include_client_id());

  std::string license_server;
  CdmKeyMessage key_msg;
  GenerateRenewalRequest(kLicenseTypeStreaming, &key_msg, &license_server);
  if (license_server.empty()) license_server = config_.license_server();

  // Validate signed renewal request
  EXPECT_TRUE(signed_message.ParseFromString(key_msg))
      << config->test_description;
  EXPECT_EQ(SignedMessage::LICENSE_REQUEST, signed_message.type())
      << config->test_description;
  EXPECT_TRUE(signed_message.has_signature()) << config->test_description;
  EXPECT_TRUE(!signed_message.msg().empty()) << config->test_description;

  // Verify license request
  video_widevine::LicenseRequest license_renewal;
  EXPECT_TRUE(license_renewal.ParseFromString(signed_message.msg()))
      << config->test_description;

  // Verify ClientId
  EXPECT_EQ(config->always_include_client_id && !config->enable_privacy_mode,
            license_renewal.has_client_id())
      << config->test_description;

  if (config->specify_app_parameters) {
    for (uint32_t i = 0; i < kNameValueParamSize; ++i) {
      bool found = false;
      for (int j = 0; j < license_renewal.client_id().client_info_size(); ++j) {
        ClientIdentification_NameValue client_info =
            license_renewal.client_id().client_info(i);
        if (name_value_params[i].first.compare(client_info.name()) == 0 &&
            name_value_params[i].second.compare(client_info.value()) == 0) {
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found) << config->test_description;
    }
  }

  if (config->enable_privacy_mode) {
    EXPECT_EQ(config->always_include_client_id,
              license_renewal.has_encrypted_client_id())
        << config->test_description;
    EXPECT_NE(
        0u, license_renewal.encrypted_client_id().encrypted_client_id().size());
  }

  VerifyKeyRequestResponse(license_server, config_.client_auth());
  decryptor_.CloseSession(session_id_);
}

INSTANTIATE_TEST_CASE_P(
    Cdm, WvCdmStreamingLicenseRenewalTest,
    ::testing::Range(&streaming_renew_client_id_test_configuration[0],
                     &streaming_renew_client_id_test_configuration[5]));

class WvCdmOfflineLicenseReleaseTest
    : public WvCdmRequestLicenseTest,
      public ::testing::WithParamInterface<
          RenewWithClientIdTestConfiguration*> {};

TEST_P(WvCdmOfflineLicenseReleaseTest, WithClientId) {
  Unprovision();
  Provision(kLevelDefault);

  RenewWithClientIdTestConfiguration* config = GetParam();
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);

  if (config->always_include_client_id) {
    key_id = a2bs_hex(
        "00000040"                          // blob size
        "70737368"                          // "pssh"
        "00000000"                          // flags
        "edef8ba979d64acea3c827dcd51d21ed"  // Widevine system id
        "00000020"                          // pssh data size
        // pssh data:
        "08011a0d7769646576696e655f746573"
        "74220d6f66666c696e655f636c697033");
  }

  const uint32_t kNameValueParamSize = 2;
  std::pair<std::string, std::string> name_value_params[kNameValueParamSize] = {
      {"Name1", "Value1"}, {"Name2", "Value2"}};
  wvcdm::CdmAppParameterMap app_parameters;
  if (config->specify_app_parameters) {
    for (uint32_t i = 0; i < kNameValueParamSize; ++i) {
      app_parameters[name_value_params[i].first] = name_value_params[i].second;
    }
  }

  TestWvCdmClientPropertySet property_set;
  if (config->enable_privacy_mode) {
    property_set.set_use_privacy_mode(true);
    if (config->specify_service_certificate)
      property_set.set_service_certificate(config_.license_service_certificate());
  }
  // TODO: pass config_.service_certificate() into CdmEngine::SetServiceCertificate()
  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(key_id, app_parameters, kLicenseTypeOffline, NULL);
  if (config->enable_privacy_mode && !config->specify_service_certificate) {
    std::string resp = GetKeyRequestResponse(config_.license_server(), client_auth);
    EXPECT_EQ(decryptor_.AddKey(session_id_, resp, &key_set_id_),
              wvcdm::NEED_KEY);
    GenerateKeyRequest(key_id, kLicenseTypeOffline);
  }
  std::string key_response;
  VerifyKeyRequestResponse(config_.license_server(), client_auth, false, &key_response);

  // Validate signed license
  SignedMessage signed_message;
  EXPECT_TRUE(signed_message.ParseFromString(key_response))
      << config->test_description;
  EXPECT_EQ(SignedMessage::LICENSE, signed_message.type())
      << config->test_description;
  EXPECT_TRUE(signed_message.has_signature()) << config->test_description;
  EXPECT_TRUE(!signed_message.msg().empty()) << config->test_description;

  // Verify license request
  video_widevine::License license;
  EXPECT_TRUE(license.ParseFromString(signed_message.msg()))
      << config->test_description;

  // Verify always_include_client_id
  EXPECT_EQ(config->always_include_client_id,
            license.policy().has_always_include_client_id());

  CdmKeySetId key_set_id = key_set_id_;
  EXPECT_TRUE(key_set_id_.size() > 0);
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();

  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  key_set_id_.clear();
  CdmKeyMessage key_msg;
  GenerateKeyRelease(key_set_id, &property_set, &key_msg);
  key_set_id_ = key_set_id;

  // Validate signed renewal request
  EXPECT_TRUE(signed_message.ParseFromString(key_msg))
      << config->test_description;
  EXPECT_EQ(SignedMessage::LICENSE_REQUEST, signed_message.type())
      << config->test_description;
  EXPECT_TRUE(signed_message.has_signature()) << config->test_description;
  EXPECT_TRUE(!signed_message.msg().empty()) << config->test_description;

  // Verify license request
  video_widevine::LicenseRequest license_release;
  EXPECT_TRUE(license_release.ParseFromString(signed_message.msg()))
      << config->test_description;

  // Verify ClientId
  EXPECT_EQ(config->always_include_client_id && !config->enable_privacy_mode,
            license_release.has_client_id())
      << config->test_description;

  if (config->specify_app_parameters) {
    for (uint32_t i = 0; i < kNameValueParamSize; ++i) {
      bool found = false;
      for (int j = 0; j < license_release.client_id().client_info_size(); ++j) {
        ClientIdentification_NameValue client_info =
            license_release.client_id().client_info(i);
        if (name_value_params[i].first.compare(client_info.name()) == 0 &&
            name_value_params[i].second.compare(client_info.value()) == 0) {
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found) << config->test_description;
    }
  }

  if (config->enable_privacy_mode) {
    EXPECT_EQ(config->always_include_client_id,
              license_release.has_encrypted_client_id())
        << config->test_description;
    EXPECT_NE(
        0u, license_release.encrypted_client_id().encrypted_client_id().size());
  }

  VerifyKeyRequestResponse(config_.license_server(), client_auth);
  decryptor_.CloseSession(session_id_);
}

INSTANTIATE_TEST_CASE_P(
    Cdm, WvCdmOfflineLicenseReleaseTest,
    ::testing::Range(&offline_release_client_id_test_configuration[0],
                     &offline_release_client_id_test_configuration[4]));

class WvCdmUsageTest
    : public WvCdmRequestLicenseTest,
      public ::testing::WithParamInterface<
          RenewWithClientIdTestConfiguration*> {};

TEST_P(WvCdmUsageTest, WithClientId) {
  Unprovision();
  Provision(kLevelDefault);

  CdmSecurityLevel security_level = GetDefaultSecurityLevel();
  std::string app_id = "";
  FileSystem file_system;
  DeviceFiles handle(&file_system);
  EXPECT_TRUE(handle.Init(security_level));
  std::vector<std::string> psts;
  EXPECT_TRUE(handle.DeleteAllUsageInfoForApp(
                  DeviceFiles::GetUsageInfoFileName(app_id),
                  &psts));

  RenewWithClientIdTestConfiguration* config = GetParam();
  std::string key_id;

  if (config->always_include_client_id) {
    key_id = a2bs_hex(                              // streaming_clip20
        "000000437073736800000000"                  // blob size and pssh
        "EDEF8BA979D64ACEA3C827DCD51D21ED00000023"  // Widevine system id
        "08011a0d7769646576696e655f746573"          // pssh data
        "74221073747265616d696e675f636c69703230");
  } else {
    key_id = a2bs_hex(                              // streaming_clip3
        "000000427073736800000000"                  // blob size and pssh
        "EDEF8BA979D64ACEA3C827DCD51D21ED00000022"  // Widevine system id
        "08011a0d7769646576696e655f746573"          // pssh data
        "74220f73747265616d696e675f636c697033");
  }
  wvcdm::CdmAppParameterMap app_parameters;
  TestWvCdmClientPropertySet property_set;

  SubSampleInfo* data = &usage_info_sub_samples_icp[0];
  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);

  GenerateKeyRequest(key_id, app_parameters, kLicenseTypeStreaming,
                     &property_set);

  std::string key_response;
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth(), true,
                           &key_response);

  // Validate signed license
  SignedMessage signed_message;
  EXPECT_TRUE(signed_message.ParseFromString(key_response))
      << config->test_description;
  EXPECT_EQ(SignedMessage::LICENSE, signed_message.type())
      << config->test_description;
  EXPECT_TRUE(signed_message.has_signature()) << config->test_description;
  EXPECT_TRUE(!signed_message.msg().empty()) << config->test_description;

  // Verify license request
  video_widevine::License license;
  EXPECT_TRUE(license.ParseFromString(signed_message.msg()))
      << config->test_description;

  // Verify always_include_client_id
  EXPECT_EQ(config->always_include_client_id,
            license.policy().has_always_include_client_id());

  EXPECT_FALSE(license.id().provider_session_token().empty());

  std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
  CdmDecryptionParameters decryption_parameters(
      &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
      &data->iv, data->block_offset, &decrypt_buffer[0]);
  decryption_parameters.is_encrypted = data->is_encrypted;
  decryption_parameters.is_secure = data->is_secure;
  decryption_parameters.subsample_flags = data->subsample_flags;
  EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                         decryption_parameters));

  EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                         decrypt_buffer.begin()));
  decryptor_.CloseSession(session_id_);

  CdmUsageInfo usage_info;
  CdmUsageInfoReleaseMessage release_msg;
  CdmResponseType status = decryptor_.GetUsageInfo(
      app_id, kDefaultCdmIdentifier, &usage_info);

  EXPECT_EQ(KEY_MESSAGE, status);
  ASSERT_FALSE(usage_info.empty());

  // Validate signed renewal request
  EXPECT_TRUE(signed_message.ParseFromString(usage_info[0]))
      << config->test_description;
  EXPECT_EQ(SignedMessage::LICENSE_REQUEST, signed_message.type())
      << config->test_description;
  EXPECT_TRUE(signed_message.has_signature()) << config->test_description;
  EXPECT_TRUE(!signed_message.msg().empty()) << config->test_description;

  // Verify license request
  video_widevine::LicenseRequest license_renewal;
  EXPECT_TRUE(license_renewal.ParseFromString(signed_message.msg()))
      << config->test_description;

  // Verify ClientId
  EXPECT_EQ(config->always_include_client_id && !config->enable_privacy_mode,
            license_renewal.has_client_id())
      << config->test_description;

  if (config->enable_privacy_mode) {
    EXPECT_EQ(config->always_include_client_id,
              license_renewal.has_encrypted_client_id())
        << config->test_description;
    EXPECT_NE(
        0u, license_renewal.encrypted_client_id().encrypted_client_id().size());
  }

  release_msg =
      GetUsageInfoResponse(config_.license_server(), config_.client_auth(), usage_info[0]);
  EXPECT_EQ(NO_ERROR, decryptor_.ReleaseUsageInfo(release_msg,
                                                  kDefaultCdmIdentifier));
}

INSTANTIATE_TEST_CASE_P(
    Cdm, WvCdmUsageTest,
    ::testing::Range(&usage_client_id_test_configuration[0],
                     &usage_client_id_test_configuration[2]));

TEST_F(WvCdmRequestLicenseTest, UsageInfoRetryTest) {
  Unprovision();
  Provision(kLevelDefault);

  CdmSecurityLevel security_level = GetDefaultSecurityLevel();
  std::string app_id = "";
  FileSystem file_system;
  DeviceFiles handle(&file_system);
  EXPECT_TRUE(handle.Init(security_level));
  std::vector<std::string> psts;
  EXPECT_TRUE(handle.DeleteAllUsageInfoForApp(
                  DeviceFiles::GetUsageInfoFileName(app_id),
                  &psts));

  SubSampleInfo* data = &usage_info_sub_samples_icp[0];
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  std::string key_id = a2bs_hex(
      "000000427073736800000000"                  // blob size and pssh
      "EDEF8BA979D64ACEA3C827DCD51D21ED00000022"  // Widevine system id
      "08011a0d7769646576696e655f74657374220f73"  // pssh data
      "747265616d696e675f636c697033");

  GenerateKeyRequest(key_id, kLicenseTypeStreaming, NULL);
  VerifyUsageKeyRequestResponse(config_.license_server(), config_.client_auth());

  std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
  CdmDecryptionParameters decryption_parameters(
      &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
      &data->iv, data->block_offset, &decrypt_buffer[0]);
  decryption_parameters.is_encrypted = data->is_encrypted;
  decryption_parameters.is_secure = data->is_secure;
  decryption_parameters.subsample_flags = data->subsample_flags;
  EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                         decryption_parameters));

  EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                         decrypt_buffer.begin()));
  decryptor_.CloseSession(session_id_);

  CdmUsageInfo usage_info;
  CdmUsageInfoReleaseMessage release_msg;
  CdmResponseType status = decryptor_.GetUsageInfo(
      app_id, kDefaultCdmIdentifier, &usage_info);
  EXPECT_EQ(usage_info.empty() ? NO_ERROR : KEY_MESSAGE, status);

  // Discard and retry to verify usage reports can be generated multiple times
  // before release.
  status = decryptor_.GetUsageInfo(app_id, kDefaultCdmIdentifier, &usage_info);
  EXPECT_EQ(usage_info.empty() ? NO_ERROR : KEY_MESSAGE, status);
  int error_count = 0;
  while (usage_info.size() > 0) {
    for (size_t i = 0; i < usage_info.size(); ++i) {
      release_msg =
          GetUsageInfoResponse(config_.license_server(), config_.client_auth(), usage_info[i]);
      EXPECT_EQ(NO_ERROR, decryptor_.ReleaseUsageInfo(release_msg,
                                                      kDefaultCdmIdentifier))
          << i << "/" << usage_info.size() << " (err " << (error_count++) << ")"
          << release_msg;
    }
    ASSERT_LE(error_count, 100);  // Give up after 100 failures.
    status = decryptor_.GetUsageInfo(
        app_id, kDefaultCdmIdentifier, &usage_info);
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

TEST_F(WvCdmRequestLicenseTest, UsageInfo_ReleaseThreeRecords) {
  Unprovision();
  Provision(kLevelDefault);

  CdmSecurityLevel security_level = GetDefaultSecurityLevel();
  std::string app_id = "";
  FileSystem file_system;
  DeviceFiles handle(&file_system);
  EXPECT_TRUE(handle.Init(security_level));
  std::vector<std::string> psts;
  EXPECT_TRUE(handle.DeleteAllUsageInfoForApp(
                  DeviceFiles::GetUsageInfoFileName(app_id),
                  &psts));

  std::string session_id_clip3, session_id_clip4, session_id_clip7;

  // Open session for streaming_clip3 and verify decryption is successful
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_clip3);

  session_id_ = session_id_clip3;
  GenerateKeyRequest(kPsshStreamingClip3, kLicenseTypeStreaming, NULL);
  VerifyUsageKeyRequestResponse(config_.license_server(),
                                config_.client_auth());

  EXPECT_TRUE(VerifyDecryption(session_id_clip3,
                               usage_info_sub_samples_icp[0]));

  // Open session for streaming_clip4 and verify decryption is successful
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_clip4);

  session_id_ = session_id_clip4;
  GenerateKeyRequest(kPsshStreamingClip4, kLicenseTypeStreaming, NULL);
  VerifyUsageKeyRequestResponse(config_.license_server(),
                                config_.client_auth());

  EXPECT_TRUE(VerifyDecryption(session_id_clip4,
                               usage_info_sub_samples_icp[1]));

  // Open session for streaming_clip7 and verify decryption is successful
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_clip7);

  session_id_ = session_id_clip7;
  GenerateKeyRequest(kPsshStreamingClip7, kLicenseTypeStreaming, NULL);
  VerifyUsageKeyRequestResponse(config_.license_server(),
                                config_.client_auth());

  EXPECT_TRUE(VerifyDecryption(session_id_clip7,
                               usage_info_sub_samples_icp[4]));

  // Close session for streaming_clip4 and release usage information
  decryptor_.CloseSession(session_id_clip4);

  CdmUsageInfo usage_info;
  CdmUsageInfoReleaseMessage release_msg;
  CdmResponseType status = decryptor_.GetUsageInfo(
      app_id, kProviderSessionTokenStreamingClip4, kDefaultCdmIdentifier,
      &usage_info);
  EXPECT_EQ(KEY_MESSAGE, status);
  EXPECT_EQ(1, usage_info.size());
  release_msg = GetUsageInfoResponse(config_.license_server(),
                                     config_.client_auth(), usage_info[0]);
  EXPECT_EQ(NO_ERROR,
            decryptor_.ReleaseUsageInfo(release_msg, kDefaultCdmIdentifier));

  decryptor_.CloseSession(session_id_clip7);

  status = decryptor_.GetUsageInfo(app_id, kProviderSessionTokenStreamingClip7,
                                   kDefaultCdmIdentifier, &usage_info);
  EXPECT_EQ(KEY_MESSAGE, status);
  EXPECT_EQ(1, usage_info.size());
  release_msg = GetUsageInfoResponse(config_.license_server(),
                                     config_.client_auth(), usage_info[0]);
  EXPECT_EQ(NO_ERROR,
            decryptor_.ReleaseUsageInfo(release_msg, kDefaultCdmIdentifier));

  decryptor_.CloseSession(session_id_clip3);

  status = decryptor_.GetUsageInfo(app_id, kProviderSessionTokenStreamingClip3,
                                   kDefaultCdmIdentifier, &usage_info);
  EXPECT_EQ(KEY_MESSAGE, status);
  EXPECT_EQ(1, usage_info.size());
  release_msg = GetUsageInfoResponse(config_.license_server(),
                                     config_.client_auth(), usage_info[0]);
  EXPECT_EQ(NO_ERROR,
            decryptor_.ReleaseUsageInfo(release_msg, kDefaultCdmIdentifier));
}

class WvCdmUsageInfoTest
    : public WvCdmRequestLicenseTest,
      public ::testing::WithParamInterface<UsageInfoSubSampleInfo*> {};

TEST_P(WvCdmUsageInfoTest, UsageInfo) {
  Unprovision();

  UsageInfoSubSampleInfo* usage_info_data = GetParam();
  TestWvCdmClientPropertySet client_property_set;
  TestWvCdmClientPropertySet* property_set = NULL;
  if (kLevel3 == usage_info_data->security_level) {
    client_property_set.set_security_level(QUERY_VALUE_SECURITY_LEVEL_L3);
    property_set = &client_property_set;
    Provision(kLevel3);
    Provision(kLevelDefault);
  } else {
    Provision(kLevelDefault);
  }

  CdmSecurityLevel security_level = GetDefaultSecurityLevel();
  FileSystem file_system;
  DeviceFiles handle(&file_system);
  EXPECT_TRUE(handle.Init(security_level));
  std::vector<std::string> psts;
  EXPECT_TRUE(handle.DeleteAllUsageInfoForApp(
                  DeviceFiles::GetUsageInfoFileName(usage_info_data->app_id),
                  &psts));

  for (size_t i = 0; i < usage_info_data->usage_info; ++i) {
    SubSampleInfo* data = usage_info_data->sub_sample + i;
    decryptor_.OpenSession(config_.key_system(), property_set, kDefaultCdmIdentifier,
                           NULL, &session_id_);
    std::string key_id = a2bs_hex(
        "000000427073736800000000"                  // blob size and pssh
        "EDEF8BA979D64ACEA3C827DCD51D21ED00000022"  // Widevine system id
        "08011a0d7769646576696e655f74657374220f73"  // pssh data
        "747265616d696e675f636c6970");

    char ch = 0x33 + i;
    key_id.append(1, ch);

    GenerateKeyRequest(key_id, kLicenseTypeStreaming, property_set);

    // TODO(rfrias): streaming_clip6 is a streaming license without a pst
    if (ch == '6')
      VerifyKeyRequestResponse(config_.license_server(), config_.client_auth(), false);
    else
      VerifyUsageKeyRequestResponse(config_.license_server(), config_.client_auth());

    std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
    CdmDecryptionParameters decryption_parameters(
        &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
        &data->iv, data->block_offset, &decrypt_buffer[0]);
    decryption_parameters.is_encrypted = data->is_encrypted;
    decryption_parameters.is_secure = data->is_secure;
    decryption_parameters.subsample_flags = data->subsample_flags;
    EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                           decryption_parameters));

    EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                           decrypt_buffer.begin()));
    decryptor_.CloseSession(session_id_);
  }

  CdmUsageInfo usage_info;
  CdmUsageInfoReleaseMessage release_msg;
  CdmResponseType status =
      decryptor_.GetUsageInfo(usage_info_data->app_id, kDefaultCdmIdentifier,
                              &usage_info);
  EXPECT_EQ(usage_info.empty() ? NO_ERROR : KEY_MESSAGE, status);
  int error_count = 0;
  while (usage_info.size() > 0) {
    for (size_t i = 0; i < usage_info.size(); ++i) {
      release_msg =
          GetUsageInfoResponse(config_.license_server(), config_.client_auth(), usage_info[i]);
      EXPECT_EQ(NO_ERROR,
                decryptor_.ReleaseUsageInfo(release_msg, kDefaultCdmIdentifier))
          << i << "/" << usage_info.size() << " (err " << (error_count++) << ")"
          << release_msg;
    }
    ASSERT_LE(error_count, 100);  // Give up after 100 failures.
    status = decryptor_.GetUsageInfo(usage_info_data->app_id,
                                     kDefaultCdmIdentifier, &usage_info);
    EXPECT_EQ(usage_info.empty() ? NO_ERROR : KEY_MESSAGE, status);
  }
}

INSTANTIATE_TEST_CASE_P(Cdm, WvCdmUsageInfoTest,
                        ::testing::Values(&usage_info_sub_sample_info[0],
                                          &usage_info_sub_sample_info[1],
                                          &usage_info_sub_sample_info[2],
                                          &usage_info_sub_sample_info[3],
                                          &usage_info_sub_sample_info[4],
                                          &usage_info_sub_sample_info[5],
                                          &usage_info_sub_sample_info[6],
                                          &usage_info_sub_sample_info[7]));

TEST_F(WvCdmRequestLicenseTest, UsageRemoveAllTest) {
  Unprovision();

  std::string app_id_empty = "";
  std::string app_id_not_empty = "not empty";

  TestWvCdmClientPropertySet property_set;
  Provision(kLevelDefault);

  CdmSecurityLevel security_level = GetDefaultSecurityLevel();
  FileSystem file_system;
  DeviceFiles handle(&file_system);
  EXPECT_TRUE(handle.Init(security_level));
  std::vector<std::string> psts;
  EXPECT_TRUE(handle.DeleteAllUsageInfoForApp(
                  DeviceFiles::GetUsageInfoFileName(""), &psts));

  for (size_t i = 0; i < N_ELEM(usage_info_sub_samples_icp); ++i) {
    SubSampleInfo* data = usage_info_sub_samples_icp + i;
    property_set.set_app_id(i % 2 == 0 ? app_id_empty : app_id_not_empty);
    decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                           NULL, &session_id_);
    std::string key_id = a2bs_hex(
        "000000427073736800000000"                  // blob size and pssh
        "EDEF8BA979D64ACEA3C827DCD51D21ED00000022"  // Widevine system id
        "08011a0d7769646576696e655f74657374220f73"  // pssh data
        "747265616d696e675f636c6970");

    char ch = 0x33 + i;
    key_id.append(1, ch);

    GenerateKeyRequest(key_id, kLicenseTypeStreaming, &property_set);

    // TODO(rfrias): streaming_clip6 is a streaming license without a pst
    if (ch == '6')
      VerifyKeyRequestResponse(config_.license_server(), config_.client_auth(), false);
    else
      VerifyUsageKeyRequestResponse(config_.license_server(), config_.client_auth());

    std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
    CdmDecryptionParameters decryption_parameters(
        &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
        &data->iv, data->block_offset, &decrypt_buffer[0]);
    decryption_parameters.is_encrypted = data->is_encrypted;
    decryption_parameters.is_secure = data->is_secure;
    decryption_parameters.subsample_flags = data->subsample_flags;
    EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                           decryption_parameters));

    EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                           decrypt_buffer.begin()));
    decryptor_.CloseSession(session_id_);
  }

  CdmUsageInfo usage_info;
  EXPECT_EQ(
      KEY_MESSAGE,
      decryptor_.GetUsageInfo(app_id_empty, kDefaultCdmIdentifier,
                              &usage_info));
  EXPECT_TRUE(usage_info.size() > 0);
  EXPECT_EQ(
      KEY_MESSAGE,
      decryptor_.GetUsageInfo(app_id_not_empty, kDefaultCdmIdentifier,
                              &usage_info));
  EXPECT_TRUE(usage_info.size() > 0);


  EXPECT_EQ(
      NO_ERROR,
      decryptor_.RemoveAllUsageInfo(app_id_not_empty, kDefaultCdmIdentifier));

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetUsageInfo(app_id_not_empty, kDefaultCdmIdentifier,
                              &usage_info));
  EXPECT_TRUE(usage_info.empty());
  EXPECT_EQ(
      KEY_MESSAGE,
      decryptor_.GetUsageInfo(app_id_empty, kDefaultCdmIdentifier,
                              &usage_info));
  EXPECT_TRUE(usage_info.size() > 0);

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.RemoveAllUsageInfo(app_id_empty, kDefaultCdmIdentifier));

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetUsageInfo(app_id_not_empty, kDefaultCdmIdentifier,
                              &usage_info));
  EXPECT_TRUE(usage_info.empty());
  EXPECT_EQ(
    NO_ERROR,
    decryptor_.GetUsageInfo(app_id_empty, kDefaultCdmIdentifier,
                            &usage_info));
  EXPECT_TRUE(usage_info.empty());
}

TEST_F(WvCdmRequestLicenseTest, RemoveCorruptedUsageInfoTest) {
  Unprovision();

  std::string app_id_empty = "";
  std::string app_id_not_empty = "not empty";

  TestWvCdmClientPropertySet property_set;
  Provision(kLevelDefault);

  CdmSecurityLevel security_level = GetDefaultSecurityLevel();
  FileSystem file_system;
  DeviceFiles handle(&file_system);
  EXPECT_TRUE(handle.Init(security_level));
  std::vector<std::string> psts;
  EXPECT_TRUE(handle.DeleteAllUsageInfoForApp(
                  DeviceFiles::GetUsageInfoFileName(""), &psts));

  for (size_t i = 0; i < N_ELEM(usage_info_sub_samples_icp); ++i) {
    SubSampleInfo* data = usage_info_sub_samples_icp + i;
    property_set.set_app_id(i % 2 == 0 ? app_id_empty : app_id_not_empty);
    decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                           NULL, &session_id_);
    std::string key_id = a2bs_hex(
        "000000427073736800000000"                  // blob size and pssh
        "EDEF8BA979D64ACEA3C827DCD51D21ED00000022"  // Widevine system id
        "08011a0d7769646576696e655f74657374220f73"  // pssh data
        "747265616d696e675f636c6970");

    char ch = 0x33 + i;
    key_id.append(1, ch);

    GenerateKeyRequest(key_id, kLicenseTypeStreaming, &property_set);

    // TODO(rfrias): streaming_clip6 is a streaming license without a pst
    if (ch == '6')
      VerifyKeyRequestResponse(config_.license_server(), config_.client_auth(), false);
    else
      VerifyUsageKeyRequestResponse(config_.license_server(), config_.client_auth());

    std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
    CdmDecryptionParameters decryption_parameters(
        &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
        &data->iv, data->block_offset, &decrypt_buffer[0]);
    decryption_parameters.is_encrypted = data->is_encrypted;
    decryption_parameters.is_secure = data->is_secure;
    decryption_parameters.subsample_flags = data->subsample_flags;
    EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                           decryption_parameters));

    EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                           decrypt_buffer.begin()));
    decryptor_.CloseSession(session_id_);
  }

  CdmUsageInfo usage_info;
  EXPECT_EQ(
      KEY_MESSAGE,
      decryptor_.GetUsageInfo(app_id_empty, kDefaultCdmIdentifier,
                              &usage_info));
  EXPECT_TRUE(usage_info.size() > 0);
  EXPECT_EQ(
      KEY_MESSAGE,
      decryptor_.GetUsageInfo(app_id_not_empty, kDefaultCdmIdentifier,
                              &usage_info));
  EXPECT_TRUE(usage_info.size() > 0);

  // Read in usage info file
  std::string path;
  EXPECT_TRUE(Properties::GetDeviceFilesBasePath(security_level, &path));
  std::string usage_info_not_empty_app_id_file_name =
      path + DeviceFiles::GetUsageInfoFileName(app_id_not_empty);

  ssize_t file_size =
      file_system.FileSize(usage_info_not_empty_app_id_file_name);
  EXPECT_LT(4, file_size);
  std::unique_ptr<File> file =
      file_system.Open(usage_info_not_empty_app_id_file_name,
                       FileSystem::kReadOnly);
  EXPECT_TRUE(NULL != file);
  std::string file_data;
  file_data.resize(file_size);
  ssize_t bytes = file->Read(&file_data[0], file_data.size());
  EXPECT_EQ(file_size, bytes);

  // Corrupt the hash of the usage info file with non-empty app id and write
  // it back out
  memset(&file_data[0]+bytes-4, 0, 4);
  file = file_system.Open(usage_info_not_empty_app_id_file_name,
                          FileSystem::kCreate | FileSystem::kTruncate);
  EXPECT_TRUE(NULL != file);
  bytes = file->Write(file_data.data(), file_data.size());
  EXPECT_EQ(file_size, bytes);

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.RemoveAllUsageInfo(app_id_not_empty, kDefaultCdmIdentifier));

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetUsageInfo(app_id_not_empty, kDefaultCdmIdentifier,
                              &usage_info));
  EXPECT_TRUE(usage_info.empty());
  EXPECT_EQ(
      KEY_MESSAGE,
      decryptor_.GetUsageInfo(app_id_empty, kDefaultCdmIdentifier,
                              &usage_info));
  EXPECT_TRUE(usage_info.size() > 0);

  // Read in usage info file
  std::string usage_info_empty_app_id_file_name =
      path + DeviceFiles::GetUsageInfoFileName(app_id_empty);

  file_size = file_system.FileSize(usage_info_empty_app_id_file_name);
  EXPECT_LT(4, file_size);
  file = file_system.Open(usage_info_empty_app_id_file_name,
                          FileSystem::kReadOnly);
  EXPECT_TRUE(NULL != file);
  file_data.resize(file_size);
  bytes = file->Read(&file_data[0], file_data.size());
  EXPECT_EQ(file_size, bytes);

  // Corrupt the hash of the usage info file with empty app id and write it
  // back out
  memset(&file_data[0]+bytes-4, 0, 4);
  file = file_system.Open(usage_info_empty_app_id_file_name,
                          FileSystem::kCreate | FileSystem::kTruncate);
  EXPECT_TRUE(NULL != file);
  bytes = file->Write(file_data.data(), file_data.size());
  EXPECT_EQ(file_size, bytes);

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.RemoveAllUsageInfo(app_id_empty, kDefaultCdmIdentifier));

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetUsageInfo(app_id_not_empty, kDefaultCdmIdentifier,
                              &usage_info));
  EXPECT_TRUE(usage_info.empty());
  EXPECT_EQ(
    NO_ERROR,
    decryptor_.GetUsageInfo(app_id_empty, kDefaultCdmIdentifier,
                            &usage_info));
  EXPECT_TRUE(usage_info.empty());
}

TEST_F(WvCdmRequestLicenseTest, RemoveCorruptedUsageInfoTest2) {
  Unprovision();

  std::string app_id_empty = "";
  std::string app_id_not_empty = "not empty";

  TestWvCdmClientPropertySet property_set;
  Provision(kLevelDefault);

  CdmSecurityLevel security_level = GetDefaultSecurityLevel();
  FileSystem file_system;
  DeviceFiles handle(&file_system);
  EXPECT_TRUE(handle.Init(security_level));
  std::vector<std::string> psts;
  EXPECT_TRUE(handle.DeleteAllUsageInfoForApp(
                  DeviceFiles::GetUsageInfoFileName(""), &psts));

  for (size_t i = 0; i < N_ELEM(usage_info_sub_samples_icp); ++i) {
    SubSampleInfo* data = usage_info_sub_samples_icp + i;
    property_set.set_app_id(i % 2 == 0 ? app_id_empty : app_id_not_empty);
    decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                           NULL, &session_id_);
    std::string key_id = a2bs_hex(
        "000000427073736800000000"                  // blob size and pssh
        "EDEF8BA979D64ACEA3C827DCD51D21ED00000022"  // Widevine system id
        "08011a0d7769646576696e655f74657374220f73"  // pssh data
        "747265616d696e675f636c6970");

    char ch = 0x33 + i;
    key_id.append(1, ch);

    GenerateKeyRequest(key_id, kLicenseTypeStreaming, &property_set);

    // TODO(rfrias): streaming_clip6 is a streaming license without a pst
    if (ch == '6')
      VerifyKeyRequestResponse(config_.license_server(), config_.client_auth(), false);
    else
      VerifyUsageKeyRequestResponse(config_.license_server(), config_.client_auth());

    std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
    CdmDecryptionParameters decryption_parameters(
        &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
        &data->iv, data->block_offset, &decrypt_buffer[0]);
    decryption_parameters.is_encrypted = data->is_encrypted;
    decryption_parameters.is_secure = data->is_secure;
    decryption_parameters.subsample_flags = data->subsample_flags;
    EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                           decryption_parameters));

    EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                           decrypt_buffer.begin()));
    decryptor_.CloseSession(session_id_);
  }

  CdmUsageInfo usage_info;
  EXPECT_EQ(
      KEY_MESSAGE,
      decryptor_.GetUsageInfo(app_id_empty, kDefaultCdmIdentifier,
                              &usage_info));
  EXPECT_TRUE(usage_info.size() > 0);
  EXPECT_EQ(
      KEY_MESSAGE,
      decryptor_.GetUsageInfo(app_id_not_empty, kDefaultCdmIdentifier,
                              &usage_info));
  EXPECT_TRUE(usage_info.size() > 0);

  // Read in usage info file
  std::string path;
  EXPECT_TRUE(Properties::GetDeviceFilesBasePath(security_level, &path));
  std::string usage_info_not_empty_app_id_file_name =
      path + DeviceFiles::GetUsageInfoFileName(app_id_not_empty);

  ssize_t file_size =
      file_system.FileSize(usage_info_not_empty_app_id_file_name);
  EXPECT_LT(4, file_size);
  std::unique_ptr<File> file =
      file_system.Open(usage_info_not_empty_app_id_file_name,
                       FileSystem::kReadOnly);
  EXPECT_TRUE(NULL != file);
  std::string file_data;
  file_data.resize(file_size);
  ssize_t bytes = file->Read(&file_data[0], file_data.size());
  EXPECT_EQ(file_size, bytes);

  video_widevine_client::sdk::HashedFile hash_file;

  EXPECT_TRUE(hash_file.ParseFromString(file_data));
  size_t pos = file_data.find(hash_file.hash());
  EXPECT_NE(std::string::npos, pos);
  EXPECT_NE(0u, pos);

  // Corrupt the protobuf key field of the hash of the usage info file
  // with non-empty app id and write it back out
  file_data[pos-1] = 0xff;

  file = file_system.Open(usage_info_not_empty_app_id_file_name,
                          FileSystem::kCreate | FileSystem::kTruncate);
  EXPECT_TRUE(NULL != file);
  bytes = file->Write(file_data.data(), file_data.size());
  EXPECT_EQ(file_size, bytes);

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.RemoveAllUsageInfo(app_id_not_empty, kDefaultCdmIdentifier));

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetUsageInfo(app_id_not_empty, kDefaultCdmIdentifier,
                              &usage_info));
  EXPECT_TRUE(usage_info.empty());
  EXPECT_EQ(
      KEY_MESSAGE,
      decryptor_.GetUsageInfo(app_id_empty, kDefaultCdmIdentifier,
                              &usage_info));
  EXPECT_TRUE(usage_info.size() > 0);

  // Read in usage info file
  std::string usage_info_empty_app_id_file_name =
      path + DeviceFiles::GetUsageInfoFileName(app_id_empty);

  file_size = file_system.FileSize(usage_info_empty_app_id_file_name);
  EXPECT_LT(4, file_size);
  file = file_system.Open(usage_info_empty_app_id_file_name,
                          FileSystem::kReadOnly);
  EXPECT_TRUE(NULL != file);
  file_data.resize(file_size);
  bytes = file->Read(&file_data[0], file_data.size());
  EXPECT_EQ(file_size, bytes);

  EXPECT_TRUE(hash_file.ParseFromString(file_data));
  pos = file_data.find(hash_file.hash());
  EXPECT_NE(std::string::npos, pos);
  EXPECT_NE(0u, pos);

  // Corrupt the protobuf key field of the hash of the usage info file
  // with empty app id and write it back out
  file_data[pos-1] = 0xff;

  file = file_system.Open(usage_info_empty_app_id_file_name,
                          FileSystem::kCreate | FileSystem::kTruncate);
  EXPECT_TRUE(NULL != file);
  bytes = file->Write(file_data.data(), file_data.size());
  EXPECT_EQ(file_size, bytes);

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.RemoveAllUsageInfo(app_id_empty, kDefaultCdmIdentifier));

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetUsageInfo(app_id_not_empty, kDefaultCdmIdentifier,
                              &usage_info));
  EXPECT_TRUE(usage_info.empty());
  EXPECT_EQ(
    NO_ERROR,
    decryptor_.GetUsageInfo(app_id_empty, kDefaultCdmIdentifier,
                            &usage_info));
  EXPECT_TRUE(usage_info.empty());
}

TEST_F(WvCdmRequestLicenseTest, GetSecureStopIdsTest) {
  Unprovision();

  std::string app_id_empty = "";

  TestWvCdmClientPropertySet property_set;
  Provision(kLevelDefault);

  CdmSecurityLevel security_level = GetDefaultSecurityLevel();
  FileSystem file_system;
  DeviceFiles handle(&file_system);
  EXPECT_TRUE(handle.Init(security_level));
  std::vector<std::string> psts;
  EXPECT_TRUE(handle.DeleteAllUsageInfoForApp(
                  DeviceFiles::GetUsageInfoFileName(""), &psts));

  std::vector<CdmSecureStopId> retrieved_secure_stop_ids;
  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetSecureStopIds(app_id_empty, kDefaultCdmIdentifier,
                                  &retrieved_secure_stop_ids));
  EXPECT_TRUE(retrieved_secure_stop_ids.empty());

  // First fetch licenses for the default app
  for (size_t i = 0; i < N_ELEM(kUsageLicenseTestVector1); ++i) {
    SubSampleInfo* data = kUsageLicenseTestVector1[i].sub_sample;

    property_set.set_app_id(app_id_empty);
    decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                           NULL, &session_id_);
    GenerateKeyRequest(kUsageLicenseTestVector1[i].pssh, kLicenseTypeStreaming,
                       &property_set);

    VerifyUsageKeyRequestResponse(config_.license_server(), config_.client_auth());

    std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
    CdmDecryptionParameters decryption_parameters(
        &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
        &data->iv, data->block_offset, &decrypt_buffer[0]);
    decryption_parameters.is_encrypted = data->is_encrypted;
    decryption_parameters.is_secure = data->is_secure;
    decryption_parameters.subsample_flags = data->subsample_flags;
    EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                           decryption_parameters));

    EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                           decrypt_buffer.begin()));
    decryptor_.CloseSession(session_id_);
  }

  // Provision the other identifier
  Provision(kExampleIdentifier, kLevelDefault);

  // Verify that there are usage records for the default identifier but
  // none yet for the non-default one
  std::vector<CdmSecureStopId> expected_provider_session_tokens;
  for (size_t i = 0; i < N_ELEM(kUsageLicenseTestVector1); ++i) {
    expected_provider_session_tokens.push_back(
        kUsageLicenseTestVector1[i].provider_session_token);
  }

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetSecureStopIds(app_id_empty, kDefaultCdmIdentifier,
                                  &retrieved_secure_stop_ids));
  EXPECT_EQ(N_ELEM(kUsageLicenseTestVector1), retrieved_secure_stop_ids.size());
  EXPECT_THAT(retrieved_secure_stop_ids,
      UnorderedElementsAreArray(expected_provider_session_tokens));

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetSecureStopIds(kExampleIdentifier.app_package_name,
                                  kDefaultCdmIdentifier,
                                  &retrieved_secure_stop_ids));
  EXPECT_TRUE(retrieved_secure_stop_ids.empty());

  // Now fetch licenses for the other identifier
  for (size_t i = 0; i < N_ELEM(kUsageLicenseTestVector2); ++i) {
    SubSampleInfo* data = kUsageLicenseTestVector2[i].sub_sample;

    property_set.set_app_id(kExampleIdentifier.app_package_name);
    EXPECT_EQ(NO_ERROR,
              decryptor_.OpenSession(config_.key_system(), &property_set,
                                     kExampleIdentifier, NULL,
                                     &session_id_));
    std::string init_data_type = "video/mp4";
    CdmAppParameterMap app_parameters;
    GenerateKeyRequest(wvcdm::KEY_MESSAGE, init_data_type,
                       kUsageLicenseTestVector2[i].pssh, app_parameters,
                       kLicenseTypeStreaming, kExampleIdentifier,
                       &property_set);

    VerifyUsageKeyRequestResponse(config_.license_server(), config_.client_auth());

    std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
    CdmDecryptionParameters decryption_parameters(
        &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
        &data->iv, data->block_offset, &decrypt_buffer[0]);
    decryption_parameters.is_encrypted = data->is_encrypted;
    decryption_parameters.is_secure = data->is_secure;
    decryption_parameters.subsample_flags = data->subsample_flags;
    EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                           decryption_parameters));

    EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                           decrypt_buffer.begin()));
    decryptor_.CloseSession(session_id_);
  }

  // Verify that there are usage records for both the default and
  // non-default identifier.
  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetSecureStopIds(kDefaultCdmIdentifier.app_package_name,
                                  kDefaultCdmIdentifier,
                                  &retrieved_secure_stop_ids));
  EXPECT_EQ(N_ELEM(kUsageLicenseTestVector1), retrieved_secure_stop_ids.size());
  EXPECT_THAT(retrieved_secure_stop_ids,
      UnorderedElementsAreArray(expected_provider_session_tokens));

  retrieved_secure_stop_ids.clear();
  expected_provider_session_tokens.clear();

  for (size_t i = 0; i < N_ELEM(kUsageLicenseTestVector2); ++i) {
    expected_provider_session_tokens.push_back(
        kUsageLicenseTestVector2[i].provider_session_token);
  }

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetSecureStopIds(kExampleIdentifier.app_package_name,
                                  kExampleIdentifier,
                                  &retrieved_secure_stop_ids));
  EXPECT_EQ(N_ELEM(kUsageLicenseTestVector2), retrieved_secure_stop_ids.size());
  EXPECT_THAT(retrieved_secure_stop_ids,
      UnorderedElementsAreArray(expected_provider_session_tokens));

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.RemoveAllUsageInfo(kDefaultCdmIdentifier.app_package_name,
                                    kDefaultCdmIdentifier));
  EXPECT_EQ(
      NO_ERROR,
      decryptor_.RemoveAllUsageInfo(kExampleIdentifier.app_package_name,
                                    kExampleIdentifier));

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetSecureStopIds(kDefaultCdmIdentifier.app_package_name,
                                  kDefaultCdmIdentifier,
                                  &retrieved_secure_stop_ids));
  EXPECT_TRUE(retrieved_secure_stop_ids.empty());
  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetSecureStopIds(kExampleIdentifier.app_package_name,
                                  kExampleIdentifier,
                                  &retrieved_secure_stop_ids));
  EXPECT_TRUE(retrieved_secure_stop_ids.empty());
}

TEST_F(WvCdmRequestLicenseTest, UsageRecoveryTest) {
  Unprovision();

  std::string app_id_empty = "";

  TestWvCdmClientPropertySet property_set;
  Provision(kLevelDefault);

  CdmSecurityLevel security_level = GetDefaultSecurityLevel();
  FileSystem file_system;
  DeviceFiles handle(&file_system);
  EXPECT_TRUE(handle.Init(security_level));
  std::vector<std::string> psts;
  EXPECT_TRUE(handle.DeleteAllUsageInfoForApp(
                  DeviceFiles::GetUsageInfoFileName(""), &psts));

  // Fetch a usage license
  SubSampleInfo* data = kUsageLicenseTestVector1[0].sub_sample;

  property_set.set_app_id(app_id_empty);
  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(kUsageLicenseTestVector1[0].pssh, kLicenseTypeStreaming,
                     &property_set);

  VerifyUsageKeyRequestResponse(config_.license_server(), config_.client_auth());

  std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
  CdmDecryptionParameters decryption_parameters(
      &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
      &data->iv, data->block_offset, &decrypt_buffer[0]);
  decryption_parameters.is_encrypted = data->is_encrypted;
  decryption_parameters.is_secure = data->is_secure;
  decryption_parameters.subsample_flags = data->subsample_flags;
  EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                         decryption_parameters));

  EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                         decrypt_buffer.begin()));
  decryptor_.CloseSession(session_id_);

  std::string path;
  EXPECT_TRUE(Properties::GetDeviceFilesBasePath(security_level, &path));
  std::string usage_info_file_name =
      path + DeviceFiles::GetUsageInfoFileName(app_id_empty);

  // Read in usage info file
  ssize_t file_size = file_system.FileSize(usage_info_file_name);
  EXPECT_LT(4, file_size);
  std::unique_ptr<File> file =
      file_system.Open(usage_info_file_name, FileSystem::kReadOnly);
  EXPECT_TRUE(NULL != file);
  std::string file_data;
  file_data.resize(file_size);
  ssize_t bytes = file->Read(&file_data[0], file_data.size());
  EXPECT_EQ(file_size, bytes);

  // Corrupt the hash of the usage info file and write it back out
  memset(&file_data[0]+bytes-4, 0, 4);
  file = file_system.Open(usage_info_file_name,
                          FileSystem::kCreate | FileSystem::kTruncate);
  EXPECT_TRUE(NULL != file);
  bytes = file->Write(file_data.data(), file_data.size());
  EXPECT_EQ(file_size, bytes);

  // Fetch a second usage license, this should fail as the usage table is
  // corrupt
  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(kUsageLicenseTestVector1[1].pssh, kLicenseTypeStreaming,
                     &property_set);

  std::string response;
  VerifyKeyRequestResponse(wvcdm::STORE_USAGE_INFO_ERROR, config_.license_server(),
                           config_.client_auth(), true, &response);

  decryptor_.CloseSession(session_id_);

  // Fetch the second usage license and verify that it is usable
  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(kUsageLicenseTestVector1[1].pssh, kLicenseTypeStreaming,
                     &property_set);

  VerifyUsageKeyRequestResponse(config_.license_server(), config_.client_auth());

  data = kUsageLicenseTestVector1[1].sub_sample;

  decrypt_buffer.resize(data->encrypt_data.size());
  decryption_parameters.key_id = &data->key_id;
  decryption_parameters.encrypt_buffer = &data->encrypt_data.front();
  decryption_parameters.encrypt_length = data->encrypt_data.size();
  decryption_parameters.iv = &data->iv;
  decryption_parameters.block_offset = data->block_offset;
  decryption_parameters.decrypt_buffer = &decrypt_buffer[0];
  decryption_parameters.decrypt_buffer_length = data->encrypt_data.size();
  decryption_parameters.is_encrypted = data->is_encrypted;
  decryption_parameters.is_secure = data->is_secure;
  decryption_parameters.subsample_flags = data->subsample_flags;
  EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                         decryption_parameters));

  EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                         decrypt_buffer.begin()));
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, UsageRemoveSecureStopTest) {
  Unprovision();

  std::string app_id_empty = "";

  TestWvCdmClientPropertySet property_set;
  Provision(kLevelDefault);

  CdmSecurityLevel security_level = GetDefaultSecurityLevel();
  FileSystem file_system;
  DeviceFiles handle(&file_system);
  EXPECT_TRUE(handle.Init(security_level));
  std::vector<std::string> psts;
  EXPECT_TRUE(handle.DeleteAllUsageInfoForApp(
                  DeviceFiles::GetUsageInfoFileName(""), &psts));

  // First fetch licenses for the default app
  for (size_t i = 0; i < N_ELEM(kUsageLicenseTestVector1); ++i) {
    SubSampleInfo* data = kUsageLicenseTestVector1[i].sub_sample;

    property_set.set_app_id(app_id_empty);
    decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                           NULL, &session_id_);
    GenerateKeyRequest(kUsageLicenseTestVector1[i].pssh, kLicenseTypeStreaming,
                       &property_set);

    VerifyUsageKeyRequestResponse(config_.license_server(), config_.client_auth());

    std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
    CdmDecryptionParameters decryption_parameters(
        &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
        &data->iv, data->block_offset, &decrypt_buffer[0]);
    decryption_parameters.is_encrypted = data->is_encrypted;
    decryption_parameters.is_secure = data->is_secure;
    decryption_parameters.subsample_flags = data->subsample_flags;
    EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                           decryption_parameters));

    EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                           decrypt_buffer.begin()));
    decryptor_.CloseSession(session_id_);
  }

  // Provision and fetch licenses for the other identifier
  Provision(kExampleIdentifier, kLevelDefault);

  for (size_t i = 0; i < N_ELEM(kUsageLicenseTestVector2); ++i) {
    SubSampleInfo* data = kUsageLicenseTestVector2[i].sub_sample;

    property_set.set_app_id(kExampleIdentifier.app_package_name);
    EXPECT_EQ(NO_ERROR,
              decryptor_.OpenSession(config_.key_system(), &property_set,
                                     kExampleIdentifier, NULL,
                                     &session_id_));
    std::string init_data_type = "video/mp4";
    CdmAppParameterMap app_parameters;
    GenerateKeyRequest(wvcdm::KEY_MESSAGE, init_data_type,
                       kUsageLicenseTestVector2[i].pssh, app_parameters,
                       kLicenseTypeStreaming, kExampleIdentifier,
                       &property_set);

    VerifyUsageKeyRequestResponse(config_.license_server(), config_.client_auth());

    std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
    CdmDecryptionParameters decryption_parameters(
        &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
        &data->iv, data->block_offset, &decrypt_buffer[0]);
    decryption_parameters.is_encrypted = data->is_encrypted;
    decryption_parameters.is_secure = data->is_secure;
    decryption_parameters.subsample_flags = data->subsample_flags;
    EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                           decryption_parameters));

    EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                           decrypt_buffer.begin()));
    decryptor_.CloseSession(session_id_);
  }

  // Release usage records for both the default and non-default identifier.
  std::vector<CdmSecureStopId> secure_stop_ids;
  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetSecureStopIds(kDefaultCdmIdentifier.app_package_name,
                                  kDefaultCdmIdentifier,
                                  &secure_stop_ids));
  EXPECT_EQ(N_ELEM(kUsageLicenseTestVector1), secure_stop_ids.size());

  for (size_t i = 0; i < N_ELEM(kUsageLicenseTestVector1); ++i) {
    EXPECT_EQ(
        NO_ERROR,
        decryptor_.RemoveUsageInfo(kDefaultCdmIdentifier.app_package_name,
                                   kDefaultCdmIdentifier,
                                   secure_stop_ids[i]));
  }

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetSecureStopIds(kDefaultCdmIdentifier.app_package_name,
                                  kDefaultCdmIdentifier,
                                  &secure_stop_ids));
  EXPECT_TRUE(secure_stop_ids.empty());

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetSecureStopIds(kExampleIdentifier.app_package_name,
                                  kExampleIdentifier,
                                  &secure_stop_ids));
  EXPECT_EQ(N_ELEM(kUsageLicenseTestVector2), secure_stop_ids.size());

  for (size_t i = 0; i < N_ELEM(kUsageLicenseTestVector2); ++i) {
    EXPECT_EQ(
        NO_ERROR,
        decryptor_.RemoveUsageInfo(kExampleIdentifier.app_package_name,
                                   kExampleIdentifier,
                                   secure_stop_ids[i]));
  }

  EXPECT_EQ(
      NO_ERROR,
      decryptor_.GetSecureStopIds(kExampleIdentifier.app_package_name,
                                  kExampleIdentifier,
                                  &secure_stop_ids));
  EXPECT_TRUE(secure_stop_ids.empty());
}

// TODO(rfrias): Enable when b/123370099 has been addressed
TEST_F(WvCdmRequestLicenseTest, VerifyProviderClientToken) {
  Unprovision();
  Provision(kLevelDefault);

  // The default offline asset "offline_clip2" does not include a
  // provider session token but "offline_clip5" does, so replace the last
  // char in init data with '5'
  std::string key_id;
  std::string client_auth;
  GetOfflineConfiguration(&key_id, &client_auth);
  key_id[key_id.size()-1] = '5';

  // Acquire license
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  GenerateKeyRequest(key_id, kLicenseTypeOffline);
  EXPECT_TRUE(!key_msg_.empty());
  std::string key_msg = key_msg_;
  std::string key_response;
  VerifyKeyRequestResponse(config_.license_server(), client_auth, false,
                           &key_response);

  CdmKeySetId key_set_id = key_set_id_;
  EXPECT_TRUE(!key_set_id_.empty());
  EXPECT_TRUE(!key_response.empty());

  // Validate signed license request
  SignedMessage signed_message;
  EXPECT_TRUE(signed_message.ParseFromString(key_msg));
  EXPECT_EQ(SignedMessage::LICENSE_REQUEST, signed_message.type());
  EXPECT_TRUE(signed_message.has_signature());
  EXPECT_TRUE(!signed_message.msg().empty());

  // Verify license request
  video_widevine::LicenseRequest license_request;
  EXPECT_TRUE(license_request.ParseFromString(signed_message.msg()));

  // Verify that the provider client token is absent in the license request
  EXPECT_FALSE(license_request.client_id().has_provider_client_token());
  EXPECT_TRUE(license_request.client_id().provider_client_token().empty());

  // Validate signed license response
  EXPECT_TRUE(signed_message.ParseFromString(key_response));
  EXPECT_EQ(SignedMessage::LICENSE, signed_message.type());
  EXPECT_TRUE(signed_message.has_signature());
  EXPECT_TRUE(!signed_message.msg().empty());

  // Verify license
  video_widevine::License license;
  EXPECT_TRUE(license.ParseFromString(signed_message.msg()));

  // Verify that the provider client token is present in the license
  EXPECT_TRUE(license.has_provider_client_token());
  EXPECT_TRUE(!license.provider_client_token().empty());

  decryptor_.CloseSession(session_id_);
  session_id_.clear();

  // Restore offline license and renew it
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));

  std::string license_server;
  GenerateRenewalRequest(kLicenseTypeOffline, &license_server);
  EXPECT_TRUE(!key_msg_.empty());
  key_msg = key_msg_;
  EXPECT_FALSE(license_server.empty());
  VerifyKeyRequestResponse(license_server, client_auth);

  // Validate signed license renewal request
  EXPECT_TRUE(signed_message.ParseFromString(key_msg));
  EXPECT_EQ(SignedMessage::LICENSE_REQUEST, signed_message.type());
  EXPECT_TRUE(signed_message.has_signature());
  EXPECT_TRUE(!signed_message.msg().empty());

  // Verify license renewal request
  EXPECT_TRUE(license_request.ParseFromString(signed_message.msg()));

  // Verify provider client token is present in the license renewal request
  EXPECT_TRUE(license_request.client_id().has_provider_client_token());
  EXPECT_TRUE(!license_request.client_id().provider_client_token().empty());

  decryptor_.CloseSession(session_id_);
  session_id_.clear();

  // Restore and release offline license
  key_set_id_.clear();
  GenerateKeyRelease(key_set_id);
  key_set_id_ = key_set_id;
  EXPECT_TRUE(!key_msg_.empty());
  key_msg = key_msg_;
  VerifyKeyRequestResponse(config_.license_server(), client_auth);

  // Validate signed license release request
  EXPECT_TRUE(signed_message.ParseFromString(key_msg));
  EXPECT_EQ(SignedMessage::LICENSE_REQUEST, signed_message.type());
  EXPECT_TRUE(signed_message.has_signature());
  EXPECT_TRUE(!signed_message.msg().empty());

  // Verify license release request
  EXPECT_TRUE(license_request.ParseFromString(signed_message.msg()));

  // Verify provider client token is present in the license release request
  EXPECT_TRUE(license_request.client_id().has_provider_client_token());
  EXPECT_TRUE(!license_request.client_id().provider_client_token().empty());
}

TEST_F(WvCdmRequestLicenseTest, QueryUnmodifiedSessionStatus) {
  // Test that the global value is returned when no properties are modifying it.
  std::string security_level;
  ASSERT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevelDefault,
                                   wvcdm::QUERY_KEY_SECURITY_LEVEL,
                                   &security_level));
  EXPECT_EQ(GetSecurityLevel(NULL), security_level);
}

TEST_F(WvCdmRequestLicenseTest, QueryModifiedSessionStatus) {
  // Test that L3 is returned when properties downgrade security.
  Unprovision();
  Provision(kLevel3);
  TestWvCdmClientPropertySet property_set_L3;
  property_set_L3.set_security_level(QUERY_VALUE_SECURITY_LEVEL_L3);

  EXPECT_EQ(QUERY_VALUE_SECURITY_LEVEL_L3, GetSecurityLevel(&property_set_L3));
}

TEST_F(WvCdmRequestLicenseTest, QueryKeyStatus) {
  Unprovision();
  Provision(kLevelDefault);

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  CdmQueryMap query_info;
  CdmQueryMap::iterator itr;
  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryKeyStatus(session_id_, &query_info));

  itr = query_info.find(wvcdm::QUERY_KEY_LICENSE_TYPE);
  ASSERT_TRUE(itr != query_info.end());
  EXPECT_EQ(wvcdm::QUERY_VALUE_STREAMING, itr->second);
  itr = query_info.find(wvcdm::QUERY_KEY_PLAY_ALLOWED);
  ASSERT_TRUE(itr != query_info.end());
  EXPECT_EQ(wvcdm::QUERY_VALUE_TRUE, itr->second);
  itr = query_info.find(wvcdm::QUERY_KEY_PERSIST_ALLOWED);
  ASSERT_TRUE(itr != query_info.end());
  EXPECT_EQ(wvcdm::QUERY_VALUE_FALSE, itr->second);
  itr = query_info.find(wvcdm::QUERY_KEY_RENEW_ALLOWED);
  ASSERT_TRUE(itr != query_info.end());
  EXPECT_EQ(wvcdm::QUERY_VALUE_TRUE, itr->second);

  int64_t remaining_time;
  std::istringstream ss;
  itr = query_info.find(wvcdm::QUERY_KEY_LICENSE_DURATION_REMAINING);
  ASSERT_TRUE(itr != query_info.end());
  ss.str(itr->second);
  ss >> remaining_time;
  ASSERT_FALSE(ss.fail());
  EXPECT_LT(0, remaining_time);
  itr = query_info.find(wvcdm::QUERY_KEY_PLAYBACK_DURATION_REMAINING);
  ASSERT_TRUE(itr != query_info.end());
  ss.clear();
  ss.str(itr->second);
  ss >> remaining_time;
  ASSERT_FALSE(ss.fail());
  EXPECT_LT(0, remaining_time);

  itr = query_info.find(wvcdm::QUERY_KEY_RENEWAL_SERVER_URL);
  ASSERT_TRUE(itr != query_info.end());
  EXPECT_LT(0u, itr->second.size());

  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, QueryStatus) {
  std::string value;
  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevelDefault,
                                   wvcdm::QUERY_KEY_SECURITY_LEVEL, &value));

  EXPECT_EQ(2u, value.size());
  EXPECT_TRUE(wvcdm::QUERY_VALUE_SECURITY_LEVEL_L3 == value ||
              wvcdm::QUERY_VALUE_SECURITY_LEVEL_L1 == value);

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_DEVICE_ID,
                                   &value));
  EXPECT_LT(0u, value.size());

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_SYSTEM_ID,
                                   &value));
  std::istringstream ss(value);
  uint32_t system_id;
  ss >> system_id;
  ASSERT_FALSE(ss.fail());
  EXPECT_TRUE(ss.eof());

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevelDefault,
                                   wvcdm::QUERY_KEY_PROVISIONING_ID, &value));
  EXPECT_TRUE(16u == value.size() || 32u == value.size())
      << "provisioning id size: " << value.size();

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(
                kLevelDefault, wvcdm::QUERY_KEY_CURRENT_HDCP_LEVEL, &value));
  EXPECT_TRUE(
      value == QUERY_VALUE_HDCP_NONE || value == QUERY_VALUE_HDCP_V1 ||
      value == QUERY_VALUE_HDCP_V2_0 || value == QUERY_VALUE_HDCP_V2_1 ||
      value == QUERY_VALUE_HDCP_V2_2 ||
      value == QUERY_VALUE_HDCP_NO_DIGITAL_OUTPUT);

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevelDefault,
                                   wvcdm::QUERY_KEY_MAX_HDCP_LEVEL, &value));
  EXPECT_TRUE(
      value == QUERY_VALUE_HDCP_NONE || value == QUERY_VALUE_HDCP_V1 ||
      value == QUERY_VALUE_HDCP_V2_0 || value == QUERY_VALUE_HDCP_V2_1 ||
      value == QUERY_VALUE_HDCP_V2_2 ||
      value == QUERY_VALUE_HDCP_NO_DIGITAL_OUTPUT);

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevelDefault,
                                   wvcdm::QUERY_KEY_USAGE_SUPPORT, &value));
  EXPECT_TRUE(value == QUERY_VALUE_TRUE || value == QUERY_VALUE_FALSE);

  EXPECT_EQ(
      wvcdm::NO_ERROR,
      decryptor_.QueryStatus(kLevelDefault,
                             wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS, &value));
  ss.clear();
  ss.str(value);
  uint32_t open_sessions;
  ss >> open_sessions;
  ASSERT_FALSE(ss.fail());
  EXPECT_TRUE(ss.eof());

  EXPECT_EQ(
      wvcdm::NO_ERROR,
      decryptor_.QueryStatus(kLevelDefault,
                             wvcdm::QUERY_KEY_MAX_NUMBER_OF_SESSIONS, &value));
  ss.clear();
  ss.str(value);
  uint32_t max_sessions;
  ss >> max_sessions;
  ASSERT_FALSE(ss.fail());
  EXPECT_TRUE(ss.eof());
  EXPECT_LE(open_sessions, max_sessions);
  EXPECT_LE(8u, max_sessions);

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(
                kLevelDefault, wvcdm::QUERY_KEY_OEMCRYPTO_API_VERSION, &value));
  ss.clear();
  ss.str(value);
  uint32_t api_version;
  ss >> api_version;
  ASSERT_FALSE(ss.fail());
  EXPECT_TRUE(ss.eof());
  EXPECT_LE(10u, api_version);

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(
                kLevelDefault, wvcdm::QUERY_KEY_RESOURCE_RATING_TIER, &value));
  ss.clear();
  ss.str(value);
  uint32_t resource_rating_tier;
  ss >> resource_rating_tier;
  ASSERT_FALSE(ss.fail());
  EXPECT_TRUE(ss.eof());

  if (api_version >= OEM_CRYPTO_API_VERSION_SUPPORTS_RESOURCE_RATING_TIER) {
    EXPECT_LE(RESOURCE_RATING_TIER_LOW, resource_rating_tier);
    EXPECT_GE(RESOURCE_RATING_TIER_HIGH, resource_rating_tier);
  } else {
    EXPECT_TRUE(resource_rating_tier < RESOURCE_RATING_TIER_LOW ||
                resource_rating_tier > RESOURCE_RATING_TIER_HIGH)
        << "resource rating tier: " << resource_rating_tier;
  }

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevelDefault,
                                   wvcdm::QUERY_KEY_OEMCRYPTO_BUILD_INFORMATION,
                                   &value));
  EXPECT_TRUE(!value.empty());

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevelDefault,
                                   wvcdm::QUERY_KEY_CURRENT_SRM_VERSION,
                                   &value));
}

TEST_F(WvCdmRequestLicenseTest, QueryStatusL3) {
  std::string value;
  EXPECT_EQ(
      wvcdm::NO_ERROR,
      decryptor_.QueryStatus(kLevel3, wvcdm::QUERY_KEY_SECURITY_LEVEL, &value));
  EXPECT_EQ(wvcdm::QUERY_VALUE_SECURITY_LEVEL_L3, value);

  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.QueryStatus(
                                 kLevel3, wvcdm::QUERY_KEY_DEVICE_ID, &value));
  EXPECT_LT(0u, value.size());

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_SYSTEM_ID,
                                   &value));
  std::istringstream ss(value);
  uint32_t default_system_id;
  ss >> default_system_id;
  ASSERT_FALSE(ss.fail());
  EXPECT_TRUE(ss.eof());

  std::string default_security_level;
  EXPECT_EQ(
      wvcdm::NO_ERROR,
      decryptor_.QueryStatus(kLevelDefault, wvcdm::QUERY_KEY_SECURITY_LEVEL,
                             &default_security_level));

  EXPECT_EQ(wvcdm::NO_ERROR, decryptor_.QueryStatus(
                                 kLevel3, wvcdm::QUERY_KEY_SYSTEM_ID, &value));
  ss.clear();
  ss.str(value);
  uint32_t system_id;
  ss >> system_id;
  ASSERT_FALSE(ss.fail());
  EXPECT_TRUE(ss.eof());
  if (default_security_level != wvcdm::QUERY_VALUE_SECURITY_LEVEL_L3) {
    EXPECT_NE(default_system_id, system_id);
  }

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevel3, wvcdm::QUERY_KEY_PROVISIONING_ID,
                                   &value));
  EXPECT_TRUE(16u == value.size() || 32u == value.size())
      << "provisioning id size: " << value.size();

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(kLevel3, wvcdm::QUERY_KEY_CURRENT_HDCP_LEVEL,
                                   &value));
  EXPECT_TRUE(
      value == QUERY_VALUE_HDCP_NONE || value == QUERY_VALUE_HDCP_V1 ||
      value == QUERY_VALUE_HDCP_V2_0 || value == QUERY_VALUE_HDCP_V2_1 ||
      value == QUERY_VALUE_HDCP_V2_2 ||
      value == QUERY_VALUE_HDCP_NO_DIGITAL_OUTPUT);

  EXPECT_EQ(
      wvcdm::NO_ERROR,
      decryptor_.QueryStatus(kLevel3, wvcdm::QUERY_KEY_MAX_HDCP_LEVEL, &value));
  EXPECT_TRUE(
      value == QUERY_VALUE_HDCP_NONE || value == QUERY_VALUE_HDCP_V1 ||
      value == QUERY_VALUE_HDCP_V2_0 || value == QUERY_VALUE_HDCP_V2_1 ||
      value == QUERY_VALUE_HDCP_V2_2 ||
      value == QUERY_VALUE_HDCP_NO_DIGITAL_OUTPUT);

  EXPECT_EQ(
      wvcdm::NO_ERROR,
      decryptor_.QueryStatus(kLevel3, wvcdm::QUERY_KEY_USAGE_SUPPORT, &value));
  EXPECT_TRUE(value == QUERY_VALUE_TRUE || value == QUERY_VALUE_FALSE);

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(
                kLevel3, wvcdm::QUERY_KEY_NUMBER_OF_OPEN_SESSIONS, &value));
  ss.clear();
  ss.str(value);
  uint32_t open_sessions;
  ss >> open_sessions;
  ASSERT_FALSE(ss.fail());
  EXPECT_TRUE(ss.eof());

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(
                kLevel3, wvcdm::QUERY_KEY_MAX_NUMBER_OF_SESSIONS, &value));
  ss.clear();
  ss.str(value);
  uint32_t max_sessions;
  ss >> max_sessions;
  ASSERT_FALSE(ss.fail());
  EXPECT_TRUE(ss.eof());
  EXPECT_LE(open_sessions, max_sessions);
  EXPECT_LE(8u, max_sessions);

  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(
                kLevel3, wvcdm::QUERY_KEY_OEMCRYPTO_API_VERSION, &value));
  ss.clear();
  ss.str(value);
  uint32_t api_version;
  ss >> api_version;
  ASSERT_FALSE(ss.fail());
  EXPECT_TRUE(ss.eof());
  EXPECT_LE(10u, api_version);
}

TEST_F(WvCdmRequestLicenseTest, QueryOemCryptoSessionId) {
  Unprovision();
  Provision(kLevelDefault);

  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  CdmQueryMap query_info;
  CdmQueryMap::iterator itr;
  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryOemCryptoSessionId(session_id_, &query_info));

  uint32_t oem_crypto_session_id;
  itr = query_info.find(wvcdm::QUERY_KEY_OEMCRYPTO_SESSION_ID);
  ASSERT_TRUE(itr != query_info.end());
  std::istringstream ss;
  ss.str(itr->second);
  ss >> oem_crypto_session_id;
  ASSERT_FALSE(ss.fail());

  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, IsSecurityLevelSupported) {
  // Level 1 may either be or not be supported. Invoking the method without
  // imposing any expecations to make sure it completes.
  bool is_supported = WvContentDecryptionModule::IsSecurityLevelSupported(
      wvcdm::kSecurityLevelL1);
  EXPECT_FALSE(WvContentDecryptionModule::IsSecurityLevelSupported(
      wvcdm::kSecurityLevelL2));
  EXPECT_TRUE(WvContentDecryptionModule::IsSecurityLevelSupported(
      wvcdm::kSecurityLevelL3));
  EXPECT_FALSE(WvContentDecryptionModule::IsSecurityLevelSupported(
      wvcdm::kSecurityLevelUnknown));
  EXPECT_FALSE(WvContentDecryptionModule::IsSecurityLevelSupported(
      wvcdm::kSecurityLevelUninitialized));
}

TEST_F(WvCdmRequestLicenseTest, DISABLED_OfflineLicenseDecryptionTest) {
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  GenerateKeyRequest(binary_key_id(), kLicenseTypeOffline);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  /*
    // key 1, encrypted, 256b
    DecryptionData data;
    data.is_encrypted = true;
    data.is_secure = false;
    data.key_id = wvcdm::a2bs_hex("30313233343536373839414243444546");
    data.encrypt_data = wvcdm::a2b_hex(
        "b6d7d2430aa82b1cb8bd32f02e1f3b2a8d84f9eddf935ced5a6a98022cbb4561"
        "8346a749fdb336858a64d7169fd0aa898a32891d14c24bed17fdc17fd62b8771"
        "a8e22e9f093fa0f2aacd293d471b8e886d5ed8d0998ab2fde2d908580ff88c93"
        "c0f0bbc14867267b3a3955bb6e7d05fca734a3aec3463d786d555cad83536ebe"
        "4496d934d40df2aba5aea98c1145a2890879568ae31bb8a85d74714a4ad75785"
        "7488523e697f5fd370eac746d56990a81cc76a178e3d6d65743520cdbc669412"
        "9e73b86214256c67430cf78662346cab3e2bdd6f095dddf75b7fb3868c5ff5ff"
        "3e1bbf08d456532ffa9df6e21a8bb2664c2d2a6d47ee78f9a6d53b2f2c8c087c");
    data.iv = wvcdm::a2b_hex("86856b9409743ca107b043e82068c7b6");
    data.block_offset = 0;
    data.decrypt_data = wvcdm::a2b_hex(
        "cc4a7fed8c5ac6e316e45317805c43e6d62a383ad738219c65e7a259dc12b46a"
        "d50a3f8ce2facec8eeadff9cfa6b649212b88602b41f6d4c510c05af07fd523a"
        "e7032634d9f8db5dd652d35f776376c5fc56e7031ed7cb28b72427fd4b367b6d"
        "8c4eb6e46ed1249de5d24a61aeb08ebd60984c10581042ca8b0ef6bc44ec34a0"
        "d4a77d68125c9bb1ace6f650e8716540f5b20d6482f7cfdf1b57a9ee9802160c"
        "a632ce42934347410abc61bb78fba11b093498572de38bca96101ecece455e3b"
        "5fef6805c44a2609cf97ce0dac7f15695c8058c590eda517f845108b90dfb29c"
        "e73f3656000399f2fd196bc6fc225f3a7b8f578237751fd485ff070b5289e5cf");

    std::vector<uint8_t> decrypt_buffer;
    size_t encrypt_length = data.encrypt_data.size();
    decrypt_buffer.resize(encrypt_length);

    EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_,
                                           data.is_encrypted,
                                           data.is_secure,
                                           data.key_id,
                                           &data.encrypt_data.front(),
                                           encrypt_length,
                                           data.iv,
                                           data.block_offset,
                                           &decrypt_buffer.front(),
                                           0));

    EXPECT_TRUE(std::equal(data.decrypt_data.begin(), data.decrypt_data.end(),
        decrypt_buffer.begin()));
  */
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, DISABLED_RestoreOfflineLicenseDecryptionTest) {
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  GenerateKeyRequest(binary_key_id(), kLicenseTypeOffline);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());
  CdmKeySetId key_set_id = key_set_id_;
  EXPECT_FALSE(key_set_id_.empty());
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));
  /*
    // key 1, encrypted, 256b
    DecryptionData data;
    data.is_encrypted = true;
    data.is_secure = false;
    data.key_id = wvcdm::a2bs_hex("30313233343536373839414243444546");
    data.encrypt_data = wvcdm::a2b_hex(
        "b6d7d2430aa82b1cb8bd32f02e1f3b2a8d84f9eddf935ced5a6a98022cbb4561"
        "8346a749fdb336858a64d7169fd0aa898a32891d14c24bed17fdc17fd62b8771"
        "a8e22e9f093fa0f2aacd293d471b8e886d5ed8d0998ab2fde2d908580ff88c93"
        "c0f0bbc14867267b3a3955bb6e7d05fca734a3aec3463d786d555cad83536ebe"
        "4496d934d40df2aba5aea98c1145a2890879568ae31bb8a85d74714a4ad75785"
        "7488523e697f5fd370eac746d56990a81cc76a178e3d6d65743520cdbc669412"
        "9e73b86214256c67430cf78662346cab3e2bdd6f095dddf75b7fb3868c5ff5ff"
        "3e1bbf08d456532ffa9df6e21a8bb2664c2d2a6d47ee78f9a6d53b2f2c8c087c");
    data.iv = wvcdm::a2b_hex("86856b9409743ca107b043e82068c7b6");
    data.block_offset = 0;
    data.decrypt_data = wvcdm::a2b_hex(
        "cc4a7fed8c5ac6e316e45317805c43e6d62a383ad738219c65e7a259dc12b46a"
        "d50a3f8ce2facec8eeadff9cfa6b649212b88602b41f6d4c510c05af07fd523a"
        "e7032634d9f8db5dd652d35f776376c5fc56e7031ed7cb28b72427fd4b367b6d"
        "8c4eb6e46ed1249de5d24a61aeb08ebd60984c10581042ca8b0ef6bc44ec34a0"
        "d4a77d68125c9bb1ace6f650e8716540f5b20d6482f7cfdf1b57a9ee9802160c"
        "a632ce42934347410abc61bb78fba11b093498572de38bca96101ecece455e3b"
        "5fef6805c44a2609cf97ce0dac7f15695c8058c590eda517f845108b90dfb29c"
        "e73f3656000399f2fd196bc6fc225f3a7b8f578237751fd485ff070b5289e5cf");

    std::vector<uint8_t> decrypt_buffer;
    size_t encrypt_length = data.encrypt_data.size();
    decrypt_buffer.resize(encrypt_length);

    EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_,
                                           data.is_encrypted,
                                           data.is_secure,
                                           data.key_id,
                                           &data.encrypt_data.front(),
                                           encrypt_length,
                                           data.iv,
                                           data.block_offset,
                                           &decrypt_buffer.front(),
                                           0));

    EXPECT_TRUE(std::equal(data.decrypt_data.begin(), data.decrypt_data.end(),
        decrypt_buffer.begin()));
  */
  decryptor_.CloseSession(session_id_);
}

// TODO(rfrias, edwinwong): pending L1 OEMCrypto due to key block handling
/*
TEST_F(WvCdmRequestLicenseTest, KeyControlBlockDecryptionTest) {
  decryptor_.OpenSession(config_.key_system(), &session_id_);
  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  DecryptionData data;

  // block 4, key 2, encrypted
  data.is_encrypted = true;
  data.is_secure = false;
  data.key_id = wvcdm::a2bs_hex("0915007CAA9B5931B76A3A85F046523E");
  data.encrypt_data = wvcdm::a2b_hex(
      "6758ac1c6ccf5d08479e3bfc62bbc0fd154aff4415aa7ed53d89e3983248d117"
      "ab5137ae7cedd9f9d7321d4cf35a7013237afbcc2d893d1d928efa94e9f7e2ed"
      "1855463cf75ff07ecc0246b90d0734f42d98aeea6a0a6d2618a8339bd0aca368"
      "4fb4a4670c0385e5bd5de9e2d8b9226851b8f8955adfbab968793b46fd152f5e"
      "e608467bb2695836f8f76c32731f5e208176d05e4b07020d58f6282c477f3840"
      "b8079c02e8bd1d03191d190cc505ddfbb2e9bacc794534c91fe409d62f5389b9"
      "35ed66134bd30f09f8da9dbfe6b8cf53d13cae34dae6e89109216e3a02233d5c"
      "2f66aef74313aae4a99b654b485b5cc207b2dc8d44a8b99a4dc196a9820eccef");
  data.iv = wvcdm::a2b_hex("c8f2d133ec357fe727cd233b3bfa755f");
  data.block_offset = 0;
  data.decrypt_data = wvcdm::a2b_hex(
      "34bab89185f1be990dfc454410c7c9093d008bc783908838b02a65b26db28759"
      "dca9dc5f117b3c8c3898358722d1b4c490e5a5d168ba0f9f8a3d4371b8fd1057"
      "2d6dd65f3f9d1850de8d76dc71bd6dc6c23da4e1223fcc3e47162033a6f82890"
      "e2bd6e9d6ddbe453830afc89064ed18078c786f8f746fcbafd88e83e7160cce5"
      "62fa7a7d699ef8421bda020d242ae4f61a786213b707c3b17b83d77510f9a07e"
      "d9d7e47d8f8fa2aff86eb26d61ddf384a27513e3facf6b1f5fe6c0d063b8856c"
      "c486d930393ea79ba73ba293eda39059e2ce9ee7bd5d31ab11f35e55dc35dfe0"
      "ea5e2ec684014852add6e29ce7d88a1595641ae4c0dd10155526b5a87560ec9d");

  std::vector<uint8_t> decrypt_buffer;
  size_t encrypt_length = data[i].encrypt_data.size();
  decrypt_buffer.resize(encrypt_length);

  EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_,
                                         data.is_encrypted,
                                         data.is_secure,
                                         data.key_id,
                                         &data.encrypt_data.front(),
                                         encrypt_length,
                                         data.iv,
                                         data.block_offset,
                                         &decrypt_buffer.front()));

    EXPECT_TRUE(std::equal(data.decrypt_data.begin(),
        data.decrypt_data.end(),
        decrypt_buffer.begin()));
  }
  decryptor_.CloseSession(session_id_);
}
*/

class WvCdmSessionSharingTest
    : public WvCdmRequestLicenseTest,
      public ::testing::WithParamInterface<SessionSharingSubSampleInfo*> {};

TEST_P(WvCdmSessionSharingTest, SessionSharingTest) {
  SessionSharingSubSampleInfo* session_sharing_info = GetParam();

  CdmIdentifier cdm_identifier = session_sharing_info->cdm_identifier;
  if (!cdm_identifier.IsEquivalentToDefault()) {
    Provision(session_sharing_info->cdm_identifier, kLevelDefault);
  }

  TestWvCdmClientPropertySet property_set;
  property_set.set_session_sharing_mode(
      session_sharing_info->session_sharing_enabled);

  ASSERT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), &property_set,
                                             cdm_identifier, NULL,
                                             &session_id_));
  CdmSessionId gp_session_id_1 = session_id_;
  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming, cdm_identifier);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  // TODO(rfrias): Move content information to ConfigTestEnv
  std::string gp_client_auth2 =
      "?source=YOUTUBE&video_id=z3S_NhwueaM&oauth=ya.gtsqawidevine";
  std::string gp_key_id2 = wvcdm::a2bs_hex(
      "000000347073736800000000"                    // blob size and pssh
      "edef8ba979d64acea3c827dcd51d21ed00000014"    // Widevine system id
      "08011210bdf1cb4fffc6506b8b7945b0bd2917fb");  // pssh data

  ASSERT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), &property_set,
                                             cdm_identifier, NULL,
                                             &session_id_));
  CdmSessionId gp_session_id_2 = session_id_;
  GenerateKeyRequest(gp_key_id2, kLicenseTypeStreaming, cdm_identifier);
  VerifyKeyRequestResponse(config_.license_server(), gp_client_auth2);

  SubSampleInfo* data = session_sharing_info->sub_sample;
  std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
  CdmDecryptionParameters decryption_parameters(
      &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
      &data->iv, data->block_offset, &decrypt_buffer[0]);
  decryption_parameters.is_encrypted = data->is_encrypted;
  decryption_parameters.is_secure = data->is_secure;
  decryption_parameters.subsample_flags = data->subsample_flags;

  if (session_sharing_info->session_sharing_enabled || !data->is_encrypted) {
    EXPECT_EQ(NO_ERROR,
              decryptor_.Decrypt(gp_session_id_2, data->validate_key_id,
                                 decryption_parameters))
        << "session_sharing_info->session_sharing_enabled "
        << session_sharing_info->session_sharing_enabled << std::endl
        << "data->is_encrypted " << data->is_encrypted << std::endl;
    EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                           decrypt_buffer.begin()));
  } else {
    EXPECT_EQ(NEED_KEY,
              decryptor_.Decrypt(gp_session_id_2, data->validate_key_id,
                                 decryption_parameters));
  }

  if (!cdm_identifier.IsEquivalentToDefault()) {
    // Unprovision both security level certs.
    decryptor_.Unprovision(kSecurityLevelL1, cdm_identifier);
    decryptor_.Unprovision(kSecurityLevelL3, cdm_identifier);
  }

  decryptor_.CloseSession(gp_session_id_1);
  decryptor_.CloseSession(gp_session_id_2);
}

INSTANTIATE_TEST_CASE_P(Cdm, WvCdmSessionSharingTest,
                        ::testing::Range(&session_sharing_sub_samples[0],
                                         &session_sharing_sub_samples[7]));

TEST_F(WvCdmRequestLicenseTest, SessionSharingTest) {
  TestWvCdmClientPropertySet property_set;
  property_set.set_session_sharing_mode(true);

  // TODO(rfrias): Move content information to ConfigTestEnv
  const std::string init_data1 = a2bs_hex(
      "000000347073736800000000"                    // blob size and pssh
      "EDEF8BA979D64ACEA3C827DCD51D21ED00000014"    // Widevine system id
      "0801121030313233343536373839616263646566");  // pssh data

  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  CdmSessionId session_id1 = session_id_;
  GenerateKeyRequest(init_data1, kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  // TODO(rfrias): Move content information to ConfigTestEnv
  std::string gp_client_auth2 =
      "?source=YOUTUBE&video_id=z3S_NhwueaM&oauth=ya.gtsqawidevine";
  std::string init_data2 = a2bs_hex(
      "000000347073736800000000"                    // blob size and pssh
      "edef8ba979d64acea3c827dcd51d21ed00000014"    // Widevine system id
      "08011210bdf1cb4fffc6506b8b7945b0bd2917fb");  // pssh data

  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  CdmSessionId session_id2 = session_id_;
  GenerateKeyRequest(init_data2, kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), gp_client_auth2);

  SubSampleInfo* data = &single_encrypted_sub_sample_short_expiry;

  std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
  CdmDecryptionParameters decryption_parameters(
      &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
      &data->iv, data->block_offset, &decrypt_buffer[0]);
  decryption_parameters.is_encrypted = data->is_encrypted;
  decryption_parameters.is_secure = data->is_secure;
  decryption_parameters.subsample_flags = data->subsample_flags;

  EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id1, data->validate_key_id,
                                         decryption_parameters));

  sleep(kSingleEncryptedSubSampleIcpLicenseDurationExpiration -
        kSingleEncryptedSubSampleIcpLicenseExpirationWindow);

  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  CdmSessionId session_id3 = session_id_;
  GenerateKeyRequest(init_data1, kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id1, data->validate_key_id,
                                         decryption_parameters));

  sleep(kSingleEncryptedSubSampleIcpLicenseExpirationWindow);
  // session 1 will be expired, shared session 3 will be used to decrypt
  EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id1, data->validate_key_id,
                                         decryption_parameters));

  decryptor_.CloseSession(session_id1);
  decryptor_.CloseSession(session_id2);
  decryptor_.CloseSession(session_id3);
}

TEST_F(WvCdmRequestLicenseTest, DecryptionKeyExpiredTest) {
  const std::string kCpKeyId = a2bs_hex(
      "000000347073736800000000"                    // blob size and pssh
      "EDEF8BA979D64ACEA3C827DCD51D21ED00000014"    // Widevine system id
      "0801121030313233343536373839616263646566");  // pssh data
  SubSampleInfo* data = &single_encrypted_sub_sample_short_expiry;
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  if (data->retrieve_key) {
    GenerateKeyRequest(kCpKeyId, kLicenseTypeStreaming);
    VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());
  }

  std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
  CdmDecryptionParameters decryption_parameters(
      &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
      &data->iv, data->block_offset, &decrypt_buffer[0]);
  decryption_parameters.is_encrypted = data->is_encrypted;
  decryption_parameters.is_secure = data->is_secure;
  decryption_parameters.subsample_flags = data->subsample_flags;
  EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                         decryption_parameters));
  sleep(kSingleEncryptedSubSampleIcpLicenseDurationExpiration +
        kSingleEncryptedSubSampleIcpLicenseExpirationWindow);
  EXPECT_EQ(NEED_KEY, decryptor_.Decrypt(session_id_, data->validate_key_id,
                                         decryption_parameters));
  decryptor_.CloseSession(session_id_);
}

TEST_F(WvCdmRequestLicenseTest, PlaybackExpiry) {
  StrictMock<TestWvCdmEventListener> listener;
  DecryptCallbackTester decrypt_callback(
      &decryptor_,
      &usage_info_sub_samples_icp[0]);
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         &listener, &session_id_);

  EXPECT_CALL(
      listener,
      OnSessionKeysChange(
          session_id_,
          AllOf(Each(Pair(_, kKeyStatusUsable)), Not(IsEmpty())), true))
      .WillOnce(Invoke(&decrypt_callback, &DecryptCallbackTester::Decrypt));
  EXPECT_CALL(listener, OnExpirationUpdate(session_id_, _));

  GenerateKeyRequest(kPsshStreamingClip21, kLicenseTypeStreaming, NULL);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  EXPECT_CALL(
      listener,
      OnSessionKeysChange(
          session_id_,
          AllOf(Each(Pair(_, kKeyStatusExpired)), Not(IsEmpty())), false));

  // Elapse time so that the key should now be considered expired.
  std::this_thread::sleep_for(kExpirationStreamingClip21PlaybackDurationTimeMs);
}

TEST_F(WvCdmRequestLicenseTest, PlaybackExpiry_DecryptBeforeLicense) {
  StrictMock<TestWvCdmEventListener> listener;
  DecryptCallbackTester decrypt_callback(
      &decryptor_,
      &usage_info_sub_samples_icp[0]);
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier,
                         &listener, &session_id_);

  // Decrypt before license is received is expected to fail but should
  // not start the playback timer
  EXPECT_FALSE(VerifyDecryption(session_id_,
                                usage_info_sub_samples_icp[0], NEED_KEY));
  std::this_thread::sleep_for(kExpirationStreamingClip21PlaybackDurationTimeMs);

  EXPECT_CALL(
      listener,
      OnSessionKeysChange(
          session_id_,
          AllOf(Each(Pair(_, kKeyStatusUsable)), Not(IsEmpty())), true))
      .WillOnce(Invoke(&decrypt_callback, &DecryptCallbackTester::Decrypt));
  EXPECT_CALL(listener, OnExpirationUpdate(session_id_, _));

  GenerateKeyRequest(kPsshStreamingClip21, kLicenseTypeStreaming, NULL);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  EXPECT_CALL(
      listener,
      OnSessionKeysChange(
          session_id_,
          AllOf(Each(Pair(_, kKeyStatusExpired)), Not(IsEmpty())), false));

  // Elapse time so that the key should now be considered expired.
  std::this_thread::sleep_for(kExpirationStreamingClip21PlaybackDurationTimeMs);
}

TEST_F(WvCdmRequestLicenseTest, SessionKeyChangeNotificationTest) {
  StrictMock<TestWvCdmEventListener> listener;
  DecryptCallbackTester decrypt_callback(
      &decryptor_,
      &single_encrypted_sub_sample_short_expiry);
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, &listener,
                         &session_id_);
  EXPECT_CALL(
      listener,
      OnSessionKeysChange(
          session_id_,
          AllOf(Each(Pair(_, kKeyStatusUsable)), Not(IsEmpty())), true))
      .WillOnce(Invoke(&decrypt_callback, &DecryptCallbackTester::Decrypt));

  EXPECT_CALL(listener, OnExpirationUpdate(session_id_, _));

  const std::string kCpKeyId = a2bs_hex(
      "000000347073736800000000"                    // blob size and pssh
      "EDEF8BA979D64ACEA3C827DCD51D21ED00000014"    // Widevine system id
      "0801121030313233343536373839616263646566");  // pssh data

  GenerateKeyRequest(kCpKeyId, kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  decryptor_.CloseSession(session_id_);
}

class WvCdmDecryptionTest
    : public WvCdmRequestLicenseTest,
      public ::testing::WithParamInterface<SubSampleInfo*> {};

TEST_P(WvCdmDecryptionTest, DecryptionTest) {
  SubSampleInfo* data = GetParam();
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  if (data->retrieve_key) {
    GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);
    VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());
  }

  uint32_t decrypt_sample_buffer_size = 0;
  uint32_t decrypt_buffer_offset = 0;
  for (size_t i = 0; i < data->num_of_subsamples; i++)
    decrypt_sample_buffer_size += (data + i)->encrypt_data.size();

  std::vector<uint8_t> decrypt_buffer(decrypt_sample_buffer_size);
  std::vector<uint8_t> expected_decrypt_data;

  for (size_t i = 0; i < data->num_of_subsamples; i++) {
    CdmDecryptionParameters decryption_parameters(
        &(data + i)->key_id, &(data + i)->encrypt_data.front(),
        (data + i)->encrypt_data.size(), &(data + i)->iv,
        (data + i)->block_offset, &decrypt_buffer[0]);
    decryption_parameters.is_encrypted = (data + i)->is_encrypted;
    decryption_parameters.is_secure = (data + i)->is_secure;
    decryption_parameters.subsample_flags = (data + i)->subsample_flags;
    decryption_parameters.decrypt_buffer_length = decrypt_sample_buffer_size;
    decryption_parameters.decrypt_buffer_offset = decrypt_buffer_offset;
    expected_decrypt_data.insert(expected_decrypt_data.end(),
                                 (data+i)->decrypt_data.begin(),
                                 (data+i)->decrypt_data.end());
    EXPECT_EQ(NO_ERROR,
              decryptor_.Decrypt(session_id_, (data + i)->validate_key_id,
                                 decryption_parameters));

    decrypt_buffer_offset += (data +i)->encrypt_data.size();
  }
  EXPECT_TRUE(std::equal(expected_decrypt_data.begin(),
                         expected_decrypt_data.end(),
                         decrypt_buffer.begin()));
  decryptor_.CloseSession(session_id_);
}

INSTANTIATE_TEST_CASE_P(Cdm, WvCdmDecryptionTest,
                        ::testing::Values(&clear_sub_sample,
                                          &clear_sub_sample_no_key,
                                          &single_encrypted_sub_sample,
                                          &switch_key_encrypted_sub_samples[0],
                                          &partial_encrypted_sub_samples[0]));

class WvCdmSessionSharingNoKeyTest
    : public WvCdmRequestLicenseTest,
      public ::testing::WithParamInterface<SubSampleInfo*> {};

TEST_P(WvCdmSessionSharingNoKeyTest, DecryptionTest) {
  SubSampleInfo* data = GetParam();

  TestWvCdmClientPropertySet property_set;
  property_set.set_session_sharing_mode(true);

  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  CdmSessionId gp_session_id_1 = session_id_;
  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);

  // TODO(rfrias): Move content information to ConfigTestEnv
  std::string gp_client_auth2 =
      "?source=YOUTUBE&video_id=z3S_NhwueaM&oauth=ya.gtsqawidevine";
  std::string gp_key_id2 = wvcdm::a2bs_hex(
      "000000347073736800000000"                    // blob size and pssh
      "edef8ba979d64acea3c827dcd51d21ed00000014"    // Widevine system id
      "08011210bdf1cb4fffc6506b8b7945b0bd2917fb");  // pssh data

  decryptor_.OpenSession(config_.key_system(), &property_set, kDefaultCdmIdentifier,
                         NULL, &session_id_);
  CdmSessionId gp_session_id_2 = session_id_;
  GenerateKeyRequest(gp_key_id2, kLicenseTypeStreaming);

  std::vector<uint8_t> decrypt_buffer(data->encrypt_data.size());
  CdmDecryptionParameters decryption_parameters(
      &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
      &data->iv, data->block_offset, &decrypt_buffer[0]);
  decryption_parameters.is_encrypted = data->is_encrypted;
  decryption_parameters.is_secure = data->is_secure;
  decryption_parameters.subsample_flags = data->subsample_flags;

  bool can_decrypt = !data->is_encrypted &&
      data->subsample_flags & OEMCrypto_FirstSubsample &&
      data->subsample_flags & OEMCrypto_LastSubsample;
  EXPECT_EQ(can_decrypt ? NO_ERROR : KEY_NOT_FOUND_IN_SESSION,
            decryptor_.Decrypt(gp_session_id_2, data->validate_key_id,
                               decryption_parameters));
  if (can_decrypt) {
    EXPECT_TRUE(std::equal(data->decrypt_data.begin(), data->decrypt_data.end(),
                           decrypt_buffer.begin()));
  }

  decryptor_.CloseSession(gp_session_id_1);
  decryptor_.CloseSession(gp_session_id_2);
}

INSTANTIATE_TEST_CASE_P(Cdm, WvCdmSessionSharingNoKeyTest,
                        ::testing::Values(&clear_sub_sample,
                                          &clear_sub_samples[0],
                                          &clear_sub_samples[1],
                                          &clear_sub_sample_no_key,
                                          &single_encrypted_sub_sample));

TEST(VersionNumberTest, VersionNumberChangeCanary) {
  std::string release_number =
      android::base::GetProperty("ro.build.version.release", "");
  ASSERT_TRUE(release_number.size() > 0);
  EXPECT_STREQ("10", release_number.c_str())
      << "The Android version number has changed. You need to update this test "
         "and also possibly update the Widevine version number in "
         "properties_android.cpp.";

  std::string widevine_version;
  ASSERT_TRUE(Properties::GetWVCdmVersion(&widevine_version));
  EXPECT_EQ("15.0.0", widevine_version)
      << "The Widevine CDM version number has changed. Did you forget to "
         "update this test after changing it?";
}

TEST_F(WvCdmRequestLicenseTest, AddHlsStreamingKeyTest) {
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  CdmAppParameterMap app_parameters;
  GenerateKeyRequest(wvcdm::KEY_MESSAGE, HLS_INIT_DATA_FORMAT,
                     kAttributeListSampleAes, app_parameters,
                     kLicenseTypeStreaming, NULL);
  //TODO(rfrias): Remove once we switch to git-on-borg
  std::string license_server = "https://proxy.uat.widevine.com/proxy";
  VerifyKeyRequestResponse(license_server, config_.client_auth());
  decryptor_.CloseSession(session_id_);
}

class WvHlsInitDataTest
    : public WvCdmRequestLicenseTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(WvHlsInitDataTest, InvalidHlsFormatTest) {
  decryptor_.OpenSession(config_.key_system(), NULL, kDefaultCdmIdentifier, NULL,
                         &session_id_);
  CdmAppParameterMap app_parameters;
  std::string init_data = GetParam();
  GenerateKeyRequest(wvcdm::INIT_DATA_NOT_FOUND, HLS_INIT_DATA_FORMAT,
                     init_data, app_parameters, kLicenseTypeStreaming, NULL);
  decryptor_.CloseSession(session_id_);
}

INSTANTIATE_TEST_CASE_P(Cdm, WvHlsInitDataTest,
                        ::testing::Values(
                            kAttributeListKeyFormatNonWidevine,
                            kAttributeListKeyFormatVersionUnregonized,
                            kAttributeListUnspecifiedIv,
                            kAttributeListUnspecifiedMethod));

class WvHlsDecryptionTest
    : public WvCdmRequestLicenseTest,
      public ::testing::WithParamInterface<HlsDecryptionInfo*> {};

TEST_P(WvHlsDecryptionTest, HlsDecryptionTest) {
  Provision(kLevel3);
  TestWvCdmClientPropertySet client_property_set;
  client_property_set.set_security_level(QUERY_VALUE_SECURITY_LEVEL_L3);
  HlsDecryptionInfo* info = GetParam();

  TestWvCdmHlsEventListener listener;
  decryptor_.OpenSession(config_.key_system(), &client_property_set,
                         kDefaultCdmIdentifier, &listener, &session_id_);
  CdmAppParameterMap app_parameters;
  GenerateKeyRequest(wvcdm::KEY_MESSAGE, HLS_INIT_DATA_FORMAT,
                     info->attribute_list, app_parameters,
                     kLicenseTypeStreaming, NULL);
  //TODO(rfrias): Remove once we switch to git-on-borg
  std::string license_server = "https://proxy.uat.widevine.com/proxy";
  VerifyKeyRequestResponse(license_server, config_.client_auth());
  CdmKeyStatusMap key_status_map = listener.GetKeyStatusMap();
  EXPECT_EQ(1u, key_status_map.size());
  KeyId key_id = key_status_map.begin()->first;
  EXPECT_EQ(KEY_ID_SIZE, key_id.size());

  for (size_t i = 0; i < info->number_of_segments; ++i) {
    HlsSegmentInfo* data = info->segment_info + i;
    std::vector<uint8_t> output_buffer(data->encrypted_data.size(), 0);
    std::vector<uint8_t> iv(data->iv.begin(), data->iv.end());
    CdmDecryptionParameters decryption_parameters(
        &key_id, reinterpret_cast<const uint8_t*>(data->encrypted_data.c_str()),
        data->encrypted_data.size(), &iv, 0, &output_buffer[0]);
    decryption_parameters.is_encrypted = true;
    decryption_parameters.is_secure = false;
    decryption_parameters.cipher_mode = kCipherModeCbc;
    if (info->sample_aes) {
      decryption_parameters.pattern_descriptor.encrypt_blocks = 1;
      decryption_parameters.pattern_descriptor.skip_blocks = 9;
    }
    decryption_parameters.subsample_flags = data->subsample_flags;
    EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, false,
                                           decryption_parameters));
    EXPECT_EQ(data->clear_data,
              std::string(reinterpret_cast<const char*>(&output_buffer[0]),
                          output_buffer.size()));
  }

  decryptor_.CloseSession(session_id_);
}

INSTANTIATE_TEST_CASE_P(Cdm, WvHlsDecryptionTest,
                        ::testing::Range(&kHlsDecryptionTestVectors[0],
                                         &kHlsDecryptionTestVectors[11]));

class WvHlsFourCCBackwardCompatibilityTest
    : public WvCdmRequestLicenseTest,
      public ::testing::WithParamInterface<HlsDecryptionInfo*> {};

TEST_P(WvHlsFourCCBackwardCompatibilityTest, HlsDecryptionTest) {
  Provision(kLevel3);
  TestWvCdmClientPropertySet client_property_set;
  client_property_set.set_security_level(QUERY_VALUE_SECURITY_LEVEL_L3);
  HlsDecryptionInfo* info = GetParam();

  TestWvCdmHlsEventListener listener;
  decryptor_.OpenSession(config_.key_system(), &client_property_set,
                         kDefaultCdmIdentifier, &listener, &session_id_);
  CdmAppParameterMap app_parameters;
  GenerateKeyRequest(wvcdm::KEY_MESSAGE, ISO_BMFF_VIDEO_MIME_TYPE,
                     info->attribute_list, app_parameters,
                     kLicenseTypeStreaming, NULL);
  //TODO(rfrias): Remove once we switch to git-on-borg
  std::string license_server = "https://proxy.uat.widevine.com/proxy";
  VerifyKeyRequestResponse(license_server, config_.client_auth());
  CdmKeyStatusMap key_status_map = listener.GetKeyStatusMap();
  EXPECT_EQ(1u, key_status_map.size());
  KeyId key_id = key_status_map.begin()->first;
  EXPECT_EQ(KEY_ID_SIZE, key_id.size());

  for (size_t i = 0; i < info->number_of_segments; ++i) {
    HlsSegmentInfo* data = info->segment_info + i;
    std::vector<uint8_t> output_buffer(data->encrypted_data.size(), 0);
    std::vector<uint8_t> iv(data->iv.begin(), data->iv.end());
    CdmDecryptionParameters decryption_parameters(
        &key_id, reinterpret_cast<const uint8_t*>(data->encrypted_data.c_str()),
        data->encrypted_data.size(), &iv, 0, &output_buffer[0]);
    decryption_parameters.is_encrypted = true;
    decryption_parameters.is_secure = false;
    decryption_parameters.cipher_mode = kCipherModeCbc;
    if (info->sample_aes) {
      decryption_parameters.pattern_descriptor.encrypt_blocks = 1;
      decryption_parameters.pattern_descriptor.skip_blocks = 9;
    }
    decryption_parameters.subsample_flags = data->subsample_flags;
    EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, false,
                                           decryption_parameters));
    EXPECT_EQ(data->clear_data,
              std::string(reinterpret_cast<const char*>(&output_buffer[0]),
                          output_buffer.size()));
  }

  decryptor_.CloseSession(session_id_);
}

INSTANTIATE_TEST_CASE_P(
    Cdm, WvHlsFourCCBackwardCompatibilityTest,
    ::testing::Range(&kHlsFourCCBackwardCompatibilityTestVectors[0],
                     &kHlsFourCCBackwardCompatibilityTestVectors[4]));

class WvCenc30Test
    : public WvCdmRequestLicenseTest,
      public ::testing::WithParamInterface<SingleSampleDecryptionInfo*> {};

TEST_P(WvCenc30Test, DecryptionTest) {
  Provision(kLevel3);
  TestWvCdmClientPropertySet client_property_set;
  client_property_set.set_security_level(QUERY_VALUE_SECURITY_LEVEL_L3);

  std::string value;
  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(
                kLevel3, wvcdm::QUERY_KEY_OEMCRYPTO_API_VERSION, &value));
  std::istringstream ss(value);
  uint32_t api_version;
  ss >> api_version;
  ASSERT_FALSE(ss.fail());
  EXPECT_TRUE(ss.eof());

  // Ability to switch between cipher modes without re-requesting a license
  // was introduced in OEMCrypto v14
  if (api_version < 14u)
    return;

  SingleSampleDecryptionInfo* info = GetParam();

  TestWvCdmHlsEventListener listener;
  decryptor_.OpenSession(config_.key_system(), &client_property_set,
                         kDefaultCdmIdentifier, &listener, &session_id_);
  CdmAppParameterMap app_parameters;
  GenerateKeyRequest(info->pssh, app_parameters,
                     kLicenseTypeStreaming, NULL);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());
  CdmKeyStatusMap key_status_map = listener.GetKeyStatusMap();
  EXPECT_EQ(8u, key_status_map.size());

  Cenc30SampleInfo* data = &info->sample_info;
  std::vector<uint8_t> output_buffer(data->encrypted_data.size(), 0);
  std::vector<uint8_t> iv(data->iv.begin(), data->iv.end());
  CdmDecryptionParameters decryption_parameters(
      &data->key_id,
      reinterpret_cast<const uint8_t*>(data->encrypted_data.c_str()),
      data->encrypted_data.size(), &iv, 0, &output_buffer[0]);
  decryption_parameters.is_encrypted = data->is_encrypted;
  decryption_parameters.is_secure = false;
  decryption_parameters.cipher_mode = data->cipher_mode;
  if (data->cipher_mode == kCipherModeCtr) {
    decryption_parameters.pattern_descriptor.encrypt_blocks = 1;
    decryption_parameters.pattern_descriptor.skip_blocks = 9;
  }
  decryption_parameters.subsample_flags = data->subsample_flags;
  EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, false,
                                         decryption_parameters));
  EXPECT_EQ(data->clear_data,
            std::string(reinterpret_cast<const char*>(&output_buffer[0]),
                        output_buffer.size()));

  decryptor_.CloseSession(session_id_);
}

INSTANTIATE_TEST_CASE_P(
    Cdm, WvCenc30Test,
    ::testing::Range(&kCenc30DecryptionData[0],
                     &kCenc30DecryptionData[4]));

class WvCenc30SwitchCipherModeTest
    : public WvCdmRequestLicenseTest,
      public ::testing::WithParamInterface<FourSampleDecryptionInfo*> {};

TEST_P(WvCenc30SwitchCipherModeTest, DecryptionTest) {
  Provision(kLevel3);
  TestWvCdmClientPropertySet client_property_set;
  client_property_set.set_security_level(QUERY_VALUE_SECURITY_LEVEL_L3);

  std::string value;
  EXPECT_EQ(wvcdm::NO_ERROR,
            decryptor_.QueryStatus(
                kLevel3, wvcdm::QUERY_KEY_OEMCRYPTO_API_VERSION, &value));
  std::istringstream ss(value);
  uint32_t api_version;
  ss >> api_version;
  ASSERT_FALSE(ss.fail());
  EXPECT_TRUE(ss.eof());

  // Ability to switch between cipher modes without re-requesting a license
  // was introduced in OEMCrypto v14
  if (api_version < 14)
    return;

  FourSampleDecryptionInfo* info = GetParam();

  TestWvCdmHlsEventListener listener;
  decryptor_.OpenSession(config_.key_system(), &client_property_set,
                         kDefaultCdmIdentifier, &listener, &session_id_);
  CdmAppParameterMap app_parameters;
  GenerateKeyRequest(info->pssh, app_parameters,
                     kLicenseTypeStreaming, NULL);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());
  CdmKeyStatusMap key_status_map = listener.GetKeyStatusMap();
  EXPECT_EQ(8u, key_status_map.size());

  for (size_t i = 0; i < N_ELEM(info->sample_info); ++i) {
    Cenc30SampleInfo* data = &info->sample_info[i];
    std::vector<uint8_t> output_buffer(data->encrypted_data.size(), 0);
    std::vector<uint8_t> iv(data->iv.begin(), data->iv.end());
    CdmDecryptionParameters decryption_parameters(
        &data->key_id,
        reinterpret_cast<const uint8_t*>(data->encrypted_data.c_str()),
        data->encrypted_data.size(), &iv, 0, &output_buffer[0]);
    decryption_parameters.is_encrypted = data->is_encrypted;
    decryption_parameters.is_secure = false;
    decryption_parameters.cipher_mode = data->cipher_mode;
    if (data->cipher_mode == kCipherModeCtr) {
      decryption_parameters.pattern_descriptor.encrypt_blocks = 1;
      decryption_parameters.pattern_descriptor.skip_blocks = 9;
    }
    decryption_parameters.subsample_flags = data->subsample_flags;
    EXPECT_EQ(NO_ERROR, decryptor_.Decrypt(session_id_, false,
                                           decryption_parameters));
    EXPECT_EQ(data->clear_data,
              std::string(reinterpret_cast<const char*>(&output_buffer[0]),
                          output_buffer.size()));
  }

  decryptor_.CloseSession(session_id_);
}

INSTANTIATE_TEST_CASE_P(
    Cdm, WvCenc30SwitchCipherModeTest,
    ::testing::Range(&kCenc30SwitchCipherData[0],
                     &kCenc30SwitchCipherData[8]));

TEST_F(WvCdmRequestLicenseTest, CloseCdmReleaseResourcesTest) {
  Provision(kAlternateCdmIdentifier1, kLevelDefault);

  // Retrieve a streaming license
  EXPECT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), NULL,
                                             kDefaultCdmIdentifier, NULL,
                                             &session_id_));
  GenerateKeyRequest(binary_key_id(), kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  // Open a few more sessions
  CdmSessionId session_id1;
  CdmSessionId session_id2;
  CdmSessionId session_id3;
  CdmSessionId session_id4;
  CdmSessionId alternate_session_id;
  EXPECT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), NULL,
                                             kDefaultCdmIdentifier, NULL,
                                             &session_id1));
  EXPECT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), NULL,
                                             kDefaultCdmIdentifier, NULL,
                                             &session_id2));
  EXPECT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), NULL,
                                             kDefaultCdmIdentifier, NULL,
                                             &session_id3));
  EXPECT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), NULL,
                                             kDefaultCdmIdentifier, NULL,
                                             &session_id4));
  EXPECT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), NULL,
                                             kAlternateCdmIdentifier1, NULL,
                                             &alternate_session_id));


  // Close all open sessions and disable the timer running for |session_id_|
  decryptor_.CloseCdm(kDefaultCdmIdentifier);

  EXPECT_FALSE(decryptor_.IsOpenSession(session_id_));
  EXPECT_FALSE(decryptor_.IsOpenSession(session_id1));
  EXPECT_FALSE(decryptor_.IsOpenSession(session_id2));
  EXPECT_FALSE(decryptor_.IsOpenSession(session_id3));
  EXPECT_FALSE(decryptor_.IsOpenSession(session_id4));
  EXPECT_TRUE(decryptor_.IsOpenSession(alternate_session_id));
}

// Enable when OEMCrypto v15 has been deployed. Currently setting a decrypt
// hash returns OEMCrypto_ERROR_NOT_IMPLEMENTED
TEST_F(WvCdmRequestLicenseTest, DISABLED_DecryptPathTest) {
  Provision(kDefaultCdmIdentifier, kLevelDefault);

  // Retrieve a streaming license
  EXPECT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), NULL,
                                             kDefaultCdmIdentifier, NULL,
                                             &session_id_));

  CdmSessionId invalid_session_id = session_id_ + "more";
  std::string frame_number = std::to_string(5);
  std::vector<uint8_t> binary_hash{ 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38 };
  const std::string hash = Base64Encode(binary_hash);

  // Invalid session id
  std::string hash_data =
      invalid_session_id + kComma + std::to_string(5) + kComma + hash;

  CdmSessionId session_id;
  EXPECT_NE(NO_ERROR, decryptor_.SetDecryptHash(hash_data, &session_id));

  // Valid hash data
  hash_data = session_id_ + kComma + std::to_string(5) + kComma + hash;
  EXPECT_EQ(NO_ERROR, decryptor_.SetDecryptHash(hash_data, &session_id));
  EXPECT_EQ(session_id_, session_id);

  decryptor_.CloseSession(session_id_);
}

class WvCdmRequestLicenseRollbackTest
    : public WvCdmRequestLicenseTest,
      public ::testing::WithParamInterface<SubSampleInfo*> {
 public:
  WvCdmRequestLicenseRollbackTest() {
    SubSampleInfo* data = &single_encrypted_sub_sample_short_expiry;
    decrypt_buffer_.resize(data->encrypt_data.size());
    decryption_parameters_ = CdmDecryptionParameters(
        &data->key_id, &data->encrypt_data.front(), data->encrypt_data.size(),
        &data->iv, data->block_offset, &decrypt_buffer_[0]);
    decryption_parameters_.is_encrypted = data->is_encrypted;
    decryption_parameters_.is_secure = data->is_secure;
    decryption_parameters_.subsample_flags = data->subsample_flags;
    validate_key_id_ = data->validate_key_id;
  }
  ~WvCdmRequestLicenseRollbackTest() {}

 protected:
  void RollbackSystemTime(time_t rollback_time_ms) {
    if (!in_rollback_state_) {
      LOGW("Rolling back system time %d ms.", rollback_time_ms);
      wall_time_before_rollback_ = std::chrono::system_clock::now();
      monotonic_time_before_rollback_ = std::chrono::steady_clock::now();
      auto modified_wall_time = wall_time_before_rollback_ -
                                std::chrono::milliseconds(rollback_time_ms);
      timespec modified_wall_time_spec;
      modified_wall_time_spec.tv_sec =
          std::chrono::duration_cast<std::chrono::seconds>(
              modified_wall_time.time_since_epoch())
              .count();
      modified_wall_time_spec.tv_nsec =
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              modified_wall_time.time_since_epoch())
              .count() %
          (1000 * 1000 * 1000);
      ASSERT_EQ(0, clock_settime(CLOCK_REALTIME, &modified_wall_time_spec));
      in_rollback_state_ = true;
    } else {
      LOGE("Can't rollback system time more than once without restoring.");
    }
  }

  void RestoreSystemTime() {
    if (in_rollback_state_) {
      LOGW("Restoring the system time.");
      auto monotonic_time_after_rollback = std::chrono::steady_clock::now();
      auto monotonic_time_diff =
          monotonic_time_after_rollback - monotonic_time_before_rollback_;
      auto real_time = wall_time_before_rollback_ + monotonic_time_diff;
      timespec real_time_spec;
      real_time_spec.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(
                                  real_time.time_since_epoch())
                                  .count();
      real_time_spec.tv_nsec =
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              real_time.time_since_epoch())
              .count() %
          (1000 * 1000 * 1000);
      ASSERT_EQ(0, clock_settime(CLOCK_REALTIME, &real_time_spec));
      in_rollback_state_ = false;
    } else {
      LOGW("System time has already been restored.");
    }
  }

  CdmResponseType Decrypt(CdmSessionId session_id) {
    std::fill(decrypt_buffer_.begin(), decrypt_buffer_.end(), 0);
    return decryptor_.Decrypt(session_id, validate_key_id_,
                              decryption_parameters_);
  }

  std::chrono::time_point<std::chrono::steady_clock>
      monotonic_time_before_rollback_;
  std::chrono::time_point<std::chrono::system_clock> wall_time_before_rollback_;
  bool in_rollback_state_ = false;
  const std::string init_data_with_expiry_ = a2bs_hex(
      "000000347073736800000000"                    // blob size and pssh
      "EDEF8BA979D64ACEA3C827DCD51D21ED00000014"    // Widevine system id
      "0801121030313233343536373839616263646566");  // pssh data
  // Expiration of the key corresponding to init_data_with_expiry_ with a window
  // to ensure expiration.
  const time_t kExpirationTimeMs_ =
      kSingleEncryptedSubSampleIcpLicenseDurationExpiration * 1000;
  const time_t kExpirationWindowMs_ =
      kSingleEncryptedSubSampleIcpLicenseExpirationWindow * 1000;
  const time_t kExpirationWithWindowMs_ =
      (kSingleEncryptedSubSampleIcpLicenseDurationExpiration +
       kSingleEncryptedSubSampleIcpLicenseExpirationWindow) *
      1000;
  CdmDecryptionParameters decryption_parameters_;
  std::vector<uint8_t> decrypt_buffer_;
  bool validate_key_id_;
};

TEST_F(WvCdmRequestLicenseRollbackTest, Streaming_ExpireAfterRollback) {
  Unprovision();
  Provision(kLevelDefault);

  ASSERT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), nullptr,
                                             kDefaultCdmIdentifier, nullptr,
                                             &session_id_));
  GenerateKeyRequest(init_data_with_expiry_, kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  // Verify that we can decrypt a subsample to begin with.
  EXPECT_EQ(NO_ERROR, Decrypt(session_id_));

  RollbackSystemTime(kExpirationWithWindowMs_);

  // Elapse time so that the key should now be considered expired.
  std::this_thread::sleep_for(
      std::chrono::milliseconds(kExpirationWithWindowMs_));

  // Verify that we can no longer decrypt a subsample due to key expiration.
  EXPECT_EQ(NEED_KEY, Decrypt(session_id_));

  RestoreSystemTime();

  ASSERT_EQ(NO_ERROR, decryptor_.CloseSession(session_id_));
}

TEST_F(WvCdmRequestLicenseRollbackTest, Streaming_ExpireBeforeRollback) {
  Unprovision();
  Provision(kLevelDefault);

  ASSERT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), nullptr,
                                             kDefaultCdmIdentifier, nullptr,
                                             &session_id_));
  GenerateKeyRequest(init_data_with_expiry_, kLicenseTypeStreaming);
  VerifyKeyRequestResponse(config_.license_server(), config_.client_auth());

  // Elapse time so that the key should now be considered expired.
  std::this_thread::sleep_for(
      std::chrono::milliseconds(kExpirationWithWindowMs_));

  EXPECT_EQ(NEED_KEY, Decrypt(session_id_));

  RollbackSystemTime(kExpirationWithWindowMs_);

  // Verify that we still can't decrypt even if we rollbacked the clock.
  EXPECT_EQ(NEED_KEY, Decrypt(session_id_));

  RestoreSystemTime();

  ASSERT_EQ(NO_ERROR, decryptor_.CloseSession(session_id_));
}

TEST_F(WvCdmRequestLicenseRollbackTest, Offline_RollbackBeforeRestoreKey) {
  Unprovision();
  Provision(kLevelDefault);

  std::string unused_key_id;
  std::string client_auth;
  GetOfflineConfiguration(&unused_key_id, &client_auth);

  ASSERT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), nullptr,
                                             kDefaultCdmIdentifier, nullptr,
                                             &session_id_));
  GenerateKeyRequest(init_data_with_expiry_, kLicenseTypeOffline);
  VerifyKeyRequestResponse(config_.license_server(), client_auth);

  // Verify that we can decrypt a subsample to begin with.
  EXPECT_EQ(NO_ERROR, Decrypt(session_id_));

  CdmKeySetId key_set_id = key_set_id_;
  EXPECT_FALSE(key_set_id_.empty());
  decryptor_.CloseSession(session_id_);

  // This number must be > the time between GenerateKeyRequest and this call.
  RollbackSystemTime(10 * 1000);

  session_id_.clear();
  decryptor_.OpenSession(config_.key_system(), nullptr, kDefaultCdmIdentifier,
                         nullptr, &session_id_);

  decryptor_.RestoreKey(session_id_, key_set_id);

  // Verify we can't decrypt.
  EXPECT_EQ(DECRYPT_NOT_READY, Decrypt(session_id_));

  RestoreSystemTime();

  // Sleep for a little bit to account for the execution time of OpenSession and
  // RestoreKey.
  std::this_thread::sleep_for(
      std::chrono::milliseconds(kExpirationTimeMs_ / 2));

  // Verify we can decrypt.
  EXPECT_EQ(NO_ERROR, Decrypt(session_id_));

  ASSERT_EQ(NO_ERROR, decryptor_.CloseSession(session_id_));
}

TEST_F(WvCdmRequestLicenseRollbackTest,
       Offline_RollbackAndExpireAfterRestoreKey) {
  Unprovision();
  Provision(kLevelDefault);

  std::string unused_key_id;
  std::string client_auth;
  GetOfflineConfiguration(&unused_key_id, &client_auth);

  ASSERT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), nullptr,
                                             kDefaultCdmIdentifier, nullptr,
                                             &session_id_));
  GenerateKeyRequest(init_data_with_expiry_, kLicenseTypeOffline);
  VerifyKeyRequestResponse(config_.license_server(), client_auth);

  CdmKeySetId key_set_id = key_set_id_;
  EXPECT_FALSE(key_set_id_.empty());
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  decryptor_.OpenSession(config_.key_system(), nullptr, kDefaultCdmIdentifier,
                         nullptr, &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));

  RollbackSystemTime(kExpirationWithWindowMs_);

  // Elapse time so that the key should now be considered expired.
  std::this_thread::sleep_for(
      std::chrono::milliseconds(kExpirationWithWindowMs_));

  // Verify that we can no longer decrypt a subsample due to key expiration.
  EXPECT_EQ(NEED_KEY, Decrypt(session_id_));

  RestoreSystemTime();

  ASSERT_EQ(NO_ERROR, decryptor_.CloseSession(session_id_));
}

TEST_F(WvCdmRequestLicenseRollbackTest,
       Offline_ExpireAndRollbackAfterRestoreKey) {
  Unprovision();
  Provision(kLevelDefault);

  std::string unused_key_id;
  std::string client_auth;
  GetOfflineConfiguration(&unused_key_id, &client_auth);

  ASSERT_EQ(NO_ERROR, decryptor_.OpenSession(config_.key_system(), nullptr,
                                             kDefaultCdmIdentifier, nullptr,
                                             &session_id_));
  GenerateKeyRequest(init_data_with_expiry_, kLicenseTypeOffline);
  VerifyKeyRequestResponse(config_.license_server(), client_auth);

  CdmKeySetId key_set_id = key_set_id_;
  EXPECT_FALSE(key_set_id_.empty());
  decryptor_.CloseSession(session_id_);

  session_id_.clear();
  decryptor_.OpenSession(config_.key_system(), nullptr, kDefaultCdmIdentifier,
                         nullptr, &session_id_);
  EXPECT_EQ(wvcdm::KEY_ADDED, decryptor_.RestoreKey(session_id_, key_set_id));

  // Elapse time so that the key should now be considered expired.
  std::this_thread::sleep_for(
      std::chrono::milliseconds(kExpirationWithWindowMs_));

  RollbackSystemTime(kExpirationWithWindowMs_);

  // Verify that we can no longer decrypt a subsample due to key expiration.
  EXPECT_EQ(NEED_KEY, Decrypt(session_id_));

  RestoreSystemTime();

  ASSERT_EQ(NO_ERROR, decryptor_.CloseSession(session_id_));
}

}  // namespace wvcdm
