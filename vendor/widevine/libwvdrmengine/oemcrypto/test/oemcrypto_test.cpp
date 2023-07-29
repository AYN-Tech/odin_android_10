// Copyright 2018 Google LLC. All Rights Reserved. This file and proprietary
// source code may only be used and distributed under the Widevine Master
// License Agreement.
//
// OEMCrypto unit tests
//
#include <ctype.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

#include "OEMCryptoCENC.h"
#include "log.h"
#include "oec_device_features.h"
#include "oec_session_util.h"
#include "oec_test_data.h"
#include "oemcrypto_session_tests_helper.h"
#include "oemcrypto_types.h"
#include "platform.h"
#include "string_conversions.h"
#include "wvcrc32.h"

using ::testing::Bool;
using ::testing::Combine;
using ::testing::Range;
using ::testing::Values;
using ::testing::WithParamInterface;
using ::testing::tuple;
using namespace std;

namespace std {  // GTest wants PrintTo to be in the std namespace.
void PrintTo(const tuple<OEMCrypto_CENCEncryptPatternDesc, OEMCryptoCipherMode,
                         bool>& param,
             ostream* os) {
  OEMCrypto_CENCEncryptPatternDesc pattern = ::testing::get<0>(param);
  OEMCryptoCipherMode mode = ::testing::get<1>(param);
  bool decrypt_inplace = ::testing::get<2>(param);
  *os << ((mode == OEMCrypto_CipherMode_CTR) ? "CTR mode" : "CBC mode")
      << ", encrypt=" << pattern.encrypt << ", skip=" << pattern.skip
      << ", decrypt in place = " << (decrypt_inplace ? "true" : "false");
}
}  // namespace std

namespace wvoec {
namespace {
// Resource tiers:
const size_t KiB = 1024;
const size_t MiB = 1024 * 1024;
// With OEMCrypto v15 and above, we have different resource requirements
// depending on the resource rating reported by OEMCrypto.  This function looks
// up the required value for the specified resource for the target OEMCrypto
// library.
template<typename T, size_t N>
T GetResourceValue(T (&resource_values)[N]) {
  if (global_features.resource_rating < 1) return resource_values[0];
  if (global_features.resource_rating > N) return resource_values[N-1];
  return resource_values[global_features.resource_rating-1];
}
const size_t kMaxSampleSize[] = {   1000*KiB,   2*MiB,   4*MiB};
const size_t kMaxNumberSubsamples[] = {   10,      16,      32};
const size_t kMaxSubsampleSize[] = { 100*KiB, 500*KiB,   1*MiB};
const size_t kMaxGenericBuffer[] = {  10*KiB, 100*KiB, 500*KiB};
const size_t kMaxConcurrentSession[] = {  10,      20,      20};
const size_t kMaxKeysPerSession   [] = {   4,      20,      20};
// Note: Frame rate and simultaneous playback are specified by resource rating,
// but are tested at the system level, so there are no unit tests for frame
// rate.

int GetRandBytes(unsigned char* buf, int num) {
  // returns 1 on success, -1 if not supported, or 0 if other failure.
  return RAND_bytes(buf, num);
}
}  // namespace

class OEMCryptoClientTest : public ::testing::Test, public SessionUtil {
 protected:
  OEMCryptoClientTest() {}

  void SetUp() override {
    ::testing::Test::SetUp();
    wvcdm::g_cutoff = wvcdm::LOG_INFO;
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    LOGD("Running test %s.%s", test_info->test_case_name(), test_info->name());
    OEMCrypto_SetSandbox(kTestSandbox, sizeof(kTestSandbox));
    ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_Initialize());
  }

  void TearDown() override {
    OEMCrypto_Terminate();
    ::testing::Test::TearDown();
  }

  const uint8_t* find(const vector<uint8_t>& message,
                      const vector<uint8_t>& substring) {
    vector<uint8_t>::const_iterator pos = search(
        message.begin(), message.end(), substring.begin(), substring.end());
    if (pos == message.end()) {
      return NULL;
    }
    return &(*pos);
  }
};

//
// General tests.
// This test is first, becuase it might give an idea why other
// tests are failing when the device has the wrong keybox installed.
TEST_F(OEMCryptoClientTest, VersionNumber) {
  const char* level = OEMCrypto_SecurityLevel();
  ASSERT_NE((char*)NULL, level);
  ASSERT_EQ('L', level[0]);
  cout << "             OEMCrypto Security Level is " << level << endl;
  uint32_t version = OEMCrypto_APIVersion();
  cout << "             OEMCrypto API version is " << version << endl;
  if (OEMCrypto_SupportsUsageTable()) {
    cout << "             OEMCrypto supports usage tables." << endl;
  } else {
    cout << "             OEMCrypto does not support usage tables." << endl;
  }
  if (version >= 15) {
    const char* build_info = OEMCrypto_BuildInformation();
    ASSERT_TRUE(build_info != NULL);
    ASSERT_TRUE(strnlen(build_info, 256) <= 256)
        << "BuildInformation should be a short printable string.";
    cout << "             BuildInformation: " << build_info << endl;
  }
  ASSERT_GE(version, 8u);
  ASSERT_LE(version, 15u);
}

// The resource rating is a number from 1 to 3, defined API 15.
TEST_F(OEMCryptoClientTest, ResourceRatingAPI15) {
  ASSERT_GE(OEMCrypto_ResourceRatingTier(), 1u);
  ASSERT_LE(OEMCrypto_ResourceRatingTier(), 3u);
}

// OEMCrypto must declare what type of provisioning scheme it uses.
TEST_F(OEMCryptoClientTest, ProvisioningDeclaredAPI12) {
  OEMCrypto_ProvisioningMethod provisioning_method =
      OEMCrypto_GetProvisioningMethod();
  cout << "             Provisioning method = "
       << ProvisioningMethodName(provisioning_method) << endl;
  ASSERT_NE(OEMCrypto_ProvisioningError, provisioning_method);
}

const char* HDCPCapabilityAsString(OEMCrypto_HDCP_Capability value) {
  switch (value) {
    case HDCP_NONE:
      return "No HDCP supported, no secure data path";
    case HDCP_V1:
      return "HDCP version 1.0";
    case HDCP_V2:
      return "HDCP version 2.0";
    case HDCP_V2_1:
      return "HDCP version 2.1";
    case HDCP_V2_2:
      return "HDCP version 2.2";
    case HDCP_V2_3:
      return "HDCP version 2.3";
    case HDCP_NO_DIGITAL_OUTPUT:
      return "No HDCP device attached/using local display with secure path";
    default:
      return "<INVALID VALUE>";
  }
}

TEST_F(OEMCryptoClientTest, CheckHDCPCapabilityAPI09) {
  OEMCryptoResult sts;
  OEMCrypto_HDCP_Capability current, maximum;
  sts = OEMCrypto_GetHDCPCapability(&current, &maximum);
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  printf("             Current HDCP Capability: 0x%02x = %s.\n", current,
         HDCPCapabilityAsString(current));
  printf("             Maximum HDCP Capability: 0x%02x = %s.\n", maximum,
         HDCPCapabilityAsString(maximum));
}

TEST_F(OEMCryptoClientTest, CheckSRMCapabilityV13) {
  // This just tests some trivial functionality of the SRM update functions.
  bool supported = OEMCrypto_IsSRMUpdateSupported();
  printf("             Update SRM Supported: %s.\n",
         supported ? "true" : "false");
  uint16_t version = 0;
  OEMCryptoResult current_result = OEMCrypto_GetCurrentSRMVersion(&version);
  if (current_result == OEMCrypto_SUCCESS) {
    printf("             Current SRM Version: %d.\n", version);
    EXPECT_NE(OEMCrypto_SUCCESS, OEMCrypto_GetCurrentSRMVersion(NULL));
  } else if (current_result == OEMCrypto_LOCAL_DISPLAY_ONLY) {
    printf("             Current SRM Status: Local Display Only.\n");
  } else {
    EXPECT_EQ(OEMCrypto_ERROR_NOT_IMPLEMENTED, current_result);
  }
  vector<uint8_t> bad_srm(42);
  GetRandBytes(bad_srm.data(), bad_srm.size());
  EXPECT_NE(OEMCrypto_SUCCESS,
            OEMCrypto_LoadSRM(bad_srm.data(), bad_srm.size()));
}

TEST_F(OEMCryptoClientTest, CheckMaxNumberOfSessionsAPI10) {
  size_t sessions_count;
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_GetNumberOfOpenSessions(&sessions_count));
  ASSERT_EQ(0u, sessions_count);
  size_t maximum;
  OEMCryptoResult sts = OEMCrypto_GetMaxNumberOfSessions(&maximum);
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  printf("             Max Number of Sessions: %zu.\n", maximum);
  size_t required_max = GetResourceValue(kMaxConcurrentSession);
  ASSERT_GE(maximum, required_max);
}

//
// initialization tests
//
TEST_F(OEMCryptoClientTest, NormalInitTermination) {
  // Should be able to terminate OEMCrypto, and then restart it.
  ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_Terminate());
  OEMCrypto_SetSandbox(kTestSandbox, sizeof(kTestSandbox));
  ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_Initialize());
}

//
// Session Tests
//
TEST_F(OEMCryptoClientTest, NormalSessionOpenClose) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.close());
}

TEST_F(OEMCryptoClientTest, TwoSessionsOpenClose) {
  Session s1;
  Session s2;
  ASSERT_NO_FATAL_FAILURE(s1.open());
  ASSERT_NO_FATAL_FAILURE(s2.open());
  ASSERT_NO_FATAL_FAILURE(s1.close());
  ASSERT_NO_FATAL_FAILURE(s2.close());
}

// This test should still pass for API v9.  A better test is below, but it only
// works for API v10.
TEST_F(OEMCryptoClientTest, EightSessionsOpenClose) {
  vector<Session> s(8);
  for (int i = 0; i < 8; i++) {
    ASSERT_NO_FATAL_FAILURE(s[i].open());
  }
  for (int i = 0; i < 8; i++) {
    ASSERT_NO_FATAL_FAILURE(s[i].close());
  }
}

// This test verifies that OEMCrypto can open approximately as many sessions as
// it claims.
TEST_F(OEMCryptoClientTest, MaxSessionsOpenCloseAPI10) {
  size_t sessions_count;
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_GetNumberOfOpenSessions(&sessions_count));
  ASSERT_EQ(0u, sessions_count);
  size_t max_sessions;
  ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_GetMaxNumberOfSessions(&max_sessions));
  // We expect OEMCrypto implementations support at least this many sessions.
  size_t required_number = GetResourceValue(kMaxConcurrentSession);
  ASSERT_GE(max_sessions, required_number);
  // We allow GetMaxNumberOfSessions to return an estimate.  This tests with a
  // pad of 5%. Even if it's just an estimate, we still require 8 sessions.
  size_t max_sessions_with_pad =
      max(max_sessions * 19 / 20, required_number);
  vector<OEMCrypto_SESSION> sessions;
  // Limit the number of sessions for testing.
  const size_t kMaxNumberOfSessionsForTesting = 0x100u;
  for (size_t i = 0; i < kMaxNumberOfSessionsForTesting; i++) {
    OEMCrypto_SESSION session_id;
    OEMCryptoResult sts = OEMCrypto_OpenSession(&session_id);
    // GetMaxNumberOfSessions might be an estimate. We allow OEMCrypto to report
    // a max that is less than what is actually supported. Assume the number
    // returned is |max|. OpenSessions shall not fail if number of active
    // sessions is less than |max|; OpenSessions should fail with
    // OEMCrypto_ERROR_TOO_MANY_SESSIONS if too many sessions are open.
    if (sts != OEMCrypto_SUCCESS) {
      ASSERT_EQ(OEMCrypto_ERROR_TOO_MANY_SESSIONS, sts);
      ASSERT_GE(i, max_sessions_with_pad);
      break;
    }
    ASSERT_EQ(OEMCrypto_SUCCESS,
              OEMCrypto_GetNumberOfOpenSessions(&sessions_count));
    ASSERT_EQ(i + 1, sessions_count);
    sessions.push_back(session_id);
  }
  for (size_t i = 0; i < sessions.size(); i++) {
    ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_CloseSession(sessions[i]));
    ASSERT_EQ(OEMCrypto_SUCCESS,
              OEMCrypto_GetNumberOfOpenSessions(&sessions_count));
    ASSERT_EQ(sessions.size() - i - 1, sessions_count);
  }
  if (sessions.size() == kMaxNumberOfSessionsForTesting) {
    printf(
        "             MaxSessionsOpenClose: reaches "
        "kMaxNumberOfSessionsForTesting(%zu). GetMaxNumberOfSessions = %zu. "
        "ERROR_TOO_MANY_SESSIONS not tested.",
        kMaxNumberOfSessionsForTesting, max_sessions);
  }
}

// Verify that GetRandom does work, and does some sanity checks on how random
// the data is.  Basically, we say that calling GetRandom twice should not
// generate much overlap.
TEST_F(OEMCryptoClientTest, GetRandomLargeBuffer) {
  // 32 bytes.  Not very large, but that's all we really need in one call.
  const size_t size = 32;
  uint8_t data1[size];
  uint8_t data2[size];
  memset(data1, 0, size);
  memset(data2, 0, size);
  ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_GetRandom(data1, size));
  ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_GetRandom(data2, size));
  // We don't have enough data to see that the data is really random,
  // so we'll just do a spot check that two calls don't return the same values.
  int count = 0;
  for (size_t i = 0; i < size; i++) {
    if (data1[i] == data2[i]) count++;
  }
  ASSERT_LE(count, 3);  // P(count > 3) = 1/256^3 = 6e-8.
}

TEST_F(OEMCryptoClientTest, GenerateNonce) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  s.GenerateNonce();
}

TEST_F(OEMCryptoClientTest, GenerateTwoNonces) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  s.GenerateNonce();
  uint32_t nonce1 = s.get_nonce();
  s.GenerateNonce();
  uint32_t nonce2 = s.get_nonce();
  ASSERT_TRUE(nonce1 != nonce2);  // Very unlikely to be equal.
}

// OEMCrypto should limit the number of nonces that it can generate in one
// second.  A flood of nonce requests can be used for a replay attack, which we
// wish to protect against.
TEST_F(OEMCryptoClientTest, PreventNonceFloodAPI09) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  int error_counter = 0;
  time_t test_start = time(NULL);
  // More than 20 nonces per second should generate an error.
  // To allow for some slop, we actually test for more.
  const int kFloodCount = 80;
  for (int i = 0; i < kFloodCount; i++) {
    s.GenerateNonce(&error_counter);
  }
  time_t test_end = time(NULL);
  int valid_counter = kFloodCount - error_counter;
  // Either oemcrypto should enforce a delay, or it should return an error from
  // GenerateNonce -- in either case the number of valid nonces is rate
  // limited.  We add two seconds to allow for round off error in both
  // test_start and test_end.
  EXPECT_LE(valid_counter, 20 * (test_end - test_start + 2));
  error_counter = 0;
  sleep(2);  // After a pause, we should be able to regenerate nonces.
  s.GenerateNonce(&error_counter);
  EXPECT_EQ(0, error_counter);
}

// Prevent a nonce flood even if each nonce is in a different session.
TEST_F(OEMCryptoClientTest, PreventNonceFlood2API09) {
  int error_counter = 0;
  time_t test_start = time(NULL);
  // More than 20 nonces per second should generate an error.
  // To allow for some slop, we actually test for more.
  const int kFloodCount = 80;
  for (int i = 0; i < kFloodCount; i++) {
    Session s;
    ASSERT_NO_FATAL_FAILURE(s.open());
    s.GenerateNonce(&error_counter);
  }
  time_t test_end = time(NULL);
  int valid_counter = kFloodCount - error_counter;
  // Either oemcrypto should enforce a delay, or it should return an error from
  // GenerateNonce -- in either case the number of valid nonces is rate
  // limited.  We add two seconds to allow for round off error in both
  // test_start and test_end.
  EXPECT_LE(valid_counter, 20 * (test_end - test_start + 2));
  error_counter = 0;
  sleep(2);  // After a pause, we should be able to regenerate nonces.
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  s.GenerateNonce(&error_counter);
  EXPECT_EQ(0, error_counter);
}

// Prevent a nonce flood even if some nonces are in a different session.  This
// is different from the test above because there are several session open at
// the same time.  We want to make sure you can't get a flood of nonces by
// opening a flood of sessions.
TEST_F(OEMCryptoClientTest, PreventNonceFlood3API09) {
  int request_counter = 0;
  int error_counter = 0;
  time_t test_start = time(NULL);
  // More than 20 nonces per second should generate an error.
  // To allow for some slop, we actually test for more.
  Session s[8];
  for (int i = 0; i < 8; i++) {
    ASSERT_NO_FATAL_FAILURE(s[i].open());
    for (int j = 0; j < 10; j++) {
      request_counter++;
      s[i].GenerateNonce(&error_counter);
    }
  }
  time_t test_end = time(NULL);
  int valid_counter = request_counter - error_counter;
  // Either oemcrypto should enforce a delay, or it should return an error from
  // GenerateNonce -- in either case the number of valid nonces is rate
  // limited.  We add two seconds to allow for round off error in both
  // test_start and test_end.
  EXPECT_LE(valid_counter, 20 * (test_end - test_start + 2));
  error_counter = 0;
  sleep(2);  // After a pause, we should be able to regenerate nonces.
  s[0].GenerateNonce(&error_counter);
  EXPECT_EQ(0, error_counter);
}

// This verifies that CopyBuffer works, even before a license has been loaded.
TEST_F(OEMCryptoClientTest, ClearCopyTestAPI10) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  const int kDataSize = 256;
  vector<uint8_t> input_buffer(kDataSize);
  GetRandBytes(input_buffer.data(), input_buffer.size());
  vector<uint8_t> output_buffer(kDataSize);
  OEMCrypto_DestBufferDesc dest_buffer;
  dest_buffer.type = OEMCrypto_BufferType_Clear;
  dest_buffer.buffer.clear.address = output_buffer.data();
  dest_buffer.buffer.clear.max_length = output_buffer.size();
  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_CopyBuffer(s.session_id(), input_buffer.data(),
                           input_buffer.size(), &dest_buffer,
                           OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample));
  ASSERT_EQ(input_buffer, output_buffer);
  ASSERT_EQ(OEMCrypto_ERROR_INVALID_CONTEXT,
            OEMCrypto_CopyBuffer(
                s.session_id(), NULL, input_buffer.size(), &dest_buffer,
                OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample));
  ASSERT_EQ(OEMCrypto_ERROR_INVALID_CONTEXT,
            OEMCrypto_CopyBuffer(
                s.session_id(), input_buffer.data(), input_buffer.size(), NULL,
                OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample));
  dest_buffer.buffer.clear.address = NULL;
  ASSERT_EQ(
      OEMCrypto_ERROR_INVALID_CONTEXT,
      OEMCrypto_CopyBuffer(s.session_id(), input_buffer.data(),
                           input_buffer.size(), &dest_buffer,
                           OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample));
  dest_buffer.buffer.clear.address = output_buffer.data();
  dest_buffer.buffer.clear.max_length = output_buffer.size() - 1;
  ASSERT_EQ(
      OEMCrypto_ERROR_SHORT_BUFFER,
      OEMCrypto_CopyBuffer(s.session_id(), input_buffer.data(),
                           input_buffer.size(), &dest_buffer,
                           OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample));
}

// This verifies that CopyBuffer works on the maximum required buffer size.
TEST_F(OEMCryptoClientTest, ClearCopyTestLargeSubsample) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  size_t max_size = GetResourceValue(kMaxSubsampleSize);
  vector<uint8_t> input_buffer(max_size);
  GetRandBytes(input_buffer.data(), input_buffer.size());
  vector<uint8_t> output_buffer(max_size);
  OEMCrypto_DestBufferDesc dest_buffer;
  dest_buffer.type = OEMCrypto_BufferType_Clear;
  dest_buffer.buffer.clear.address = output_buffer.data();
  dest_buffer.buffer.clear.max_length = output_buffer.size();
  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_CopyBuffer(s.session_id(), input_buffer.data(),
                           input_buffer.size(), &dest_buffer,
                           OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample));
  ASSERT_EQ(input_buffer, output_buffer);
}

TEST_F(OEMCryptoClientTest, CanLoadTestKeys) {
  ASSERT_NE(DeviceFeatures::NO_METHOD, global_features.derive_key_method)
      << "Session tests cannot run with out a test keybox or RSA cert.";
}

// Tests using this class are only used for devices with a keybox.  They are not
// run for devices with an OEM Certificate.
class OEMCryptoKeyboxTest : public OEMCryptoClientTest {
  void SetUp() override {
    OEMCryptoClientTest::SetUp();
    OEMCryptoResult sts = OEMCrypto_IsKeyboxValid();
    // If the production keybox is valid, use it for these tests.  Most of the
    // other tests will use a test keybox anyway, but it's nice to check the
    // device ID for the real keybox if we can.
    if (sts == OEMCrypto_SUCCESS) return;
    printf("Production keybox is NOT valid.  All tests use test keybox.\n");
    ASSERT_EQ(
        OEMCrypto_SUCCESS,
        OEMCrypto_LoadTestKeybox(reinterpret_cast<const uint8_t*>(&kTestKeybox),
                                 sizeof(kTestKeybox)));
  }
};

// This test is used to print the device ID to stdout.
TEST_F(OEMCryptoKeyboxTest, NormalGetDeviceId) {
  OEMCryptoResult sts;
  uint8_t dev_id[128] = {0};
  size_t dev_id_len = 128;
  sts = OEMCrypto_GetDeviceID(dev_id, &dev_id_len);
  cout << "             NormalGetDeviceId: dev_id = " << dev_id
       << " len = " << dev_id_len << endl;
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
}

TEST_F(OEMCryptoKeyboxTest, GetDeviceIdShortBuffer) {
  OEMCryptoResult sts;
  uint8_t dev_id[128];
  uint32_t req_len = 0;
  for (int i = 0; i < 128; ++i) {
    dev_id[i] = 0x55;
  }
  dev_id[127] = '\0';
  size_t dev_id_len = req_len;
  sts = OEMCrypto_GetDeviceID(dev_id, &dev_id_len);
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER, sts);
  // On short buffer error, function should return minimum buffer length
  ASSERT_TRUE(dev_id_len > req_len);
  // Should also return short buffer if passed a zero length and a null buffer.
  dev_id_len = req_len;
  sts = OEMCrypto_GetDeviceID(NULL, &dev_id_len);
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER, sts);
  // On short buffer error, function should return minimum buffer length
  ASSERT_TRUE(dev_id_len > req_len);
}

TEST_F(OEMCryptoKeyboxTest, NormalGetKeyData) {
  OEMCryptoResult sts;
  uint8_t key_data[256];
  size_t key_data_len = sizeof(key_data);
  sts = OEMCrypto_GetKeyData(key_data, &key_data_len);

  uint32_t* data = reinterpret_cast<uint32_t*>(key_data);
  printf("             NormalGetKeyData: system_id = %d = 0x%04X, version=%d\n",
         htonl(data[1]), htonl(data[1]), htonl(data[0]));
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
}

TEST_F(OEMCryptoKeyboxTest, GetKeyDataNullPointer) {
  OEMCryptoResult sts;
  uint8_t key_data[256];
  sts = OEMCrypto_GetKeyData(key_data, NULL);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// This test makes sure the installed keybox is valid.  It doesn't really check
// that it is a production keybox.  That must be done by an integration test.
TEST_F(OEMCryptoKeyboxTest, ProductionKeyboxValid) {
  ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_IsKeyboxValid());
}

// This tests GenerateDerivedKeys with an 8k context.
TEST_F(OEMCryptoKeyboxTest, GenerateDerivedKeysFromKeyboxLargeBuffer) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  vector<uint8_t> mac_context(kMaxMessageSize);
  vector<uint8_t> enc_context(kMaxMessageSize);
  // Stripe the data so the two vectors are not identical, and not all zeroes.
  for (size_t i = 0; i < kMaxMessageSize; i++) {
    mac_context[i] = i % 0x100;
    enc_context[i] = (3 * i) % 0x100;
  }
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_GenerateDerivedKeys(
                s.session_id(), mac_context.data(), mac_context.size(),
                enc_context.data(),enc_context.size()));
}

// This class is for tests that have an OEM Certificate instead of a keybox.
class OEMCryptoProv30Test : public OEMCryptoClientTest {};

// This verifies that the device really does claim to have a certificate.
// It should be filtered out for devices that have a keybox.
TEST_F(OEMCryptoProv30Test, DeviceClaimsOEMCertificate) {
  ASSERT_EQ(OEMCrypto_OEMCertificate, OEMCrypto_GetProvisioningMethod());
}

TEST_F(OEMCryptoProv30Test, GetDeviceId) {
  OEMCryptoResult sts;
  std::vector<uint8_t> dev_id(128, 0);
  size_t dev_id_len = dev_id.size();
  sts = OEMCrypto_GetDeviceID(dev_id.data(), &dev_id_len);
  if (sts == OEMCrypto_ERROR_NOT_IMPLEMENTED) return;
  if (sts == OEMCrypto_ERROR_SHORT_BUFFER) {
    ASSERT_GT(dev_id_len, 0u);
    dev_id.resize(dev_id_len);
    sts = OEMCrypto_GetDeviceID(dev_id.data(), &dev_id_len);
  }
  cout << "             NormalGetDeviceId: dev_id = " << dev_id.data()
       << " len = " << dev_id_len << endl;
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
}


// The OEM certificate must be valid.
TEST_F(OEMCryptoProv30Test, CertValidAPI15) {
  ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_IsKeyboxOrOEMCertValid());
}

TEST_F(OEMCryptoProv30Test, OEMCertValid) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  bool kVerify = true;
  ASSERT_NO_FATAL_FAILURE(s.LoadOEMCert(kVerify));  // Load and verify.
}

// This verifies that an OEM Certificate can be used to generate a signature.
TEST_F(OEMCryptoProv30Test, OEMCertSignature) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.LoadOEMCert());
  OEMCryptoResult sts;
  // Sign a Message
  vector<uint8_t> data(500);
  GetRandBytes(data.data(), data.size());
  size_t signature_length = 0;
  vector<uint8_t> signature(1);

  sts = OEMCrypto_GenerateRSASignature(s.session_id(), data.data(), data.size(),
                                       signature.data(), &signature_length,
                                       kSign_RSASSA_PSS);

  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER, sts);
  ASSERT_NE(static_cast<size_t>(0), signature_length);
  signature.resize(signature_length, 0);

  sts = OEMCrypto_GenerateRSASignature(s.session_id(), data.data(), data.size(),
                                       signature.data(), &signature_length,
                                       kSign_RSASSA_PSS);

  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  ASSERT_NO_FATAL_FAILURE(s.VerifyRSASignature(
      data, signature.data(), signature_length, kSign_RSASSA_PSS));
}

// This verifies that the OEM Certificate cannot be used for other RSA padding
// schemes. Those schemes should only be used by cast receiver certificates.
TEST_F(OEMCryptoProv30Test, OEMCertForbiddenPaddingScheme) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.LoadOEMCert());
  OEMCryptoResult sts;
  // Sign a Message
  vector<uint8_t> data(500);
  GetRandBytes(data.data(), data.size());
  size_t signature_length = 0;
  // We need a size one vector to pass as a pointer.
  vector<uint8_t> signature(1, 0);
  vector<uint8_t> zero(1, 0);

  sts = OEMCrypto_GenerateRSASignature(s.session_id(), data.data(), data.size(),
                                       signature.data(), &signature_length,
                                       kSign_PKCS1_Block1);
  if (OEMCrypto_ERROR_SHORT_BUFFER == sts) {
    // The OEMCrypto could complain about buffer length first, so let's
    // resize and check if it's writing to the signature again.
    signature.resize(signature_length, 0);
    zero.resize(signature_length, 0);
    sts = OEMCrypto_GenerateRSASignature(s.session_id(), data.data(),
                                         data.size(), signature.data(),
                                         &signature_length, kSign_PKCS1_Block1);
  }
  EXPECT_NE(OEMCrypto_SUCCESS, sts)
      << "OEM Cert Signed with forbidden kSign_PKCS1_Block1.";
  ASSERT_EQ(zero, signature);  // signature should not be computed.
}

// Verify that the OEM Certificate can be used to sign a large buffer.
TEST_F(OEMCryptoProv30Test, OEMCertSignatureLargeBuffer) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.LoadOEMCert());
  OEMCryptoResult sts;
  // Sign a Message
  vector<uint8_t> data(kMaxMessageSize);
  GetRandBytes(data.data(), data.size());
  size_t signature_length = 0;
  vector<uint8_t> signature(1);

  sts = OEMCrypto_GenerateRSASignature(s.session_id(), data.data(), data.size(),
                                       signature.data(), &signature_length,
                                       kSign_RSASSA_PSS);

  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER, sts);
  ASSERT_NE(static_cast<size_t>(0), signature_length);
  signature.resize(signature_length);

  sts = OEMCrypto_GenerateRSASignature(s.session_id(), data.data(), data.size(),
                                       signature.data(), &signature_length,
                                       kSign_RSASSA_PSS);

  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  ASSERT_NO_FATAL_FAILURE(s.VerifyRSASignature(
      data, signature.data(), signature_length, kSign_RSASSA_PSS));
}

//
// AddKey Tests
//
// These tests will use either a test keybox or a test certificate to derive
// session keys.
class OEMCryptoSessionTests : public OEMCryptoClientTest {
 protected:
  OEMCryptoSessionTests() {}

  void SetUp() override {
    OEMCryptoClientTest::SetUp();
    EnsureTestKeys();
    if (global_features.usage_table) {
      CreateUsageTableHeader();
    }
  }

  void CreateUsageTableHeader(bool expect_success = true) {
    size_t header_buffer_length = 0;
    OEMCryptoResult sts =
        OEMCrypto_CreateUsageTableHeader(NULL, &header_buffer_length);
    if (expect_success) {
      ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER, sts);
    } else {
      ASSERT_NE(OEMCrypto_SUCCESS, sts);
      if (sts != OEMCrypto_ERROR_SHORT_BUFFER) return;
    }
    encrypted_usage_header_.resize(header_buffer_length);
    sts = OEMCrypto_CreateUsageTableHeader(encrypted_usage_header_.data(),
                                           &header_buffer_length);
    if (expect_success) {
      ASSERT_EQ(OEMCrypto_SUCCESS, sts);
    } else {
      ASSERT_NE(OEMCrypto_SUCCESS, sts);
    }
  }

  void TearDown() override {
    // If we installed a bad keybox, end with a good one installed.
    if (global_features.derive_key_method == DeviceFeatures::FORCE_TEST_KEYBOX)
      InstallKeybox(kTestKeybox, true);
    OEMCryptoClientTest::TearDown();
  }

  vector<uint8_t> encrypted_usage_header_;
};

class OEMCryptoSessionTestKeyboxTest : public OEMCryptoSessionTests {};

TEST_F(OEMCryptoSessionTestKeyboxTest, TestKeyboxIsValid) {
  ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_IsKeyboxValid());
}

TEST_F(OEMCryptoSessionTestKeyboxTest, GoodForceKeybox) {
  ASSERT_EQ(DeviceFeatures::FORCE_TEST_KEYBOX,
            global_features.derive_key_method)
      << "ForceKeybox tests will modify the installed keybox.";
  wvoec::WidevineKeybox keybox = kTestKeybox;
  OEMCryptoResult sts;
  InstallKeybox(keybox, true);
  sts = OEMCrypto_IsKeyboxValid();
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
}

TEST_F(OEMCryptoSessionTestKeyboxTest, BadCRCForceKeybox) {
  ASSERT_EQ(DeviceFeatures::FORCE_TEST_KEYBOX,
            global_features.derive_key_method)
      << "ForceKeybox tests will modify the installed keybox.";
  wvoec::WidevineKeybox keybox = kTestKeybox;
  keybox.crc_[1] ^= 42;
  OEMCryptoResult sts;
  InstallKeybox(keybox, false);
  sts = OEMCrypto_IsKeyboxValid();
  ASSERT_EQ(OEMCrypto_ERROR_BAD_CRC, sts);
}

TEST_F(OEMCryptoSessionTestKeyboxTest, BadMagicForceKeybox) {
  ASSERT_EQ(DeviceFeatures::FORCE_TEST_KEYBOX,
            global_features.derive_key_method)
      << "ForceKeybox tests will modify the installed keybox.";
  wvoec::WidevineKeybox keybox = kTestKeybox;
  keybox.magic_[1] ^= 42;
  OEMCryptoResult sts;
  InstallKeybox(keybox, false);
  sts = OEMCrypto_IsKeyboxValid();
  ASSERT_EQ(OEMCrypto_ERROR_BAD_MAGIC, sts);
}

TEST_F(OEMCryptoSessionTestKeyboxTest, BadDataForceKeybox) {
  ASSERT_EQ(DeviceFeatures::FORCE_TEST_KEYBOX,
            global_features.derive_key_method)
      << "ForceKeybox tests will modify the installed keybox.";
  wvoec::WidevineKeybox keybox = kTestKeybox;
  keybox.data_[1] ^= 42;
  OEMCryptoResult sts;
  InstallKeybox(keybox, false);
  sts = OEMCrypto_IsKeyboxValid();
  ASSERT_EQ(OEMCrypto_ERROR_BAD_CRC, sts);
}

// Verify that keys can be derived from the test keybox, and then those derived
// keys can be used to sign a message.
TEST_F(OEMCryptoSessionTestKeyboxTest, GenerateSignature) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.GenerateDerivedKeysFromKeybox(keybox_));

  // Dummy context for testing signature generation.
  vector<uint8_t> context = wvcdm::a2b_hex(
      "0a4c08001248000000020000101907d9ffde13aa95c122678053362136bdf840"
      "8f8276e4c2d87ec52b61aa1b9f646e58734930acebe899b3e464189a14a87202"
      "fb02574e70640bd22ef44b2d7e3912250a230a14080112100915007caa9b5931"
      "b76a3a85f046523e10011a09393837363534333231180120002a0c3138383637"
      "38373430350000");

  static const uint32_t SignatureBufferMaxLength = 256;
  vector<uint8_t> signature(SignatureBufferMaxLength);
  size_t signature_length = signature.size();

  OEMCryptoResult sts;
  sts = OEMCrypto_GenerateSignature(s.session_id(), context.data(),
                                    context.size(), signature.data(),
                                    &signature_length);
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);

  static const uint32_t SignatureExpectedLength = 32;
  ASSERT_EQ(SignatureExpectedLength, signature_length);
  signature.resize(signature_length);

  std::vector<uint8_t> expected_signature;
  s.ClientSignMessage(context, &expected_signature);
  ASSERT_EQ(expected_signature, signature);
}

// Verify that a license may be loaded without a nonce.
TEST_F(OEMCryptoSessionTests, LoadKeyNoNonce) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(kDuration, 0, 42));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
}

// Verify that a second license may be not be loaded in a session.
TEST_F(OEMCryptoSessionTests, LoadKeyNoNonceTwice) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(kDuration, 0, 42));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NE(
      OEMCrypto_SUCCESS,
      OEMCrypto_LoadKeys(s.session_id(), s.message_ptr(), s.message_size(),
                         s.signature().data(), s.signature().size(),
                         s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
                         s.num_keys(), s.key_array(), s.pst_substr(),
                         GetSubstring(), OEMCrypto_ContentLicense));
}

// Verify that a license may be loaded with a nonce.
TEST_F(OEMCryptoSessionTests, LoadKeyWithNonce) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(
      s.FillSimpleMessage(0, wvoec::kControlNonceEnabled, s.get_nonce()));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
}

// Verify that a second license may be not be loaded in a session.
TEST_F(OEMCryptoSessionTests, LoadKeyWithNonceTwice) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(
      s.FillSimpleMessage(0, wvoec::kControlNonceEnabled, s.get_nonce()));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
  ASSERT_NE(
      OEMCrypto_SUCCESS,
      OEMCrypto_LoadKeys(s.session_id(), s.message_ptr(), s.message_size(),
                         s.signature().data(), s.signature().size(),
                         s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
                         s.num_keys(), s.key_array(), s.pst_substr(),
                         GetSubstring(), OEMCrypto_ContentLicense));
}

// This asks for several nonce.  This simulates several license requests being
// lost.  OEMCrypto is required to keep up to four nonce in the nonce table.
TEST_F(OEMCryptoSessionTests, LoadKeySeveralNonce) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  uint32_t first_nonce =
      s.get_nonce();  // Nonce generated when installing keys.
  s.GenerateNonce();  // two.
  s.GenerateNonce();  // three.
  s.GenerateNonce();  // four.
  ASSERT_NO_FATAL_FAILURE(
      s.FillSimpleMessage(0, wvoec::kControlNonceEnabled, first_nonce));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
}

// A license might update the mac keys and it might not. This tests that
// OEMCrypto keeps the old mac keys if the license does not update them.
TEST_F(OEMCryptoSessionTests, LoadKeyWithNoMAC) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys("", false));

  vector<uint8_t> context = wvcdm::a2b_hex(
      "0a4c08001248000000020000101907d9ffde13aa95c122678053362136bdf840"
      "8f8276e4c2d87ec52b61aa1b9f646e58734930acebe899b3e464189a14a87202"
      "fb02574e70640bd22ef44b2d7e3912250a230a14080112100915007caa9b5931"
      "b76a3a85f046523e10011a09393837363534333231180120002a0c3138383637"
      "38373430350000");

  static const uint32_t SignatureBufferMaxLength = 256;
  vector<uint8_t> signature(SignatureBufferMaxLength);
  size_t signature_length = signature.size();

  OEMCryptoResult sts;
  sts = OEMCrypto_GenerateSignature(s.session_id(), context.data(),
                                    context.size(), signature.data(),
                                    &signature_length);

  ASSERT_EQ(OEMCrypto_SUCCESS, sts);

  static const uint32_t SignatureExpectedLength = 32;
  ASSERT_EQ(SignatureExpectedLength, signature_length);
  signature.resize(signature_length);

  std::vector<uint8_t> expected_signature;
  s.ClientSignMessage(context, &expected_signature);
  ASSERT_EQ(expected_signature, signature);
}

// This verifies that entitlement keys and entitled content keys can be loaded.
TEST_F(OEMCryptoSessionTests, LoadEntitlementKeysAPI14) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleEntitlementMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadEntitlementTestKeys());
  s.FillEntitledKeyArray();
  ASSERT_NO_FATAL_FAILURE(s.LoadEntitledContentKeys());
  s.FillEntitledKeyArray();
  ASSERT_NO_FATAL_FAILURE(s.LoadEntitledContentKeys());
}

// This verifies that entitled content keys cannot be loaded if we have not yet
// loaded the entitlement keys.
TEST_F(OEMCryptoSessionTests, LoadEntitlementKeysNoEntitlementKeysAPI14) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleEntitlementMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  // We do NOT call LoadEntitlementTestKeys.
  s.FillEntitledKeyArray();
  s.LoadEntitledContentKeys(OEMCrypto_ERROR_INVALID_CONTEXT);
}

// This verifies that entitled content keys cannot be loaded if we have loaded
// the wrong entitlement keys.
TEST_F(OEMCryptoSessionTests, LoadEntitlementKeysWrongEntitlementKeysAPI14) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleEntitlementMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadEntitlementTestKeys());
  s.FillEntitledKeyArray();
  const std::string key_id = "no_key";
  memcpy(const_cast<uint8_t*>(s.encrypted_entitled_message_ptr()) +
             s.entitled_key_array()[0].entitlement_key_id.offset,
         reinterpret_cast<const uint8_t*>(key_id.c_str()), key_id.length());
  s.entitled_key_array()[0].entitlement_key_id.length = key_id.length();
  s.LoadEntitledContentKeys(OEMCrypto_KEY_NOT_ENTITLED);
}

// This tests GenerateSignature with an 8k licnese request.
TEST_F(OEMCryptoSessionTests, ClientSignatureLargeBuffer) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys("", false));

  vector<uint8_t> context(kMaxMessageSize);
  for (size_t i = 0; i < kMaxMessageSize; i++) {
    context[i] = i % 0x100;
  }
  static const uint32_t SignatureBufferMaxLength = 256;
  vector<uint8_t> signature(SignatureBufferMaxLength);
  size_t signature_length = signature.size();

  OEMCryptoResult sts;
  sts = OEMCrypto_GenerateSignature(s.session_id(), context.data(),
                                    context.size(), signature.data(),
                                    &signature_length);
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);

  static const uint32_t SignatureExpectedLength = 32;
  ASSERT_EQ(SignatureExpectedLength, signature_length);
  signature.resize(signature_length);

  std::vector<uint8_t> expected_signature;
  s.ClientSignMessage(context, &expected_signature);
  ASSERT_EQ(expected_signature, signature);
}

// This tests LoadKeys with an 8k license response.
TEST_F(OEMCryptoSessionTests, LoadKeyLargeBuffer) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  s.set_message_size(kMaxMessageSize);
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
}

// Returns a string containing two times the original message in continuous
// memory. Used as part of the BadRange tests.
std::string DuplicateMessage(MessageData& message) {
  std::string single_message = wvcdm::BytesToString(
      reinterpret_cast<const uint8_t*>(&message), sizeof(message));
  std::string double_message = single_message + single_message;
  return double_message;
}

// The Bad Range tests verify that OEMCrypto_LoadKeys checks the range
// of all the pointers.  It should reject a message if the pointer does
// not point into the message buffer.
TEST_F(OEMCryptoSessionTests, LoadKeyWithBadRange1) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  std::string double_message = DuplicateMessage(s.encrypted_license());
  OEMCrypto_Substring wrong_mac_keys = s.enc_mac_keys_substr();
  wrong_mac_keys.offset += s.message_size();

  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), reinterpret_cast<const uint8_t*>(double_message.data()),
      s.message_size(), s.signature().data(), s.signature().size(),
      s.enc_mac_keys_iv_substr(),
      wrong_mac_keys,  // Not within range of one message.
      s.num_keys(), s.key_array(), GetSubstring(), GetSubstring(),
      OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// The Bad Range tests verify that OEMCrypto_LoadKeys checks the range
// of all the pointers.  It should reject a message if the pointer does
// not point into the message buffer.
TEST_F(OEMCryptoSessionTests, LoadKeyWithBadRange2) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  std::string double_message = DuplicateMessage(s.encrypted_license());
  OEMCrypto_Substring wrong_mac_keys_iv = s.enc_mac_keys_iv_substr();
  wrong_mac_keys_iv.offset += s.message_size();

  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), reinterpret_cast<const uint8_t*>(double_message.data()),
      s.message_size(), s.signature().data(), s.signature().size(),
      wrong_mac_keys_iv,  // bad.
      s.enc_mac_keys_substr(), s.num_keys(), s.key_array(), GetSubstring(),
      GetSubstring(), OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// The Bad Range tests verify that OEMCrypto_LoadKeys checks the range
// of all the pointers.  It should reject a message if the pointer does
// not point into the message buffer.
TEST_F(OEMCryptoSessionTests, LoadKeyWithBadRange3) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  std::string double_message = DuplicateMessage(s.encrypted_license());
  s.key_array()[0].key_id.offset += s.message_size();

  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), reinterpret_cast<const uint8_t*>(double_message.data()),
      s.message_size(), s.signature().data(), s.signature().size(),
      s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(), s.num_keys(),
      s.key_array(), GetSubstring(), GetSubstring(), OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// The Bad Range tests verify that OEMCrypto_LoadKeys checks the range
// of all the pointers.  It should reject a message if the pointer does
// not point into the message buffer.
TEST_F(OEMCryptoSessionTests, LoadKeyWithBadRange4) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());

  std::string double_message = DuplicateMessage(s.encrypted_license());
  s.key_array()[1].key_data.offset += s.message_size();

  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), reinterpret_cast<const uint8_t*>(double_message.data()),
      s.message_size(), s.signature().data(), s.signature().size(),
      s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(), s.num_keys(),
      s.key_array(), GetSubstring(), GetSubstring(), OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// The Bad Range tests verify that OEMCrypto_LoadKeys checks the range
// of all the pointers.  It should reject a message if the pointer does
// not point into the message buffer.
TEST_F(OEMCryptoSessionTests, LoadKeyWithBadRange5) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  std::string double_message = DuplicateMessage(s.encrypted_license());
  s.key_array()[1].key_data_iv.offset += s.message_size();
  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), reinterpret_cast<const uint8_t*>(double_message.data()),
      s.message_size(), s.signature().data(), s.signature().size(),
      s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(), s.num_keys(),
      s.key_array(), GetSubstring(), GetSubstring(), OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// The Bad Range tests verify that OEMCrypto_LoadKeys checks the range
// of all the pointers.  It should reject a message if the pointer does
// not point into the message buffer.
TEST_F(OEMCryptoSessionTests, LoadKeyWithBadRange6) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());

  std::string double_message = DuplicateMessage(s.encrypted_license());
  s.key_array()[2].key_control.offset += s.message_size();

  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), reinterpret_cast<const uint8_t*>(double_message.data()),
      s.message_size(), s.signature().data(), s.signature().size(),
      s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(), s.num_keys(),
      s.key_array(), GetSubstring(), GetSubstring(), OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// The Bad Range tests verify that OEMCrypto_LoadKeys checks the range
// of all the pointers.  It should reject a message if the pointer does
// not point into the message buffer.
TEST_F(OEMCryptoSessionTests, LoadKeyWithBadRange7) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  std::string double_message = DuplicateMessage(s.encrypted_license());
  s.key_array()[2].key_control_iv.offset += s.message_size();

  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), reinterpret_cast<const uint8_t*>(double_message.data()),
      s.message_size(), s.signature().data(), s.signature().size(),
      s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(), s.num_keys(),
      s.key_array(), GetSubstring(), GetSubstring(), OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// The IV should not be identical to the data right before the encrypted mac
// keys.
// This test is for OEMCrypto v15.2.  It is being disabled on the Android branch
// the 15.2 updates to 15.2 were not available in time for the Q release. SOC
// vendors who are able to pass this tests, should.
TEST_F(OEMCryptoSessionTests, DISABLED_LoadKeyWithSuspiciousIV) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  // This is suspicious: the data right before the mac keys is identical to the
  // iv.
  memcpy(s.license().padding, s.license().mac_key_iv,
         sizeof(s.license().padding));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());

  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), s.message_ptr(), s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), GetSubstring(), GetSubstring(),
      OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// Test that LoadKeys fails when a key is loaded with no key control block.
TEST_F(OEMCryptoSessionTests, LoadKeyWithNullKeyControl) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  s.key_array()[2].key_control.offset = 0;
  s.key_array()[2].key_control.length = 0;

  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), s.message_ptr(), s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), GetSubstring(), GetSubstring(),
      OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// Test that LoadKeys fails when the key control block encryption has a null IV.
TEST_F(OEMCryptoSessionTests, LoadKeyWithNullKeyControlIv) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  s.key_array()[2].key_control_iv.offset = 0;
  s.key_array()[2].key_control_iv.length = 0;

  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), s.message_ptr(), s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), GetSubstring(), GetSubstring(),
      OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// Verify that LoadKeys fails when a key's nonce is not in the table.
TEST_F(OEMCryptoSessionTests, LoadKeyWithBadNonce) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, wvoec::kControlNonceEnabled,
                                              42));  // bad nonce.
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), s.message_ptr(), s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), GetSubstring(), GetSubstring(),
      OEMCrypto_ContentLicense);

  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// Verify that LoadKeys fails when an attempt is made to use a nonce twice.
TEST_F(OEMCryptoSessionTests, LoadKeyWithRepeatNonce) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  uint32_t nonce = s.get_nonce();
  ASSERT_NO_FATAL_FAILURE(
      s.FillSimpleMessage(0, wvoec::kControlNonceEnabled, nonce));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  // This is the first attempt.  It should succeed.
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
  ASSERT_NO_FATAL_FAILURE(s.close());

  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, wvoec::kControlNonceEnabled,
                                              nonce));  // same old nonce.
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  // This is the second attempt to load the keys with a repeated nonce.  It
  // should fail.
  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), s.message_ptr(), s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), GetSubstring(), GetSubstring(),
      OEMCrypto_ContentLicense);

  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// This tests that a nonce cannot be used in new session.  This is similar to
// the previous test, but does not use the nonce in the first session. The nonce
// table should be tied to a session, so generating a nonce in the first session
// and then using it in the second session should fail.
TEST_F(OEMCryptoSessionTests, LoadKeyNonceReopenSession) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  uint32_t nonce = s.get_nonce();
  // Do not use the nonce now.  Close session and use it after re-opening.
  ASSERT_NO_FATAL_FAILURE(s.close());

  // Actually, this isn't the same session.  OEMCrypto opens a new session, but
  // we are guarding against the possiblity that it re-uses the session data
  // and might not clear out the nonce table correctly.
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, wvoec::kControlNonceEnabled,
                                              nonce));  // same old nonce
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), s.message_ptr(), s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), GetSubstring(), GetSubstring(),
      OEMCrypto_ContentLicense);

  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// This tests that a nonce cannot be used in wrong session.  This is similar to
// the previous test, except we do not close session 1 before we open session 2.
TEST_F(OEMCryptoSessionTests, LoadKeyNonceWrongSession) {
  Session s1;
  ASSERT_NO_FATAL_FAILURE(s1.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s1));
  uint32_t nonce = s1.get_nonce();
  // Do not use the nonce.  Also, leave the session open.  We want to make sure
  // that s and s1 do NOT share a nonce table.  This is different from the
  // LoadKeyNonceReopenSession in that we do not close s1.

  Session s2;
  ASSERT_NO_FATAL_FAILURE(s2.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s2));
  ASSERT_NO_FATAL_FAILURE(s2.FillSimpleMessage(0, wvoec::kControlNonceEnabled,
                                              nonce));  // nonce from session s1
  ASSERT_NO_FATAL_FAILURE(s2.EncryptAndSign());
  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s2.session_id(), s2.message_ptr(), s2.message_size(),
      s2.signature().data(), s2.signature().size(), s2.enc_mac_keys_iv_substr(),
      s2.enc_mac_keys_substr(), s2.num_keys(), s2.key_array(), GetSubstring(),
      GetSubstring(), OEMCrypto_ContentLicense);

  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// LoadKeys should fail if the key control block as a bad verification string.
TEST_F(OEMCryptoSessionTests, LoadKeyWithBadVerification) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  s.license().keys[1].control.verification[2] = 'Z';
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), s.message_ptr(), s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), GetSubstring(), GetSubstring(),
      OEMCrypto_ContentLicense);

  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// This test verifies that LoadKeys still works when the message is not aligned
// in memory on a word (2 or 4 byte) boundary.
TEST_F(OEMCryptoSessionTests, LoadKeyUnalignedMessage) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      kDuration, wvoec::kControlNonceEnabled, s.get_nonce()));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  std::vector<uint8_t> buffer(1, '0');  // A string of 1 byte long.
  size_t offset = buffer.size();
  ASSERT_EQ(1u, offset);
  // We assume that vectors are allocated on as a small chunk of data that is
  // aligned on a word boundary.  I.e. we assume buffer is word aligned. Next,
  // we append the message to buffer after the single padding byte.
  buffer.insert(buffer.end(), s.message_ptr(),
                s.message_ptr() + s.message_size());
  // Thus, buffer[offset] is NOT word aligned.
  const uint8_t* unaligned_message = &buffer[offset];
  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), unaligned_message, s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), GetSubstring(), GetSubstring(),
      OEMCrypto_ContentLicense);
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
}

// This tests each key control block verification string in the range kc09-kc1?.
// This test is parameterized by the API number in the key control lock.
class SessionTestAlternateVerification : public OEMCryptoSessionTests,
                                         public WithParamInterface<int> {
 public:
  void SetUp() override {
    OEMCryptoSessionTests::SetUp();
    target_api_ = static_cast<uint32_t>(GetParam());
  }

 protected:
  uint32_t target_api_;
};

TEST_P(SessionTestAlternateVerification, LoadKeys) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  char buffer[5] = "kctl";  // This is the default verification string, required
                            // for all API versions.
  if (target_api_ > 8 && target_api_ < 100) {
    snprintf(buffer, 5, "kc%02d", target_api_);
  }
  for (unsigned int i = 0; i < s.num_keys(); i++) {
    memcpy(s.license().keys[i].control.verification, buffer, 4);
  }
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), s.message_ptr(), s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), GetSubstring(), GetSubstring(),
      OEMCrypto_ContentLicense);
  // If this is a future API, then LoadKeys should fail.
  if (global_features.api_version < target_api_) {
    ASSERT_NE(OEMCrypto_SUCCESS, sts);
  } else {
    // Otherwise, LoadKeys should succeed.
    ASSERT_EQ(OEMCrypto_SUCCESS, sts)
        << "LoadKeys failed for key control block kc" << target_api_;
  }
}

// Range of API versions to test.  This should start at 8, and go to
// the current API + 2.  We use +2 because we want to test at least 1
// future API, and the ::testing::Range is not inclusive.
INSTANTIATE_TEST_CASE_P(TestAll, SessionTestAlternateVerification,
                        Range(8, 15 + 2));

TEST_F(OEMCryptoSessionTests, LoadKeysBadSignature) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  s.signature()[0] ^= 42;  // Bad signature.
  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), s.message_ptr(), s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), GetSubstring(), GetSubstring(),
      OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// We should not be able to load keys if we haven't derived the mac keys.
TEST_F(OEMCryptoSessionTests, LoadKeysWithNoDerivedKeys) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  // don't do this: InstallTestSessionKeys(&s).
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), s.message_ptr(), s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), GetSubstring(), GetSubstring(),
      OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// LoadKeys should fail if we try to load keys with no keys.
TEST_F(OEMCryptoSessionTests, LoadKeyNoKeys) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(kDuration, 0, 42));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  int kNoKeys = 0;
  ASSERT_NE(OEMCrypto_SUCCESS,
            OEMCrypto_LoadKeys(
                s.session_id(), s.message_ptr(), s.message_size(),
                s.signature().data(), s.signature().size(), GetSubstring(),
                GetSubstring(), kNoKeys, s.key_array(), GetSubstring(),
                GetSubstring(), OEMCrypto_ContentLicense));
}

// Like the previous test, except we ask for a nonce first.
TEST_F(OEMCryptoSessionTests, LoadKeyNoKeyWithNonce) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(
      s.FillSimpleMessage(0, wvoec::kControlNonceEnabled, s.get_nonce()));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  int kNoKeys = 0;
  ASSERT_NE(OEMCrypto_SUCCESS,
            OEMCrypto_LoadKeys(
                s.session_id(), s.message_ptr(), s.message_size(),
                s.signature().data(), s.signature().size(), GetSubstring(),
                GetSubstring(), kNoKeys, s.key_array(), GetSubstring(),
                GetSubstring(), OEMCrypto_ContentLicense));
}

// SelectKey should fail if we attempt to select a key that has not been loaded.
// Also, the error should be NO_CONTENT_KEY.
TEST_F(OEMCryptoSessionTests, SelectKeyNotThereAPI15) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(
      s.FillSimpleMessage(0, wvoec::kControlNonceEnabled, s.get_nonce()));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
  const char* key_id = "no_key";
  OEMCryptoResult sts = OEMCrypto_SelectKey(
      s.session_id(), reinterpret_cast<const uint8_t*>(key_id), strlen(key_id),
      OEMCrypto_CipherMode_CTR);
  if (sts != OEMCrypto_SUCCESS) {
    EXPECT_EQ(OEMCrypto_ERROR_NO_CONTENT_KEY, sts);
  } else {
    // Delayed error code.  If select key was a success, then we should
    // eventually see the error when we decrypt.
    vector<uint8_t> in_buffer(256);
    for (size_t i = 0; i < in_buffer.size(); i++) in_buffer[i] = i % 256;
    vector<uint8_t> encryptionIv(AES_BLOCK_SIZE);
    EXPECT_EQ(1, GetRandBytes(encryptionIv.data(), AES_BLOCK_SIZE));
    // Describe the output
    vector<uint8_t> out_buffer(in_buffer.size());
    const bool is_encrypted = true;
    OEMCrypto_DestBufferDesc destBuffer;
    destBuffer.type = OEMCrypto_BufferType_Clear;
    destBuffer.buffer.clear.address = out_buffer.data();
    destBuffer.buffer.clear.max_length = out_buffer.size();
    OEMCrypto_CENCEncryptPatternDesc pattern;
    pattern.encrypt = 0;
    pattern.skip = 0;
    pattern.offset = 0;
    // Decrypt the data
    sts = OEMCrypto_DecryptCENC(
        s.session_id(), in_buffer.data(), in_buffer.size(), is_encrypted,
        encryptionIv.data(), 0, &destBuffer, &pattern,
        OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample);
    EXPECT_TRUE(
        (OEMCrypto_ERROR_NO_CONTENT_KEY == sts)       // Preferred return code.
        || (OEMCrypto_KEY_NOT_LOADED == sts));        // Obsolete return code.
  }
}

// After loading keys, we should be able to query the key control block.  If we
// attempt to query a key that has not been loaded, the error should be
// NO_CONTENT_KEY.
TEST_F(OEMCryptoSessionTests, QueryKeyControl) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(
      s.FillSimpleMessage(0, wvoec::kControlNonceEnabled, s.get_nonce()));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
  // Note: successful cases are tested in VerifyTestKeys.
  KeyControlBlock block;
  size_t size = sizeof(block) - 1;
  OEMCryptoResult sts =
      OEMCrypto_QueryKeyControl(s.session_id(), s.license().keys[0].key_id,
                                s.license().keys[0].key_id_length,
                                reinterpret_cast<uint8_t*>(&block), &size);
  if (sts == OEMCrypto_ERROR_NOT_IMPLEMENTED) {
    return;
  }
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER, sts);
  const char* key_id = "no_key";
  size = sizeof(block);
  ASSERT_EQ(OEMCrypto_ERROR_NO_CONTENT_KEY,
            OEMCrypto_QueryKeyControl(
                s.session_id(), reinterpret_cast<const uint8_t*>(key_id),
                strlen(key_id), reinterpret_cast<uint8_t*>(&block), &size));
}

// If the device says it supports anti-rollback in the hardware, then it should
// accept a key control block the anti-rollback hardware bit set.  Otherwise, it
// should reject that key control block.
TEST_F(OEMCryptoSessionTests, AntiRollbackHardwareRequired) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      0, wvoec::kControlRequireAntiRollbackHardware, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), s.message_ptr(), s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), GetSubstring(), GetSubstring(),
      OEMCrypto_ContentLicense);
  if (OEMCrypto_IsAntiRollbackHwPresent()) {
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  } else {
    ASSERT_EQ(OEMCrypto_ERROR_UNKNOWN_FAILURE, sts);
  }
}

// This test verifies that the minimum patch level can be required.  The device
// should accept a key control block with the current patch level, and it should
// reject any key control blocks with a future patch level.
TEST_F(OEMCryptoSessionTests, CheckMinimumPatchLevel) {
  uint8_t patch_level = OEMCrypto_Security_Patch_Level();
  printf("             Current Patch Level: %u.\n", patch_level);
  {
    Session s;
    ASSERT_NO_FATAL_FAILURE(s.open());
    ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
    ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
        0, patch_level << wvoec::kControlSecurityPatchLevelShift, 0));
    ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
    ASSERT_EQ(
        OEMCrypto_SUCCESS,
        OEMCrypto_LoadKeys(s.session_id(), s.message_ptr(), s.message_size(),
                           s.signature().data(), s.signature().size(),
                           s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
                           s.num_keys(), s.key_array(), GetSubstring(),
                           GetSubstring(), OEMCrypto_ContentLicense));
  }
  // Reject any future patch levels.
  if (patch_level < 0x3F) {
    Session s;
    ASSERT_NO_FATAL_FAILURE(s.open());
    ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
    ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
        0, (patch_level + 1) << wvoec::kControlSecurityPatchLevelShift, 0));
    ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
    ASSERT_EQ(
        OEMCrypto_ERROR_UNKNOWN_FAILURE,
        OEMCrypto_LoadKeys(s.session_id(), s.message_ptr(), s.message_size(),
                           s.signature().data(), s.signature().size(),
                           s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
                           s.num_keys(), s.key_array(), GetSubstring(),
                           GetSubstring(), OEMCrypto_ContentLicense));
  }
  // Accept an old patch level.
  if (patch_level > 0) {
    Session s;
    ASSERT_NO_FATAL_FAILURE(s.open());
    ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
    ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
        0, (patch_level - 1) << wvoec::kControlSecurityPatchLevelShift, 0));
    ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
    ASSERT_EQ(
        OEMCrypto_SUCCESS,
        OEMCrypto_LoadKeys(s.session_id(), s.message_ptr(), s.message_size(),
                           s.signature().data(), s.signature().size(),
                           s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
                           s.num_keys(), s.key_array(), GetSubstring(),
                           GetSubstring(), OEMCrypto_ContentLicense));
  }
}

// This test verifies that OEMCrypto can load the number of keys required for
// the reported resource level.
TEST_F(OEMCryptoSessionTests, MinimumKeysAPI12) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  size_t num_keys = GetResourceValue(kMaxKeysPerSession);
  ASSERT_LE(num_keys, kMaxNumKeys) << "Test constants need updating.";
  s.set_num_keys(num_keys);
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
  for (size_t key_index = 0; key_index < num_keys; key_index++) {
    bool kSelectKeyFirst = true;
    ASSERT_NO_FATAL_FAILURE(
        s.TestDecryptCTR(kSelectKeyFirst, OEMCrypto_SUCCESS, key_index));
  }
}

// Used to test the different HDCP versions.  This test is parameterized by the
// required HDCP version in the key control block.
class SessionTestDecryptWithHDCP : public OEMCryptoSessionTests,
                                   public WithParamInterface<int> {
 public:
  void DecryptWithHDCP(OEMCrypto_HDCP_Capability version) {
    OEMCryptoResult sts;
    OEMCrypto_HDCP_Capability current, maximum;
    sts = OEMCrypto_GetHDCPCapability(&current, &maximum);
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);
    Session s;
    ASSERT_NO_FATAL_FAILURE(s.open());
    ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
    ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
        0,
        (version << wvoec::kControlHDCPVersionShift) |
            wvoec::kControlObserveHDCP | wvoec::kControlHDCPRequired,
        0));
    ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
    ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());

    if (version > current) {
      ASSERT_NO_FATAL_FAILURE(
          s.TestDecryptCTR(true, OEMCrypto_ERROR_INSUFFICIENT_HDCP));
    } else {
      ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR(true, OEMCrypto_SUCCESS));
    }
  }
};

TEST_P(SessionTestDecryptWithHDCP, DecryptAPI09) {
  // Test parameterized by HDCP version.
  DecryptWithHDCP(static_cast<OEMCrypto_HDCP_Capability>(GetParam()));
}
INSTANTIATE_TEST_CASE_P(TestHDCP, SessionTestDecryptWithHDCP, Range(1, 6));

//
// Load, Refresh Keys Test
//
// This test is parameterized by two parameters:
// 1. A boolean that determines if the license sets a new pair of mac keys in
// the license.
// 2. The number of keys refreshed in the refresh method.  If the number of keys
// is zero, then all of the keys should be refreshed.
class SessionTestRefreshKeyTest
    : public OEMCryptoSessionTests,
      public WithParamInterface<std::pair<bool, int> > {
 public:
  void SetUp() override {
    OEMCryptoSessionTests::SetUp();
    new_mac_keys_ =
        GetParam().first;  // Whether to put new mac keys in LoadKeys.
    num_keys_ = static_cast<size_t>(GetParam().second);  // # keys in refresh.
  }

 protected:
  bool new_mac_keys_;
  size_t num_keys_;  // Number of keys to refresh.
};

// Refresh keys should work if the license does not use a nonce.
TEST_P(SessionTestRefreshKeyTest, RefreshWithNonce) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      kDuration, wvoec::kControlNonceEnabled, s.get_nonce()));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys("", new_mac_keys_));
  s.GenerateNonce();
  // License renewal message is signed by client and verified by the server.
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature());
  ASSERT_NO_FATAL_FAILURE(s.RefreshTestKeys(num_keys_,
                                            wvoec::kControlNonceEnabled,
                                            s.get_nonce(), OEMCrypto_SUCCESS));
}

// Refresh keys should work if the license does use a nonce.
TEST_P(SessionTestRefreshKeyTest, RefreshNoNonce) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(kDuration, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys("", new_mac_keys_));
  // License renewal message is signed by client and verified by the server.
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature());
  ASSERT_NO_FATAL_FAILURE(
      s.RefreshTestKeys(num_keys_, 0, 0, OEMCrypto_SUCCESS));
}

// Refresh keys should fail if the nonce has already been used.
TEST_P(SessionTestRefreshKeyTest, RefreshOldNonceAPI11) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  uint32_t nonce = s.get_nonce();
  ASSERT_NO_FATAL_FAILURE(
      s.FillSimpleMessage(kDuration, wvoec::kControlNonceEnabled, nonce));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys("", new_mac_keys_));
  // License renewal message is signed by client and verified by the server.
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature());
  // Tryinng to reuse the same nonce.
  ASSERT_NO_FATAL_FAILURE(
      s.RefreshTestKeys(num_keys_, wvoec::kControlNonceEnabled, nonce,
                        OEMCrypto_ERROR_INVALID_NONCE));
}

// Refresh keys should fail if the nonce is not in the table.
TEST_P(SessionTestRefreshKeyTest, RefreshBadNonceAPI11) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      kDuration, wvoec::kControlNonceEnabled, s.get_nonce()));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys("", new_mac_keys_));
  s.GenerateNonce();
  // License renewal message is signed by client and verified by the server.
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature());
  uint32_t nonce = s.get_nonce() ^ 42;
  ASSERT_NO_FATAL_FAILURE(
      s.RefreshTestKeys(num_keys_, wvoec::kControlNonceEnabled, nonce,
                        OEMCrypto_ERROR_INVALID_NONCE));
}

// Refresh keys should handle the maximum message size.
TEST_P(SessionTestRefreshKeyTest, RefreshLargeBuffer) {
  Session s;
  s.set_message_size(kMaxMessageSize);
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      kDuration, wvoec::kControlNonceEnabled, s.get_nonce()));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys("", new_mac_keys_));
  s.GenerateNonce();
  // License renewal message is signed by client and verified by the server.
  // This uses a large buffer for the renewal message.
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature(kMaxMessageSize));
  ASSERT_NO_FATAL_FAILURE(s.RefreshTestKeys(num_keys_,
                                            wvoec::kControlNonceEnabled,
                                            s.get_nonce(), OEMCrypto_SUCCESS));
}

// This situation would occur if an app only uses one key in the license.  When
// that happens, SelectKey would be called before the first decrypt, and then
// would not need to be called again, even if the license is refreshed.
TEST_P(SessionTestRefreshKeyTest, RefreshWithNoSelectKey) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      kDuration, wvoec::kControlNonceEnabled, s.get_nonce()));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys("", new_mac_keys_));
  // Call select key before the refresh.  No calls below to TestDecryptCTR with
  // select key set to true.
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR(true));

  s.GenerateNonce();
  // License renewal message is signed by client and verified by the server.
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature());
  // Note: we store the message in encrypted_license_, but the refresh key
  // message is not actually encrypted.  It is, however, signed.
  // FillRefreshMessage fills the message with a duration of kLongDuration.
  ASSERT_NO_FATAL_FAILURE(s.FillRefreshMessage(
      num_keys_, wvoec::kControlNonceEnabled, s.get_nonce()));
  s.ServerSignBuffer(reinterpret_cast<const uint8_t*>(&s.encrypted_license()),
                     s.message_size(), &s.signature());
  std::vector<OEMCrypto_KeyRefreshObject> key_array(num_keys_);
  s.FillRefreshArray(key_array.data(), num_keys_);
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_RefreshKeys(s.session_id(), s.message_ptr(),
                                  s.message_size(), s.signature().data(),
                                  s.signature().size(), num_keys_,
                                  key_array.data()));
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR(false));
  //  This should still be valid key, even if the refresh failed, because this
  //  is before the original license duration.
  sleep(kShortSleep);
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR(false));
  // This should be after duration of the original license, but before the
  // expiration of the refresh message.  This should succeed if and only if the
  // refresh succeeded.
  sleep(kShortSleep + kLongSleep);
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR(false));
}

// If only one key control block in the refesh, we update all the keys.
INSTANTIATE_TEST_CASE_P(TestRefreshAllKeys, SessionTestRefreshKeyTest,
                        Values(std::make_pair(true, 1),
                               std::make_pair(false, 1)));

// If multiple key control blocks, we update each key separately.
INSTANTIATE_TEST_CASE_P(TestRefreshEachKeys, SessionTestRefreshKeyTest,
                        Values(std::make_pair(true, 4),
                               std::make_pair(false, 4)));

// If the license does not allow a hash, then we should not compute one.
TEST_F(OEMCryptoSessionTests, HashForbiddenAPI15) {
  uint32_t hash_type = OEMCrypto_SupportsDecryptHash();
  // If hash is not supported, or is vendor defined, don't try to test it.
  if (hash_type != OEMCrypto_CRC_Clear_Buffer) return;
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(kDuration, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
  uint32_t frame_number = 1;
  uint32_t hash = 42;
  // It is OK to set the hash before loading the keys
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_SetDecryptHash(s.session_id(), frame_number,
                                     reinterpret_cast<const uint8_t*>(&hash),
                                     sizeof(hash)));
  // It is OK to select the key and decrypt.
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
  // But the error code should be bad.
  ASSERT_EQ(OEMCrypto_ERROR_UNKNOWN_FAILURE,
            OEMCrypto_GetHashErrorCode(s.session_id(), &frame_number));
}

//
// Decrypt Tests -- these test Decrypt CTR mode only.
//
TEST_F(OEMCryptoSessionTests, Decrypt) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(kDuration, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
}

// Verify that a zero duration means infinite license duration.
TEST_F(OEMCryptoSessionTests, DecryptZeroDuration) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
}

// Verify that several sessions may each load a license and then each may
// decrypt.
TEST_F(OEMCryptoSessionTests, SimultaneousDecrypt) {
  vector<Session> s(8);
  for (int i = 0; i < 8; i++) {
    ASSERT_NO_FATAL_FAILURE(s[i].open());
    ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s[i]));
  }
  for (int i = 0; i < 8; i++) {
    ASSERT_NO_FATAL_FAILURE(
        s[i].FillSimpleMessage(kLongDuration, 0, s[i].get_nonce()));
    ASSERT_NO_FATAL_FAILURE(s[i].EncryptAndSign());
  }
  for (int i = 0; i < 8; i++) {
    ASSERT_NO_FATAL_FAILURE(s[i].LoadTestKeys());
  }
  for (int i = 0; i < 8; i++) {
    ASSERT_NO_FATAL_FAILURE(s[i].TestDecryptCTR());
  }
  // Second call to decrypt for each session.
  for (int i = 0; i < 8; i++) {
    ASSERT_NO_FATAL_FAILURE(s[i].TestDecryptCTR());
  }
}

// This test generates several test keys, as if a license request was lost.
// This is only valid for (obsolete) devices that use a keybox to talk to a
// license server.
TEST_F(OEMCryptoSessionTests, SimultaneousDecryptWithLostMessageKeyboxTest) {
  vector<Session> s(8);
  for (int i = 0; i < 8; i++) {
    ASSERT_NO_FATAL_FAILURE(s[i].open());
    ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s[i]));
  }
  for (int i = 0; i < 8; i++) {
    ASSERT_NO_FATAL_FAILURE(s[i].GenerateDerivedKeysFromKeybox(keybox_));
    ASSERT_NO_FATAL_FAILURE(
        s[i].FillSimpleMessage(kLongDuration, 0, s[i].get_nonce()));
    ASSERT_NO_FATAL_FAILURE(s[i].EncryptAndSign());
  }
  // First set of messages are lost.  Generate second set.
  for (int i = 0; i < 8; i++) {
    ASSERT_NO_FATAL_FAILURE(s[i].GenerateDerivedKeysFromKeybox(keybox_));
    ASSERT_NO_FATAL_FAILURE(
        s[i].FillSimpleMessage(kLongDuration, 0, s[i].get_nonce()));
    ASSERT_NO_FATAL_FAILURE(s[i].EncryptAndSign());
  }
  for (int i = 0; i < 8; i++) {
    ASSERT_NO_FATAL_FAILURE(s[i].LoadTestKeys());
  }
  for (int i = 0; i < 8; i++) {
    ASSERT_NO_FATAL_FAILURE(s[i].TestDecryptCTR());
  }
  // Second call to decrypt for each session.
  for (int i = 0; i < 8; i++) {
    ASSERT_NO_FATAL_FAILURE(s[i].TestDecryptCTR());
  }
}

struct SampleSize {
  size_t clear_size;
  size_t encrypted_size;
  SampleSize(size_t clear, size_t encrypted)
      : clear_size(clear), encrypted_size(encrypted) {}
};

struct SampleInitData {
  uint8_t iv[AES_BLOCK_SIZE];
  size_t block_offset;
};

// A class of tests that test decryption for a variety of patterns and modes.
// This test is parameterized by three parameters:
// 1. The pattern used for pattern decryption.
// 2. The cipher mode for decryption: either CTR or CBC.
// 3. A boolean that determines if decrypt in place should be done.  When the
//    output buffer is clear, it should be possible for the input and output
//    buffers to be the same.
class OEMCryptoSessionTestsDecryptTests
    : public OEMCryptoSessionTests,
      public WithParamInterface<tuple<OEMCrypto_CENCEncryptPatternDesc,
                                      OEMCryptoCipherMode, bool> > {
 protected:
  void SetUp() override {
    OEMCryptoSessionTests::SetUp();
    pattern_ = ::testing::get<0>(GetParam());
    cipher_mode_ = ::testing::get<1>(GetParam());
    decrypt_inplace_ = ::testing::get<2>(GetParam());
  }

  void FindTotalSize() {
    total_size_ = 0;
    for (size_t i = 0; i < subsample_size_.size(); i++) {
      total_size_ +=
          subsample_size_[i].clear_size + subsample_size_[i].encrypted_size;
    }
  }

  void EncryptData(const vector<uint8_t>& key,
                   const vector<uint8_t>& starting_iv,
                   const vector<uint8_t>& in_buffer,
                   vector<uint8_t>* out_buffer) {
    AES_KEY aes_key;
    AES_set_encrypt_key(key.data(), AES_BLOCK_SIZE * 8, &aes_key);
    out_buffer->resize(in_buffer.size());

    uint8_t iv[AES_BLOCK_SIZE];  // Current iv.

    memcpy(iv, starting_iv.data(), AES_BLOCK_SIZE);

    size_t buffer_index = 0;  // byte index into in and out.
    size_t block_offset = 0;  // byte index into current block.
    for (size_t i = 0; i < subsample_size_.size(); i++) {
      // Copy clear content.
      if (subsample_size_[i].clear_size > 0) {
        memcpy(&(*out_buffer)[buffer_index], &in_buffer[buffer_index],
               subsample_size_[i].clear_size);
        buffer_index += subsample_size_[i].clear_size;
      }
      // Save the current iv and offsets for call to DecryptCENC.
      sample_init_data_.push_back(SampleInitData());
      memcpy(sample_init_data_[i].iv, iv, AES_BLOCK_SIZE);
      // Note: final CENC spec specifies the pattern_offset = 0 at the
      // start of each subsample.
      size_t pattern_offset = 0;
      sample_init_data_[i].block_offset = block_offset;

      size_t subsample_end = buffer_index + subsample_size_[i].encrypted_size;
      while (buffer_index < subsample_end) {
        size_t size =
            min(subsample_end - buffer_index, AES_BLOCK_SIZE - block_offset);
        size_t pattern_length = pattern_.encrypt + pattern_.skip;
        bool skip_block =
            (pattern_offset >= pattern_.encrypt) && (pattern_length > 0);
        if (pattern_length > 0) {
          pattern_offset = (pattern_offset + 1) % pattern_length;
        }
        // CBC mode should just copy a partial block at the end.  If there
        // is a partial block at the beginning, an error is returned, so we
        // can put whatever we want in the output buffer.
        if (skip_block || ((cipher_mode_ == OEMCrypto_CipherMode_CBC) &&
                           (size < AES_BLOCK_SIZE))) {
          memcpy(&(*out_buffer)[buffer_index], &in_buffer[buffer_index], size);
          block_offset = 0;  // Next block should be complete.
        } else {
          if (cipher_mode_ == OEMCrypto_CipherMode_CTR) {
            uint8_t aes_output[AES_BLOCK_SIZE];
            AES_encrypt(iv, aes_output, &aes_key);
            for (size_t n = 0; n < size; n++) {
              (*out_buffer)[buffer_index + n] =
                  aes_output[n + block_offset] ^ in_buffer[buffer_index + n];
            }
            if (size + block_offset < AES_BLOCK_SIZE) {
              // Partial block.  Don't increment iv.  Compute next block offset.
              block_offset = block_offset + size;
            } else {
              EXPECT_EQ(static_cast<size_t>(AES_BLOCK_SIZE),
                        block_offset + size);
              // Full block.  Increment iv, and set offset to 0 for next block.
              ctr128_inc64(1, iv);
              block_offset = 0;
            }
          } else {
            uint8_t aes_input[AES_BLOCK_SIZE];
            for (size_t n = 0; n < size; n++) {
              aes_input[n] = in_buffer[buffer_index + n] ^ iv[n];
            }
            AES_encrypt(aes_input, &(*out_buffer)[buffer_index], &aes_key);
            memcpy(iv, &(*out_buffer)[buffer_index], AES_BLOCK_SIZE);
            // CBC mode should always start on block boundary.
            block_offset = 0;
          }
        }
        buffer_index += size;
      }
    }
  }

  void TestDecryptCENC(const vector<uint8_t>& key,
                       const vector<uint8_t>& /* encryptionIv */,
                       const vector<uint8_t>& encryptedData,
                       const vector<uint8_t>& unencryptedData) {
    OEMCryptoResult sts;
    Session s;
    ASSERT_NO_FATAL_FAILURE(s.open());
    ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
    ASSERT_NO_FATAL_FAILURE(
        s.FillSimpleMessage(kDuration, kControlAllowHashVerification, 0));
    memcpy(s.license().keys[0].key_data, key.data(), key.size());
    s.license().keys[0].cipher_mode = cipher_mode_;
    ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
    ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
    if (global_features.supports_crc) {
      uint32_t hash =
          wvcrc32(unencryptedData.data(), unencryptedData.size());
      ASSERT_EQ(OEMCrypto_SUCCESS,
                OEMCrypto_SetDecryptHash(
                    s.session_id(), 1, reinterpret_cast<const uint8_t*>(&hash),
                    sizeof(hash)));
    }
    sts = OEMCrypto_SelectKey(s.session_id(), s.license().keys[0].key_id,
                              s.license().keys[0].key_id_length,
                              cipher_mode_);
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);
    // We decrypt each subsample.
    vector<uint8_t> output_buffer(total_size_ + 16, 0xaa);
    const uint8_t *input_buffer = NULL;
    if (decrypt_inplace_) {  // Use same buffer for input and output.
      // Copy the useful data from encryptedData to output_buffer, which
      // will be the same as input_buffer. Leave the 0xaa padding at the end.
      for(size_t i=0; i < total_size_; i++) output_buffer[i] = encryptedData[i];
      // Now let input_buffer point to the same data.
      input_buffer = output_buffer.data();
    } else {
      input_buffer = encryptedData.data();
    }
    size_t buffer_offset = 0;
    for (size_t i = 0; i < subsample_size_.size(); i++) {
      OEMCrypto_CENCEncryptPatternDesc pattern = pattern_;
      pattern.offset = 0;  // Final CENC spec says pattern offset always 0.
      bool is_encrypted = false;
      OEMCrypto_DestBufferDesc destBuffer;
      size_t block_offset = 0;
      uint8_t subsample_flags = 0;
      if (subsample_size_[i].clear_size > 0) {
        destBuffer.type = OEMCrypto_BufferType_Clear;
        destBuffer.buffer.clear.address = &output_buffer[buffer_offset];
        destBuffer.buffer.clear.max_length = total_size_ - buffer_offset;
        if (i == 0) subsample_flags |= OEMCrypto_FirstSubsample;
        if ((i == subsample_size_.size() - 1) &&
            (subsample_size_[i].encrypted_size == 0)) {
          subsample_flags |= OEMCrypto_LastSubsample;
        }
        sts =
            OEMCrypto_DecryptCENC(s.session_id(), input_buffer + buffer_offset,
                                  subsample_size_[i].clear_size, is_encrypted,
                                  sample_init_data_[i].iv, block_offset,
                                  &destBuffer, &pattern, subsample_flags);
        ASSERT_EQ(OEMCrypto_SUCCESS, sts);
        buffer_offset += subsample_size_[i].clear_size;
      }
      if (subsample_size_[i].encrypted_size > 0) {
        destBuffer.type = OEMCrypto_BufferType_Clear;
        destBuffer.buffer.clear.address = &output_buffer[buffer_offset];
        destBuffer.buffer.clear.max_length = total_size_ - buffer_offset;
        is_encrypted = true;
        block_offset = sample_init_data_[i].block_offset;
        subsample_flags = 0;
        if ((i == 0) && (subsample_size_[i].clear_size == 0)) {
          subsample_flags |= OEMCrypto_FirstSubsample;
        }
        if (i == subsample_size_.size() - 1) {
          subsample_flags |= OEMCrypto_LastSubsample;
        }
        sts = OEMCrypto_DecryptCENC(
            s.session_id(), input_buffer + buffer_offset,
            subsample_size_[i].encrypted_size, is_encrypted,
            sample_init_data_[i].iv, block_offset, &destBuffer, &pattern,
            subsample_flags);
        // CBC mode should not accept a block offset.
        if ((block_offset > 0) && (cipher_mode_ == OEMCrypto_CipherMode_CBC)) {
          ASSERT_EQ(OEMCrypto_ERROR_INVALID_CONTEXT, sts)
              << "CBC Mode should reject a non-zero block offset.";
          return;
        }
        ASSERT_EQ(OEMCrypto_SUCCESS, sts);
        buffer_offset += subsample_size_[i].encrypted_size;
      }
    }
    EXPECT_EQ(0xaa, output_buffer[total_size_]) << "Buffer overrun.";
    output_buffer.resize(total_size_);
    EXPECT_EQ(unencryptedData, output_buffer);
    if (global_features.supports_crc) {
      uint32_t frame;
      ASSERT_EQ(OEMCrypto_SUCCESS,
                OEMCrypto_GetHashErrorCode(s.session_id(), &frame));

    }
  }

  OEMCrypto_CENCEncryptPatternDesc pattern_;
  OEMCryptoCipherMode cipher_mode_;
  bool decrypt_inplace_;  // If true, input and output buffers are the same.
  vector<SampleSize> subsample_size_;
  size_t total_size_;
  vector<SampleInitData> sample_init_data_;
};

// Tests that generate partial ending blocks.  These tests should not be used
// with CTR mode and pattern encrypt.
class OEMCryptoSessionTestsPartialBlockTests
    : public OEMCryptoSessionTestsDecryptTests {};

TEST_P(OEMCryptoSessionTestsDecryptTests, SingleLargeSubsample) {
  // This subsample size is larger than a few encrypt/skip patterns.  Most
  // test cases use a pattern length of 160, so we'll run through at least two
  // full patterns if we have more than 320 -- round up to 400.
  subsample_size_.push_back(SampleSize(0, 400));
  FindTotalSize();
  vector<uint8_t> unencryptedData(total_size_);
  vector<uint8_t> encryptedData(total_size_);
  vector<uint8_t> encryptionIv(AES_BLOCK_SIZE);
  vector<uint8_t> key(AES_BLOCK_SIZE);
  EXPECT_EQ(1, GetRandBytes(encryptionIv.data(), AES_BLOCK_SIZE));
  EXPECT_EQ(1, GetRandBytes(key.data(), AES_BLOCK_SIZE));
  for (size_t i = 0; i < total_size_; i++) unencryptedData[i] = i % 256;
  EncryptData(key, encryptionIv, unencryptedData, &encryptedData);
  TestDecryptCENC(key, encryptionIv, encryptedData, unencryptedData);
}

// When the pattern length is 10 blocks, there is a discrepancy between the
// HLS and the CENC standards for samples of size 160*N+16, for N = 1, 2, 3...
// We require the CENC standard for OEMCrypto, and let a layer above us break
// samples into pieces if they wish to use the HLS standard.
TEST_P(OEMCryptoSessionTestsDecryptTests, PatternPlusOneBlock) {
  subsample_size_.push_back(SampleSize(0, 160 + 16));
  FindTotalSize();
  vector<uint8_t> unencryptedData(total_size_);
  vector<uint8_t> encryptedData(total_size_);
  vector<uint8_t> encryptionIv(AES_BLOCK_SIZE);
  vector<uint8_t> key(AES_BLOCK_SIZE);
  EXPECT_EQ(1, GetRandBytes(encryptionIv.data(), AES_BLOCK_SIZE));
  EXPECT_EQ(1, GetRandBytes(key.data(), AES_BLOCK_SIZE));
  for (size_t i = 0; i < total_size_; i++) unencryptedData[i] = i % 256;
  EncryptData(key, encryptionIv, unencryptedData, &encryptedData);
  TestDecryptCENC(key, encryptionIv, encryptedData, unencryptedData);
}

// Test that a single block can be decrypted.
TEST_P(OEMCryptoSessionTestsDecryptTests, OneBlock) {
  subsample_size_.push_back(SampleSize(0, 16));
  FindTotalSize();
  vector<uint8_t> unencryptedData(total_size_);
  vector<uint8_t> encryptedData(total_size_);
  vector<uint8_t> encryptionIv(AES_BLOCK_SIZE);
  vector<uint8_t> key(AES_BLOCK_SIZE);
  EXPECT_EQ(1, GetRandBytes(encryptionIv.data(), AES_BLOCK_SIZE));
  EXPECT_EQ(1, GetRandBytes(key.data(), AES_BLOCK_SIZE));
  for (size_t i = 0; i < total_size_; i++) unencryptedData[i] = i % 256;
  EncryptData(key, encryptionIv, unencryptedData, &encryptedData);
  TestDecryptCENC(key, encryptionIv, encryptedData, unencryptedData);
}

// This tests the ability to decrypt multiple subsamples with no offset.
// There is no offset within the block, used by CTR mode.  However, there might
// be an offset in the encrypt/skip pattern.
TEST_P(OEMCryptoSessionTestsDecryptTests, NoOffset) {
  subsample_size_.push_back(SampleSize(25, 160));
  subsample_size_.push_back(SampleSize(50, 256));
  subsample_size_.push_back(SampleSize(25, 160));
  FindTotalSize();
  vector<uint8_t> unencryptedData(total_size_);
  vector<uint8_t> encryptedData(total_size_);
  vector<uint8_t> encryptionIv(AES_BLOCK_SIZE);
  vector<uint8_t> key(AES_BLOCK_SIZE);
  EXPECT_EQ(1, GetRandBytes(encryptionIv.data(), AES_BLOCK_SIZE));
  EXPECT_EQ(1, GetRandBytes(key.data(), AES_BLOCK_SIZE));
  for (size_t i = 0; i < total_size_; i++) unencryptedData[i] = i % 256;
  EncryptData(key, encryptionIv, unencryptedData, &encryptedData);
  TestDecryptCENC(key, encryptionIv, encryptedData, unencryptedData);
}

// This tests an offset into the block for the second encrypted subsample.
// This should only work for CTR mode, for CBC mode an error is expected in
// the decrypt step.
// If this test fails for CTR mode, then it is probably handleing the
// block_offset incorrectly.
TEST_P(OEMCryptoSessionTestsPartialBlockTests, EvenOffset) {
  subsample_size_.push_back(SampleSize(25, 8));
  subsample_size_.push_back(SampleSize(25, 32));
  subsample_size_.push_back(SampleSize(25, 50));
  FindTotalSize();
  vector<uint8_t> unencryptedData(total_size_);
  vector<uint8_t> encryptedData(total_size_);
  vector<uint8_t> encryptionIv(AES_BLOCK_SIZE);
  vector<uint8_t> key(AES_BLOCK_SIZE);
  EXPECT_EQ(1, GetRandBytes(key.data(), AES_BLOCK_SIZE));
  // CTR Mode is self-inverse -- i.e. We can pick the encrypted data and
  // compute the unencrypted data.  By picking the encrypted data to be all 0,
  // it is easier to re-encrypt the data and debug problems.  Similarly, we
  // pick an iv = 0.
  EncryptData(key, encryptionIv, encryptedData, &unencryptedData);
  // Run EncryptData again to correctly compute intermediate IV vectors.
  // For CBC mode, this also computes the real encrypted data.
  EncryptData(key, encryptionIv, unencryptedData, &encryptedData);
  TestDecryptCENC(key, encryptionIv, encryptedData, unencryptedData);
}

// If the EvenOffset test passes, but this one doesn't, then DecryptCTR might
// be using the wrong definition of block offset.  Adding the block offset to
// the block boundary should give you the beginning of the encrypted data.
// This should only work for CTR mode, for CBC mode, the block offset must be
// 0, so an error is expected in the decrypt step.
// Another way to view the block offset is with the formula:
// block_boundary + block_offset = beginning of subsample.
TEST_P(OEMCryptoSessionTestsPartialBlockTests, OddOffset) {
  subsample_size_.push_back(SampleSize(10, 50));
  subsample_size_.push_back(SampleSize(10, 75));
  subsample_size_.push_back(SampleSize(10, 25));
  FindTotalSize();
  vector<uint8_t> unencryptedData(total_size_);
  vector<uint8_t> encryptedData(total_size_);
  vector<uint8_t> encryptionIv(AES_BLOCK_SIZE);
  vector<uint8_t> key(AES_BLOCK_SIZE);
  EXPECT_EQ(1, GetRandBytes(encryptionIv.data(), AES_BLOCK_SIZE));
  EXPECT_EQ(1, GetRandBytes(key.data(), AES_BLOCK_SIZE));
  for (size_t i = 0; i < total_size_; i++) unencryptedData[i] = i % 256;
  EncryptData(key, encryptionIv, unencryptedData, &encryptedData);
  TestDecryptCENC(key, encryptionIv, encryptedData, unencryptedData);
}

// This tests that the algorithm used to increment the counter for
// AES-CTR mode is correct.  There are two possible implementations:
// 1) increment the counter as if it were a 128 bit number,
// 2) increment the low 64 bits as a 64 bit number and leave the high bits
// alone.
// For CENC, the algorithm we should use is the second one.  OpenSSL defaults to
// the first.  If this test is not passing, you should look at the way you
// increment the counter.  Look at the example code in ctr128_inc64 above.
// If you start with an IV of 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE, after you
// increment twice, you should get 0xFFFFFFFFFFFFFFFF0000000000000000.
TEST_P(OEMCryptoSessionTestsDecryptTests, DecryptWithNearWrap) {
  subsample_size_.push_back(SampleSize(0, 256));
  FindTotalSize();
  vector<uint8_t> unencryptedData(total_size_);
  vector<uint8_t> encryptedData(total_size_);
  vector<uint8_t> encryptionIv(AES_BLOCK_SIZE);
  vector<uint8_t> key(AES_BLOCK_SIZE);
  EXPECT_EQ(1, GetRandBytes(key.data(), AES_BLOCK_SIZE));
  encryptionIv = wvcdm::a2b_hex("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE");
  for (size_t i = 0; i < total_size_; i++) unencryptedData[i] = i % 256;
  EncryptData(key, encryptionIv, unencryptedData, &encryptedData);
  TestDecryptCENC(key, encryptionIv, encryptedData, unencryptedData);
}

// This tests the case where an encrypted sample is not an even number of
// blocks. For CTR mode, the partial block is encrypted.  For CBC mode the
// partial block should be a copy of the clear data.
TEST_P(OEMCryptoSessionTestsPartialBlockTests, PartialBlock) {
  // Note: for more complete test coverage, we want a sample size that is in
  // the encrypted range for some tests, e.g. (3,7), and in the skip range for
  // other tests, e.g. (7, 3).  3*16 < 50 and 7*16 > 50.
  subsample_size_.push_back(SampleSize(0, 50));
  FindTotalSize();
  vector<uint8_t> unencryptedData(total_size_);
  vector<uint8_t> encryptedData(total_size_);
  vector<uint8_t> encryptionIv(AES_BLOCK_SIZE);
  vector<uint8_t> key(AES_BLOCK_SIZE);
  EXPECT_EQ(1, GetRandBytes(encryptionIv.data(), AES_BLOCK_SIZE));
  EXPECT_EQ(1, GetRandBytes(key.data(), AES_BLOCK_SIZE));
  for (size_t i = 0; i < total_size_; i++) unencryptedData[i] = i % 256;
  EncryptData(key, encryptionIv, unencryptedData, &encryptedData);
  TestDecryptCENC(key, encryptionIv, encryptedData, unencryptedData);
}

// Based on the resource rating, oemcrypto should handle at least
// kMaxNumberSubsamples na kMaxSampleSize
TEST_P(OEMCryptoSessionTestsDecryptTests, DecryptMaxSample) {
  size_t max_size = GetResourceValue(kMaxSampleSize);
  size_t max_subsample_size = GetResourceValue(kMaxSubsampleSize);
  size_t num_subsamples = GetResourceValue(kMaxNumberSubsamples);
  if (num_subsamples * max_subsample_size > max_size) {
    max_subsample_size = max_size / num_subsamples;
  }
  for(size_t i = 0; i < num_subsamples/2; i += 2) {
    subsample_size_.push_back(SampleSize(max_subsample_size, 0));
    subsample_size_.push_back(SampleSize(0, max_subsample_size));
  }
  FindTotalSize();
  vector<uint8_t> unencryptedData(total_size_);
  vector<uint8_t> encryptedData(total_size_);
  vector<uint8_t> encryptionIv(AES_BLOCK_SIZE);
  vector<uint8_t> key(AES_BLOCK_SIZE);
  EXPECT_EQ(1, GetRandBytes(encryptionIv.data(), AES_BLOCK_SIZE));
  EXPECT_EQ(1, GetRandBytes(key.data(), AES_BLOCK_SIZE));
  for (size_t i = 0; i < total_size_; i++) unencryptedData[i] = i % 256;
  EncryptData(key, encryptionIv, unencryptedData, &encryptedData);
  TestDecryptCENC(key, encryptionIv, encryptedData, unencryptedData);
}

// This tests that we can decrypt the required maximum number of subsamples.
TEST_P(OEMCryptoSessionTestsDecryptTests, DecryptMaxSubsample) {
  size_t max_subsample_size = GetResourceValue(kMaxSubsampleSize);
  subsample_size_.push_back(SampleSize(max_subsample_size, 0));
  subsample_size_.push_back(SampleSize(0, max_subsample_size));
  FindTotalSize();
  vector<uint8_t> unencryptedData(total_size_);
  vector<uint8_t> encryptedData(total_size_);
  vector<uint8_t> encryptionIv(AES_BLOCK_SIZE);
  vector<uint8_t> key(AES_BLOCK_SIZE);
  EXPECT_EQ(1, GetRandBytes(encryptionIv.data(), AES_BLOCK_SIZE));
  EXPECT_EQ(1, GetRandBytes(key.data(), AES_BLOCK_SIZE));
  for (size_t i = 0; i < total_size_; i++) unencryptedData[i] = i % 256;
  EncryptData(key, encryptionIv, unencryptedData, &encryptedData);
  TestDecryptCENC(key, encryptionIv, encryptedData, unencryptedData);
}

// There are probably no frames this small, but we should handle them anyway.
TEST_P(OEMCryptoSessionTestsDecryptTests, DecryptSmallBuffer) {
  subsample_size_.push_back(SampleSize(5, 5));
  FindTotalSize();
  vector<uint8_t> unencryptedData(total_size_);
  vector<uint8_t> encryptedData(total_size_);
  vector<uint8_t> encryptionIv(AES_BLOCK_SIZE);
  vector<uint8_t> key(AES_BLOCK_SIZE);
  EXPECT_EQ(1, GetRandBytes(encryptionIv.data(), AES_BLOCK_SIZE));
  EXPECT_EQ(1, GetRandBytes(key.data(), AES_BLOCK_SIZE));
  for (size_t i = 0; i < total_size_; i++) unencryptedData[i] = i % 256;
  EncryptData(key, encryptionIv, unencryptedData, &encryptedData);
  TestDecryptCENC(key, encryptionIv, encryptedData, unencryptedData);
}

// Test the case where there is only a clear subsample and no encrypted
// subsample.
TEST_P(OEMCryptoSessionTestsDecryptTests, DecryptUnencrypted) {
  subsample_size_.push_back(SampleSize(256, 0));
  FindTotalSize();
  vector<uint8_t> unencryptedData(total_size_);
  vector<uint8_t> encryptedData(total_size_);
  vector<uint8_t> encryptionIv(AES_BLOCK_SIZE);
  vector<uint8_t> key(AES_BLOCK_SIZE);
  EXPECT_EQ(1, GetRandBytes(encryptionIv.data(), AES_BLOCK_SIZE));
  EXPECT_EQ(1, GetRandBytes(key.data(), AES_BLOCK_SIZE));
  for (size_t i = 0; i < total_size_; i++) unencryptedData[i] = i % 256;
  EncryptData(key, encryptionIv, unencryptedData, &encryptedData);
  TestDecryptCENC(key, encryptionIv, encryptedData, unencryptedData);
}

TEST_F(OEMCryptoSessionTests, DecryptUnencryptedNoKey) {
  OEMCryptoResult sts;
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  // Clear data should be copied even if there is no key selected.
  // Set up our expected input and output
  // This is dummy decrypted data.
  vector<uint8_t> in_buffer(256);
  for (size_t i = 0; i < in_buffer.size(); i++) in_buffer[i] = i % 256;
  vector<uint8_t> encryptionIv(AES_BLOCK_SIZE);
  EXPECT_EQ(1, GetRandBytes(encryptionIv.data(), AES_BLOCK_SIZE));
  // Describe the output
  vector<uint8_t> out_buffer(in_buffer.size());
  OEMCrypto_DestBufferDesc destBuffer;
  destBuffer.type = OEMCrypto_BufferType_Clear;
  destBuffer.buffer.clear.address = out_buffer.data();
  destBuffer.buffer.clear.max_length = out_buffer.size();
  OEMCrypto_CENCEncryptPatternDesc pattern;
  pattern.encrypt = 0;
  pattern.skip = 0;
  pattern.offset = 0;

  // Decrypt the data
  sts =
      OEMCrypto_DecryptCENC(s.session_id(), in_buffer.data(), in_buffer.size(),
                            false, encryptionIv.data(), 0, &destBuffer,
                            &pattern,
                            OEMCrypto_FirstSubsample | OEMCrypto_LastSubsample);
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  ASSERT_EQ(in_buffer, out_buffer);
}

// Used to construct a specific pattern.
OEMCrypto_CENCEncryptPatternDesc MakePattern(size_t encrypt, size_t skip) {
  OEMCrypto_CENCEncryptPatternDesc pattern;
  pattern.encrypt = encrypt;
  pattern.skip = skip;
  pattern.offset = 0;  // offset is deprecated.
  return pattern;
}

INSTANTIATE_TEST_CASE_P(CTRTests, OEMCryptoSessionTestsPartialBlockTests,
                        Combine(Values(MakePattern(0,0)),
                                Values(OEMCrypto_CipherMode_CTR),
                                Bool()));

// Decrypt in place for CBC tests was only required in v13.
INSTANTIATE_TEST_CASE_P(
    CBCTestsAPI14, OEMCryptoSessionTestsPartialBlockTests,
    Combine(
        Values(MakePattern(0, 0),
               MakePattern(3, 7),
               // HLS Edge case.  We should follow the CENC spec, not HLS spec.
               MakePattern(9, 1),
               MakePattern(1, 9),
               MakePattern(1, 3),
               MakePattern(2, 1)),
        Values(OEMCrypto_CipherMode_CBC), Bool()));

INSTANTIATE_TEST_CASE_P(
    CTRTestsAPI11, OEMCryptoSessionTestsDecryptTests,
    Combine(
        Values(MakePattern(0, 0),
               MakePattern(3, 7),
               // Pattern length should be 10, but that is not guaranteed.
               MakePattern(1, 3),
               MakePattern(2, 1)),
        Values(OEMCrypto_CipherMode_CTR), Bool()));

// Decrypt in place for CBC tests was only required in v13.
INSTANTIATE_TEST_CASE_P(
    CBCTestsAPI14, OEMCryptoSessionTestsDecryptTests,
    Combine(
        Values(MakePattern(0, 0),
               MakePattern(3, 7),
               // HLS Edge case.  We should follow the CENC spec, not HLS spec.
               MakePattern(9, 1),
               MakePattern(1, 9),
               // Pattern length should be 10, but that is not guaranteed.
               MakePattern(1, 3),
               MakePattern(2, 1)),
        Values(OEMCrypto_CipherMode_CBC), Bool()));

// A request to decrypt data to a clear buffer when the key control block
// requires a secure data path.
TEST_F(OEMCryptoSessionTests, DecryptSecureToClear) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      kDuration,
      wvoec::kControlObserveDataPath | wvoec::kControlDataPathSecure,
      0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
  ASSERT_NO_FATAL_FAILURE(
      s.TestDecryptCTR(true, OEMCrypto_ERROR_UNKNOWN_FAILURE));
}

// If analog is forbidden, then decrypt to a clear buffer should be forbidden.
TEST_F(OEMCryptoSessionTests, DecryptNoAnalogToClearAPI13) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      kDuration, wvoec::kControlDisableAnalogOutput, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
  ASSERT_NO_FATAL_FAILURE(
      s.TestDecryptCTR(true, OEMCrypto_ERROR_ANALOG_OUTPUT));
}

// Test that key duration is honored.
TEST_F(OEMCryptoSessionTests, KeyDuration) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      kDuration, wvoec::kControlNonceEnabled, s.get_nonce()));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR(true, OEMCrypto_SUCCESS));
  sleep(kShortSleep);  //  Should still be valid key.
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR(false, OEMCrypto_SUCCESS));
  sleep(kLongSleep);  // Should be expired key.
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR(false, OEMCrypto_ERROR_KEY_EXPIRED));
  ASSERT_NO_FATAL_FAILURE(s.TestSelectExpired(0));
}

//
// Certificate Root of Trust Tests
//
class OEMCryptoLoadsCertificate : public OEMCryptoSessionTestKeyboxTest {};

// This test verifies that we can create a wrapped RSA key, and then reload it.
TEST_F(OEMCryptoLoadsCertificate, LoadRSASessionKey) {
  CreateWrappedRSAKey(kSign_RSASSA_PSS, true);
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.InstallRSASessionTestKey(wrapped_rsa_key_));
}

// This creates a wrapped RSA key, and then does the sanity check that the
// unencrypted key is not found in the wrapped key.  The wrapped key should be
// encrypted.
TEST_F(OEMCryptoLoadsCertificate, CertificateProvision) {
  CreateWrappedRSAKey(kSign_RSASSA_PSS, true);
  // We should not be able to find the rsa key in the wrapped key. It should
  // be encrypted.
  ASSERT_EQ(NULL, find(wrapped_rsa_key_, encoded_rsa_key_));
}

// Verify that RewrapDeviceRSAKey checks pointers are within the provisioning
// message.
TEST_F(OEMCryptoLoadsCertificate, CertificateProvisionBadRange1KeyboxTest) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.GenerateDerivedKeysFromKeybox(keybox_));
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature());
  struct RSAPrivateKeyMessage encrypted;
  std::vector<uint8_t> signature;
  ASSERT_NO_FATAL_FAILURE(s.MakeRSACertificate(&encrypted, sizeof(encrypted),
                                               &signature, kSign_RSASSA_PSS,
                                               encoded_rsa_key_));
  vector<uint8_t> wrapped_key;
  const uint8_t* message_ptr = reinterpret_cast<const uint8_t*>(&encrypted);
  size_t wrapped_key_length = 0;
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER,
            OEMCrypto_RewrapDeviceRSAKey(
                s.session_id(), message_ptr, sizeof(encrypted),
                signature.data(), signature.size(), &encrypted.nonce,
                encrypted.rsa_key, encrypted.rsa_key_length,
                encrypted.rsa_key_iv, NULL, &wrapped_key_length));
  wrapped_key.clear();
  wrapped_key.assign(wrapped_key_length, 0);
  uint32_t nonce = encrypted.nonce;
  ASSERT_NE(
      OEMCrypto_SUCCESS,
      OEMCrypto_RewrapDeviceRSAKey(
          s.session_id(), message_ptr, sizeof(encrypted), signature.data(),
          signature.size(), &nonce, encrypted.rsa_key, encrypted.rsa_key_length,
          encrypted.rsa_key_iv, &(wrapped_key.front()), &wrapped_key_length));
}

// Verify that RewrapDeviceRSAKey checks pointers are within the provisioning
// message.
TEST_F(OEMCryptoLoadsCertificate, CertificateProvisionBadRange2KeyboxTest) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.GenerateDerivedKeysFromKeybox(keybox_));
  // Provisioning request would be signed by client and verified by server.
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature());
  struct RSAPrivateKeyMessage encrypted;
  std::vector<uint8_t> signature;
  s.MakeRSACertificate(&encrypted, sizeof(encrypted), &signature,
                       kSign_RSASSA_PSS, encoded_rsa_key_);
  vector<uint8_t> wrapped_key;
  const uint8_t* message_ptr = reinterpret_cast<const uint8_t*>(&encrypted);
  size_t wrapped_key_length = 0;
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER,
            OEMCrypto_RewrapDeviceRSAKey(
                s.session_id(), message_ptr, sizeof(encrypted),
                signature.data(), signature.size(), &encrypted.nonce,
                encrypted.rsa_key, encrypted.rsa_key_length,
                encrypted.rsa_key_iv, NULL, &wrapped_key_length));
  wrapped_key.clear();
  wrapped_key.assign(wrapped_key_length, 0);
  vector<uint8_t> bad_buffer(encrypted.rsa_key,
                             encrypted.rsa_key + sizeof(encrypted.rsa_key));

  ASSERT_NE(OEMCrypto_SUCCESS,
            OEMCrypto_RewrapDeviceRSAKey(
                s.session_id(), message_ptr, sizeof(encrypted),
                signature.data(), signature.size(), &encrypted.nonce,
                bad_buffer.data(), encrypted.rsa_key_length,
                encrypted.rsa_key_iv, wrapped_key.data(), &wrapped_key_length));
}

// Verify that RewrapDeviceRSAKey checks pointers are within the provisioning
// message.
TEST_F(OEMCryptoLoadsCertificate, CertificateProvisionBadRange3KeyboxTest) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.GenerateDerivedKeysFromKeybox(keybox_));
  // Provisioning request would be signed by client and verified by server.
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature());
  struct RSAPrivateKeyMessage encrypted;
  std::vector<uint8_t> signature;
  s.MakeRSACertificate(&encrypted, sizeof(encrypted), &signature,
                       kSign_RSASSA_PSS, encoded_rsa_key_);
  const uint8_t* message_ptr = reinterpret_cast<const uint8_t*>(&encrypted);
  vector<uint8_t> wrapped_key;

  size_t wrapped_key_length = 0;
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER,
            OEMCrypto_RewrapDeviceRSAKey(
                s.session_id(), message_ptr, sizeof(encrypted),
                signature.data(), signature.size(), &encrypted.nonce,
                encrypted.rsa_key, encrypted.rsa_key_length,
                encrypted.rsa_key_iv, NULL, &wrapped_key_length));
  wrapped_key.clear();
  wrapped_key.assign(wrapped_key_length, 0);
  vector<uint8_t> bad_buffer(encrypted.rsa_key,
                             encrypted.rsa_key + sizeof(encrypted.rsa_key));

  ASSERT_NE(OEMCrypto_SUCCESS,
            OEMCrypto_RewrapDeviceRSAKey(
                s.session_id(), message_ptr, sizeof(encrypted),
                signature.data(), signature.size(), &encrypted.nonce,
                encrypted.rsa_key, encrypted.rsa_key_length, bad_buffer.data(),
                &(wrapped_key.front()), &wrapped_key_length));
}

// Test that RewrapDeviceRSAKey verifies the message signature.
TEST_F(OEMCryptoLoadsCertificate, CertificateProvisionBadSignatureKeyboxTest) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.GenerateDerivedKeysFromKeybox(keybox_));
  // Provisioning request would be signed by client and verified by server.
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature());
  struct RSAPrivateKeyMessage encrypted;
  std::vector<uint8_t> signature;
  s.MakeRSACertificate(&encrypted, sizeof(encrypted), &signature,
                       kSign_RSASSA_PSS, encoded_rsa_key_);
  vector<uint8_t> wrapped_key;
  const uint8_t* message_ptr = reinterpret_cast<const uint8_t*>(&encrypted);

  size_t wrapped_key_length = 0;
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER,
            OEMCrypto_RewrapDeviceRSAKey(
                s.session_id(), message_ptr, sizeof(encrypted),
                signature.data(), signature.size(), &encrypted.nonce,
                encrypted.rsa_key, encrypted.rsa_key_length,
                encrypted.rsa_key_iv, NULL, &wrapped_key_length));
  wrapped_key.clear();
  wrapped_key.assign(wrapped_key_length, 0);
  signature[4] ^= 42;  // bad signature.
  ASSERT_NE(OEMCrypto_SUCCESS,
            OEMCrypto_RewrapDeviceRSAKey(
                s.session_id(), message_ptr, sizeof(encrypted),
                signature.data(), signature.size(), &encrypted.nonce,
                encrypted.rsa_key, encrypted.rsa_key_length,
                encrypted.rsa_key_iv, wrapped_key.data(), &wrapped_key_length));
}

// Test that RewrapDeviceRSAKey verifies the nonce is current.
TEST_F(OEMCryptoLoadsCertificate, CertificateProvisionBadNonceKeyboxTest) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.GenerateDerivedKeysFromKeybox(keybox_));
  // Provisioning request would be signed by client and verified by server.
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature());
  struct RSAPrivateKeyMessage encrypted;
  std::vector<uint8_t> signature;
  s.MakeRSACertificate(&encrypted, sizeof(encrypted), &signature,
                       kSign_RSASSA_PSS, encoded_rsa_key_);
  vector<uint8_t> wrapped_key;
  const uint8_t* message_ptr = reinterpret_cast<const uint8_t*>(&encrypted);

  size_t wrapped_key_length = 0;
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER,
            OEMCrypto_RewrapDeviceRSAKey(
                s.session_id(), message_ptr, sizeof(encrypted),
                signature.data(), signature.size(), &encrypted.nonce,
                encrypted.rsa_key, encrypted.rsa_key_length,
                encrypted.rsa_key_iv, NULL, &wrapped_key_length));
  wrapped_key.clear();
  wrapped_key.assign(wrapped_key_length, 0);
  encrypted.nonce ^= 42;  // Almost surely a bad nonce.
  ASSERT_EQ(OEMCrypto_ERROR_INVALID_NONCE,
            OEMCrypto_RewrapDeviceRSAKey(
                s.session_id(), message_ptr, sizeof(encrypted),
                signature.data(), signature.size(), &encrypted.nonce,
                encrypted.rsa_key, encrypted.rsa_key_length,
                encrypted.rsa_key_iv, wrapped_key.data(), &wrapped_key_length));
}

// Test that RewrapDeviceRSAKey verifies the RSA key is valid.
TEST_F(OEMCryptoLoadsCertificate, CertificateProvisionBadRSAKeyKeyboxTest) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.GenerateDerivedKeysFromKeybox(keybox_));
  // Provisioning request would be signed by client and verified by server.
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature());
  struct RSAPrivateKeyMessage encrypted;
  std::vector<uint8_t> signature;
  s.MakeRSACertificate(&encrypted, sizeof(encrypted), &signature,
                       kSign_RSASSA_PSS, encoded_rsa_key_);
  vector<uint8_t> wrapped_key;
  const uint8_t* message_ptr = reinterpret_cast<const uint8_t*>(&encrypted);

  size_t wrapped_key_length = 0;
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER,
            OEMCrypto_RewrapDeviceRSAKey(
                s.session_id(), message_ptr, sizeof(encrypted),
                signature.data(), signature.size(), &encrypted.nonce,
                encrypted.rsa_key, encrypted.rsa_key_length,
                encrypted.rsa_key_iv, NULL, &wrapped_key_length));
  wrapped_key.clear();
  wrapped_key.assign(wrapped_key_length, 0);
  encrypted.rsa_key[1] ^= 42;  // Almost surely a bad key.
  ASSERT_NE(OEMCrypto_SUCCESS,
            OEMCrypto_RewrapDeviceRSAKey(
                s.session_id(), message_ptr, sizeof(encrypted),
                signature.data(), signature.size(), &encrypted.nonce,
                encrypted.rsa_key, encrypted.rsa_key_length,
                encrypted.rsa_key_iv, wrapped_key.data(), &wrapped_key_length));
}

// Test that RewrapDeviceRSAKey accepts the maximum message size.
TEST_F(OEMCryptoLoadsCertificate, CertificateProvisionLargeBufferKeyboxTest) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.GenerateDerivedKeysFromKeybox(keybox_));
  // Provisioning request would be signed by client and verified by server.
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature());
  struct LargeRSAPrivateKeyMessage : public RSAPrivateKeyMessage {
    uint8_t padding[kMaxMessageSize - sizeof(RSAPrivateKeyMessage)];
  } encrypted;
  std::vector<uint8_t> signature;
  s.MakeRSACertificate(&encrypted, sizeof(encrypted), &signature,
                       kSign_RSASSA_PSS, encoded_rsa_key_);
  vector<uint8_t> wrapped_key;
  ASSERT_NO_FATAL_FAILURE(s.RewrapRSAKey(encrypted, sizeof(encrypted),
                                         signature, &wrapped_key, true));
  // Verify that the clear key is not contained in the wrapped key.
  // It should be encrypted.
  ASSERT_EQ(NULL, find(wrapped_key, encoded_rsa_key_));
}

// Test that RewrapDeviceRSAKey30 verifies the nonce.
TEST_F(OEMCryptoLoadsCertificate, CertificateProvisionBadNonceProv30Test) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.LoadOEMCert());
  s.GenerateNonce();
  uint32_t bad_nonce = s.get_nonce() ^ 42;
  struct RSAPrivateKeyMessage encrypted;
  std::vector<uint8_t> signature;
  std::vector<uint8_t> message_key;
  std::vector<uint8_t> encrypted_message_key;
  s.GenerateRSASessionKey(&message_key, &encrypted_message_key);
  ASSERT_NO_FATAL_FAILURE(s.MakeRSACertificate(&encrypted, sizeof(encrypted),
                                               &signature, kSign_RSASSA_PSS,
                                               encoded_rsa_key_, &message_key));
  size_t wrapped_key_length = 0;
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER,
            OEMCrypto_RewrapDeviceRSAKey30(
                s.session_id(), &bad_nonce, encrypted_message_key.data(),
                encrypted_message_key.size(), encrypted.rsa_key,
                encrypted.rsa_key_length, encrypted.rsa_key_iv, NULL,
                &wrapped_key_length));
  vector<uint8_t> wrapped_key(wrapped_key_length, 0);
  ASSERT_EQ(OEMCrypto_ERROR_INVALID_NONCE,
            OEMCrypto_RewrapDeviceRSAKey30(
                s.session_id(), &bad_nonce, encrypted_message_key.data(),
                encrypted_message_key.size(), encrypted.rsa_key,
                encrypted.rsa_key_length, encrypted.rsa_key_iv,
                wrapped_key.data(), &wrapped_key_length));
}

// Test that RewrapDeviceRSAKey30 verifies that the RSA key is valid.
TEST_F(OEMCryptoLoadsCertificate, CertificateProvisionBadRSAKeyProv30Test) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.LoadOEMCert());
  s.GenerateNonce();
  struct RSAPrivateKeyMessage encrypted;
  std::vector<uint8_t> signature;
  std::vector<uint8_t> message_key;
  std::vector<uint8_t> encrypted_message_key;
  s.GenerateRSASessionKey(&message_key, &encrypted_message_key);
  ASSERT_NO_FATAL_FAILURE(s.MakeRSACertificate(&encrypted, sizeof(encrypted),
                                               &signature, kSign_RSASSA_PSS,
                                               encoded_rsa_key_, &message_key));
  size_t wrapped_key_length = 0;
  uint32_t nonce = s.get_nonce();
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER,
            OEMCrypto_RewrapDeviceRSAKey30(
                s.session_id(), &nonce, encrypted_message_key.data(),
                encrypted_message_key.size(), encrypted.rsa_key,
                encrypted.rsa_key_length, encrypted.rsa_key_iv, NULL,
                &wrapped_key_length));
  vector<uint8_t> wrapped_key(wrapped_key_length, 0);
  encrypted.rsa_key[1] ^= 42;  // Almost surely a bad key.
  ASSERT_EQ(OEMCrypto_ERROR_INVALID_RSA_KEY,
            OEMCrypto_RewrapDeviceRSAKey30(
                s.session_id(), &nonce, encrypted_message_key.data(),
                encrypted_message_key.size(), encrypted.rsa_key,
                encrypted.rsa_key_length, encrypted.rsa_key_iv,
                wrapped_key.data(), &wrapped_key_length));
}

// Test that a wrapped RSA key can be loaded.
TEST_F(OEMCryptoLoadsCertificate, LoadWrappedRSAKey) {
  OEMCryptoResult sts;
  CreateWrappedRSAKey(kSign_RSASSA_PSS, true);

  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  sts = OEMCrypto_LoadDeviceRSAKey(s.session_id(), wrapped_rsa_key_.data(),
                                   wrapped_rsa_key_.size());
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
}

// This tests that a device with a keybox can also decrypt with a cert.
// Decrypt for devices that only use a cert are tested in the session tests.
TEST_F(OEMCryptoLoadsCertificate, CertificateDecrypt) {
  CreateWrappedRSAKey(kSign_RSASSA_PSS, true);
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.InstallRSASessionTestKey(wrapped_rsa_key_));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(kDuration, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
}

// Test a 3072 bit RSA key certificate.
TEST_F(OEMCryptoLoadsCertificate, TestLargeRSAKey3072) {
  encoded_rsa_key_.assign(kTestRSAPKCS8PrivateKeyInfo3_3072,
                          kTestRSAPKCS8PrivateKeyInfo3_3072 +
                          sizeof(kTestRSAPKCS8PrivateKeyInfo3_3072));
  CreateWrappedRSAKey(kSign_RSASSA_PSS, true);
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.PreparePublicKey(encoded_rsa_key_.data(),
                                             encoded_rsa_key_.size()));
  ASSERT_NO_FATAL_FAILURE(s.InstallRSASessionTestKey(wrapped_rsa_key_));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(kDuration, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
}

// Test an RSA key certificate which has a private key generated using the
// Carmichael totient.
TEST_F(OEMCryptoLoadsCertificate, TestCarmichaelRSAKey) {
  encoded_rsa_key_.assign(
      kTestKeyRSACarmichael_2048,
      kTestKeyRSACarmichael_2048 + sizeof(kTestKeyRSACarmichael_2048));
  CreateWrappedRSAKey(kSign_RSASSA_PSS, true);
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.PreparePublicKey(encoded_rsa_key_.data(),
                                             encoded_rsa_key_.size()));
  ASSERT_NO_FATAL_FAILURE(s.InstallRSASessionTestKey(wrapped_rsa_key_));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(kDuration, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys());
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
}

// This tests that two sessions can use different RSA keys simultaneously.
TEST_F(OEMCryptoLoadsCertificate, TestMultipleRSAKeys) {
  CreateWrappedRSAKey(kSign_RSASSA_PSS, true);
  Session s1;  // Session s1 loads the default rsa key, but doesn't use it
               // until after s2 uses its key.
  ASSERT_NO_FATAL_FAILURE(s1.open());
  ASSERT_NO_FATAL_FAILURE(s1.PreparePublicKey(encoded_rsa_key_.data(),
                                             encoded_rsa_key_.size()));
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_LoadDeviceRSAKey(s1.session_id(), wrapped_rsa_key_.data(),
                                       wrapped_rsa_key_.size()));

  Session s2;  // Session s2 uses a different rsa key.
  encoded_rsa_key_.assign(kTestRSAPKCS8PrivateKeyInfo4_2048,
                          kTestRSAPKCS8PrivateKeyInfo4_2048 +
                              sizeof(kTestRSAPKCS8PrivateKeyInfo4_2048));
  CreateWrappedRSAKey(kSign_RSASSA_PSS, true);
  ASSERT_NO_FATAL_FAILURE(s2.open());
  ASSERT_NO_FATAL_FAILURE(s2.PreparePublicKey(encoded_rsa_key_.data(),
                                             encoded_rsa_key_.size()));
  ASSERT_NO_FATAL_FAILURE(s2.InstallRSASessionTestKey(wrapped_rsa_key_));
  ASSERT_NO_FATAL_FAILURE(s2.FillSimpleMessage(kDuration, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s2.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s2.LoadTestKeys());
  ASSERT_NO_FATAL_FAILURE(s2.TestDecryptCTR());
  s2.close();

  // After s2 has loaded its rsa key, we continue using s1's key.
  ASSERT_NO_FATAL_FAILURE(s1.GenerateDerivedKeysFromSessionKey());
  ASSERT_NO_FATAL_FAILURE(s1.FillSimpleMessage(kDuration, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s1.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s1.LoadTestKeys());
  ASSERT_NO_FATAL_FAILURE(s1.TestDecryptCTR());
}

// Devices that load certificates, should at least support RSA 2048 keys.
TEST_F(OEMCryptoLoadsCertificate, SupportsCertificatesAPI13) {
  ASSERT_NE(0u,
            OEMCrypto_Supports_RSA_2048bit & OEMCrypto_SupportedCertificates())
      << "Supported certificates is only " << OEMCrypto_SupportedCertificates();
}

class OEMCryptoUsesCertificate : public OEMCryptoLoadsCertificate {
 protected:
  void SetUp() override {
    OEMCryptoLoadsCertificate::SetUp();
    ASSERT_NO_FATAL_FAILURE(session_.open());
    if (global_features.derive_key_method !=
        DeviceFeatures::LOAD_TEST_RSA_KEY) {
      CreateWrappedRSAKey(kSign_RSASSA_PSS, true);
      ASSERT_EQ(OEMCrypto_SUCCESS,
                OEMCrypto_LoadDeviceRSAKey(session_.session_id(),
                                           wrapped_rsa_key_.data(),
                                           wrapped_rsa_key_.size()));
    }
  }

  void TearDown() override {
    session_.close();
    OEMCryptoLoadsCertificate::TearDown();
  }

  Session session_;
};

// This test is not run by default, because it takes a long time and
// is used to measure RSA performance, not test functionality.
TEST_F(OEMCryptoLoadsCertificate, RSAPerformance) {
  const std::chrono::milliseconds kTestDuration(5000);
  OEMCryptoResult sts;
  std::chrono::steady_clock clock;
  sleep(2);  // Make sure are not nonce limited.

  auto start_time = clock.now();
  int count = 15;
  for (int i = 0; i < count; i++) {  // Only 20 nonce available.
    CreateWrappedRSAKey(kSign_RSASSA_PSS, true);
  }
  auto delta_time = clock.now() - start_time;
  const double provision_time =
      delta_time / std::chrono::milliseconds(1) / count;

  Session session;
  CreateWrappedRSAKey(kSign_RSASSA_PSS, true);
  start_time = clock.now();
  count = 0;
  while (clock.now() - start_time < kTestDuration) {
    Session s;
    ASSERT_NO_FATAL_FAILURE(s.open());
    sts = OEMCrypto_LoadDeviceRSAKey(s.session_id(), wrapped_rsa_key_.data(),
                                     wrapped_rsa_key_.size());
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);
    const size_t size = 50;
    vector<uint8_t> licenseRequest(size);
    GetRandBytes(licenseRequest.data(), licenseRequest.size());
    size_t signature_length = 0;
    sts = OEMCrypto_GenerateRSASignature(s.session_id(), licenseRequest.data(),
                                         licenseRequest.size(), NULL,
                                         &signature_length, kSign_RSASSA_PSS);
    ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER, sts);
    ASSERT_NE(static_cast<size_t>(0), signature_length);
    uint8_t* signature = new uint8_t[signature_length];
    sts = OEMCrypto_GenerateRSASignature(s.session_id(), licenseRequest.data(),
                                         licenseRequest.size(), signature,
                                         &signature_length, kSign_RSASSA_PSS);
    delete[] signature;
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);
    count++;
  }
  delta_time = clock.now() - start_time;
  const double license_request_time =
      delta_time / std::chrono::milliseconds(1) / count;

  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_LoadDeviceRSAKey(s.session_id(), wrapped_rsa_key_.data(),
                                       wrapped_rsa_key_.size()));
  vector<uint8_t> session_key;
  vector<uint8_t> enc_session_key;
  ASSERT_NO_FATAL_FAILURE(s.PreparePublicKey(encoded_rsa_key_.data(),
                                             encoded_rsa_key_.size()));
  ASSERT_TRUE(s.GenerateRSASessionKey(&session_key, &enc_session_key));
  vector<uint8_t> mac_context;
  vector<uint8_t> enc_context;
  s.FillDefaultContext(&mac_context, &enc_context);

  enc_session_key = wvcdm::a2b_hex(
      "7789c619aa3b9fa3c0a53f57a4abc6"
      "02157c8aa57e3c6fb450b0bea22667fb"
      "0c3200f9d9d618e397837c720dc2dadf"
      "486f33590744b2a4e54ca134ae7dbf74"
      "434c2fcf6b525f3e132262f05ea3b3c1"
      "198595c0e52b573335b2e8a3debd0d0d"
      "d0306f8fcdde4e76476be71342957251"
      "e1688c9ca6c1c34ed056d3b989394160"
      "cf6937e5ce4d39cc73d11a2e93da21a2"
      "fa019d246c852fe960095b32f120c3c2"
      "7085f7b64aac344a68d607c0768676ce"
      "d4c5b2d057f7601921b453a451e1dea0"
      "843ebfef628d9af2784d68e86b730476"
      "e136dfe19989de4be30a4e7878efcde5"
      "ad2b1254f80c0c5dd3cf111b56572217"
      "b9f58fc1dacbf74b59d354a1e62cfa0e"
      "bf");
  start_time = clock.now();
  while (clock.now() - start_time < kTestDuration) {
    ASSERT_EQ(OEMCrypto_SUCCESS,
              OEMCrypto_DeriveKeysFromSessionKey(
                  s.session_id(),
                  enc_session_key.data(), enc_session_key.size(),
                  mac_context.data(), mac_context.size(),
                  enc_context.data(), enc_context.size()));
    count++;
  }
  delta_time = clock.now() - start_time;
  const double derive_keys_time =
      delta_time / std::chrono::milliseconds(1) / count;

  const char* level = OEMCrypto_SecurityLevel();
  printf("PERF:head, security, provision (ms), lic req(ms), derive keys(ms)\n");
  printf("PERF:stat, %s, %8.3f, %8.3f, %8.3f\n", level, provision_time,
         license_request_time, derive_keys_time);
}

// Test that OEMCrypto can compute an RSA signature.
TEST_F(OEMCryptoUsesCertificate, RSASignature) {
  OEMCryptoResult sts;
  // Sign a Message
  vector<uint8_t> licenseRequest(500);
  GetRandBytes(licenseRequest.data(), licenseRequest.size());
  size_t signature_length = 0;
  uint8_t signature[500];

  sts = OEMCrypto_GenerateRSASignature(
      session_.session_id(), licenseRequest.data(), licenseRequest.size(),
      signature, &signature_length, kSign_RSASSA_PSS);

  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER, sts);
  ASSERT_NE(static_cast<size_t>(0), signature_length);
  ASSERT_GE(sizeof(signature), signature_length);

  sts = OEMCrypto_GenerateRSASignature(
      session_.session_id(), licenseRequest.data(), licenseRequest.size(),
      signature, &signature_length, kSign_RSASSA_PSS);

  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  // In the real world, the signature above would just have been used to contact
  // the license server to get this response.
  ASSERT_NO_FATAL_FAILURE(session_.PreparePublicKey(encoded_rsa_key_.data(),
                                                    encoded_rsa_key_.size()));
  ASSERT_NO_FATAL_FAILURE(session_.VerifyRSASignature(
      licenseRequest, signature, signature_length, kSign_RSASSA_PSS));
}

// Test that OEMCrypto can compute an RSA signature of a message with the
// maximum size.
TEST_F(OEMCryptoUsesCertificate, RSASignatureLargeBuffer) {
  OEMCryptoResult sts;
  // Sign a Message
  vector<uint8_t> licenseRequest(kMaxMessageSize);
  GetRandBytes(licenseRequest.data(), licenseRequest.size());
  size_t signature_length = 0;
  uint8_t signature[500];

  sts = OEMCrypto_GenerateRSASignature(
      session_.session_id(), licenseRequest.data(), licenseRequest.size(),
      signature, &signature_length, kSign_RSASSA_PSS);

  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER, sts);
  ASSERT_NE(static_cast<size_t>(0), signature_length);
  ASSERT_GE(sizeof(signature), signature_length);

  sts = OEMCrypto_GenerateRSASignature(
      session_.session_id(), licenseRequest.data(), licenseRequest.size(),
      signature, &signature_length, kSign_RSASSA_PSS);

  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  // In the real world, the signature above would just have been used to contact
  // the license server to get this response.
  ASSERT_NO_FATAL_FAILURE(session_.PreparePublicKey(encoded_rsa_key_.data(),
                                                    encoded_rsa_key_.size()));
  ASSERT_NO_FATAL_FAILURE(session_.VerifyRSASignature(
      licenseRequest, signature, signature_length, kSign_RSASSA_PSS));
}

// Test DeriveKeysFromSessionKey using the maximum size for the HMAC context.
TEST_F(OEMCryptoUsesCertificate, GenerateDerivedKeysLargeBuffer) {
  vector<uint8_t> session_key;
  vector<uint8_t> enc_session_key;
  ASSERT_NO_FATAL_FAILURE(session_.PreparePublicKey(encoded_rsa_key_.data(),
                                                    encoded_rsa_key_.size()));
  ASSERT_TRUE(session_.GenerateRSASessionKey(&session_key, &enc_session_key));
  vector<uint8_t> mac_context(kMaxMessageSize);
  vector<uint8_t> enc_context(kMaxMessageSize);
  // Stripe the data so the two vectors are not identical, and not all zeroes.
  for (size_t i = 0; i < kMaxMessageSize; i++) {
    mac_context[i] = i % 0x100;
    enc_context[i] = (3 * i) % 0x100;
  }
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_DeriveKeysFromSessionKey(
                session_.session_id(),
                enc_session_key.data(), enc_session_key.size(),
                mac_context.data(), mac_context.size(),
                enc_context.data(), enc_context.size()));
}

// This test attempts to use alternate algorithms for loaded device certs.
class OEMCryptoLoadsCertificateAlternates : public OEMCryptoLoadsCertificate {
 protected:
  void DisallowForbiddenPadding(RSA_Padding_Scheme scheme, size_t size) {
    OEMCryptoResult sts;
    Session s;
    ASSERT_NO_FATAL_FAILURE(s.open());
    sts = OEMCrypto_LoadDeviceRSAKey(s.session_id(), wrapped_rsa_key_.data(),
                                     wrapped_rsa_key_.size());
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);

    // Sign a Message
    vector<uint8_t> licenseRequest(size);
    GetRandBytes(licenseRequest.data(), licenseRequest.size());
    size_t signature_length = 256;
    vector<uint8_t> signature(signature_length);
    sts = OEMCrypto_GenerateRSASignature(s.session_id(), licenseRequest.data(),
                                         licenseRequest.size(),
                                         signature.data(), &signature_length,
                                         scheme);
    // Allow OEMCrypto to request a full buffer.
    if (sts == OEMCrypto_ERROR_SHORT_BUFFER) {
      ASSERT_NE(static_cast<size_t>(0), signature_length);
      signature.assign(signature_length, 0);
      sts = OEMCrypto_GenerateRSASignature(s.session_id(),
                                           licenseRequest.data(),
                                           licenseRequest.size(),
                                           signature.data(), &signature_length,
                                           scheme);
    }

    EXPECT_NE(OEMCrypto_SUCCESS, sts)
        << "Signed with forbidden padding scheme=" << (int)scheme
        << ", size=" << (int)size;
    vector<uint8_t> zero(signature_length, 0);
    ASSERT_EQ(zero, signature);  // signature should not be computed.
  }

  void TestSignature(RSA_Padding_Scheme scheme, size_t size) {
    OEMCryptoResult sts;
    Session s;
    ASSERT_NO_FATAL_FAILURE(s.open());
    sts = OEMCrypto_LoadDeviceRSAKey(s.session_id(), wrapped_rsa_key_.data(),
                                     wrapped_rsa_key_.size());
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);

    vector<uint8_t> licenseRequest(size);
    GetRandBytes(licenseRequest.data(), licenseRequest.size());
    size_t signature_length = 0;
    sts = OEMCrypto_GenerateRSASignature(s.session_id(), licenseRequest.data(),
                                         licenseRequest.size(), NULL,
                                         &signature_length, scheme);
    ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER, sts);
    ASSERT_NE(static_cast<size_t>(0), signature_length);

    uint8_t* signature = new uint8_t[signature_length];
    sts = OEMCrypto_GenerateRSASignature(s.session_id(), licenseRequest.data(),
                                         licenseRequest.size(), signature,
                                         &signature_length, scheme);

    ASSERT_EQ(OEMCrypto_SUCCESS, sts)
        << "Failed to sign with padding scheme=" << (int)scheme
        << ", size=" << (int)size;
    ASSERT_NO_FATAL_FAILURE(s.PreparePublicKey(encoded_rsa_key_.data(),
                                               encoded_rsa_key_.size()));
    ASSERT_NO_FATAL_FAILURE(s.VerifyRSASignature(licenseRequest, signature,
                                                 signature_length, scheme));
    delete[] signature;
  }

  void DisallowDeriveKeys() {
    OEMCryptoResult sts;
    Session s;
    ASSERT_NO_FATAL_FAILURE(s.open());
    sts = OEMCrypto_LoadDeviceRSAKey(s.session_id(), wrapped_rsa_key_.data(),
                                     wrapped_rsa_key_.size());
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);
    s.GenerateNonce();
    vector<uint8_t> session_key;
    vector<uint8_t> enc_session_key;
    ASSERT_NO_FATAL_FAILURE(s.PreparePublicKey(encoded_rsa_key_.data(),
                                               encoded_rsa_key_.size()));
    ASSERT_TRUE(s.GenerateRSASessionKey(&session_key, &enc_session_key));
    vector<uint8_t> mac_context;
    vector<uint8_t> enc_context;
    s.FillDefaultContext(&mac_context, &enc_context);
    ASSERT_NE(OEMCrypto_SUCCESS,
              OEMCrypto_DeriveKeysFromSessionKey(
                  s.session_id(), enc_session_key.data(),
                  enc_session_key.size(), mac_context.data(),
                  mac_context.size(), enc_context.data(), enc_context.size()));
  }

  // If force is true, we assert that the key loads successfully.
  void LoadWithAllowedSchemes(uint32_t schemes, bool force) {
    CreateWrappedRSAKey(schemes, force);
    key_loaded_ = (wrapped_rsa_key_.size() > 0);
    if (force) ASSERT_TRUE(key_loaded_);
  }

  bool key_loaded_;
};

// The alternate padding is only required for cast receivers, but all devices
// should forbid the alternate padding for regular certificates.
TEST_F(OEMCryptoLoadsCertificateAlternates, DisallowForbiddenPaddingAPI09) {
  LoadWithAllowedSchemes(kSign_RSASSA_PSS, true);  // Use default padding scheme
  DisallowForbiddenPadding(kSign_PKCS1_Block1, 50);
}

// The alternate padding is only required for cast receivers, but if a device
// does load an alternate certificate, it should NOT use it for generating
// a license request signature.
TEST_F(OEMCryptoLoadsCertificateAlternates, TestSignaturePKCS1) {
  // Try to load an RSA key with alternative padding schemes.  This signing
  // scheme is used by cast receivers.
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, false);
  // If the device is a cast receiver, then this scheme is required.
  if (global_features.cast_receiver) ASSERT_TRUE(key_loaded_);
  // If the key loaded with no error, then we will verify that it is not used
  // for forbidden padding schemes.
  if (key_loaded_) {
    // The other padding scheme should fail.
    DisallowForbiddenPadding(kSign_RSASSA_PSS, 83);
    DisallowDeriveKeys();
    if (global_features.cast_receiver) {
      // A signature with a valid size should succeed.
      TestSignature(kSign_PKCS1_Block1, 83);
      TestSignature(kSign_PKCS1_Block1, 50);
    }
    // A signature with padding that is too big should fail.
    DisallowForbiddenPadding(kSign_PKCS1_Block1, 84);  // too big.
  }
}

// Try to load an RSA key with alternative padding schemes.  This key is allowed
// to sign with either padding scheme.  Devices are not required to support both
// padding schemes.
TEST_F(OEMCryptoLoadsCertificateAlternates, TestSignatureBoth) {
  LoadWithAllowedSchemes(kSign_RSASSA_PSS | kSign_PKCS1_Block1, false);
  // If the device loads this key, it should process it correctly.
  if (key_loaded_) {
    DisallowDeriveKeys();
    // A signature with padding that is too big should fail.
    DisallowForbiddenPadding(kSign_PKCS1_Block1, 84);
    if (global_features.cast_receiver) {
      TestSignature(kSign_RSASSA_PSS, 200);
      // A signature with a valid size should succeed.
      TestSignature(kSign_PKCS1_Block1, 83);
      TestSignature(kSign_PKCS1_Block1, 50);
    }
  }
}

// This test verifies RSA signing with the alternate padding scheme used by
// Android cast receivers, PKCS1 Block 1.  These tests are not required for
// other devices, and should be filtered out by DeviceFeatures::Initialize for
// those devices.
class OEMCryptoCastReceiverTest : public OEMCryptoLoadsCertificateAlternates {
 protected:
  vector<uint8_t> encode(uint8_t type, const vector<uint8_t>& substring) {
    vector<uint8_t> result;
    result.push_back(type);
    if (substring.size() < 0x80) {
      uint8_t length = substring.size();
      result.push_back(length);
    } else if (substring.size() < 0x100) {
      result.push_back(0x81);
      uint8_t length = substring.size();
      result.push_back(length);
    } else {
      result.push_back(0x82);
      uint16_t length = substring.size();
      result.push_back(length >> 8);
      result.push_back(length & 0xFF);
    }
    result.insert(result.end(), substring.begin(), substring.end());
    return result;
  }
  vector<uint8_t> concat(const vector<uint8_t>& a, const vector<uint8_t>& b) {
    vector<uint8_t> result = a;
    result.insert(result.end(), b.begin(), b.end());
    return result;
  }

  // This encodes the RSA key used in the PKCS#1 signing tests below.
  void BuildRSAKey() {
    vector<uint8_t> field_n =
        encode(0x02, wvcdm::a2b_hex("00"
                                    "df271fd25f8644496b0c81be4bd50297"
                                    "ef099b002a6fd67727eb449cea566ed6"
                                    "a3981a71312a141cabc9815c1209e320"
                                    "a25b32464e9999f18ca13a9fd3892558"
                                    "f9e0adefdd3650dd23a3f036d60fe398"
                                    "843706a40b0b8462c8bee3bce12f1f28"
                                    "60c2444cdc6a44476a75ff4aa24273cc"
                                    "be3bf80248465f8ff8c3a7f3367dfc0d"
                                    "f5b6509a4f82811cedd81cdaaa73c491"
                                    "da412170d544d4ba96b97f0afc806549"
                                    "8d3a49fd910992a1f0725be24f465cfe"
                                    "7e0eabf678996c50bc5e7524abf73f15"
                                    "e5bef7d518394e3138ce4944506aaaaf"
                                    "3f9b236dcab8fc00f87af596fdc3d9d6"
                                    "c75cd508362fae2cbeddcc4c7450b17b"
                                    "776c079ecca1f256351a43b97dbe2153"));
    vector<uint8_t> field_e = encode(0x02, wvcdm::a2b_hex("010001"));
    vector<uint8_t> field_d =
        encode(0x02, wvcdm::a2b_hex("5bd910257830dce17520b03441a51a8c"
                                    "ab94020ac6ecc252c808f3743c95b7c8"
                                    "3b8c8af1a5014346ebc4242cdfb5d718"
                                    "e30a733e71f291e4d473b61bfba6daca"
                                    "ed0a77bd1f0950ae3c91a8f901118825"
                                    "89e1d62765ee671e7baeea309f64d447"
                                    "bbcfa9ea12dce05e9ea8939bc5fe6108"
                                    "581279c982b308794b3448e7f7b95229"
                                    "2df88c80cb40142c4b5cf5f8ddaa0891"
                                    "678d610e582fcb880f0d707caf47d09a"
                                    "84e14ca65841e5a3abc5e9dba94075a9"
                                    "084341f0edad9b68e3b8e082b80b6e6e"
                                    "8a0547b44fb5061b6a9131603a5537dd"
                                    "abd01d8e863d8922e9aa3e4bfaea0b39"
                                    "d79283ad2cbc8a59cce7a6ecf4e4c81e"
                                    "d4c6591c807defd71ab06866bb5e7745"));
    vector<uint8_t> field_p =
        encode(0x02, wvcdm::a2b_hex("00"
                                    "f44f5e4246391f482b2f5296e3602eb3"
                                    "4aa136427710f7c0416d403fd69d4b29"
                                    "130cfebef34e885abdb1a8a0a5f0e9b5"
                                    "c33e1fc3bfc285b1ae17e40cc67a1913"
                                    "dd563719815ebaf8514c2a7aa0018e63"
                                    "b6c631dc315a46235716423d11ff5803"
                                    "4e610645703606919f5c7ce2660cd148"
                                    "bd9efc123d9c54b6705590d006cfcf3f"));
    vector<uint8_t> field_q =
        encode(0x02, wvcdm::a2b_hex("00"
                                    "e9d49841e0e0a6ad0d517857133e36dc"
                                    "72c1bdd90f9174b52e26570f373640f1"
                                    "c185e7ea8e2ed7f1e4ebb951f70a5802"
                                    "3633b0097aec67c6dcb800fc1a67f9bb"
                                    "0563610f08ebc8746ad129772136eb1d"
                                    "daf46436450d318332a84982fe5d28db"
                                    "e5b3e912407c3e0e03100d87d436ee40"
                                    "9eec1cf85e80aba079b2e6106b97bced"));
    vector<uint8_t> field_exp1 =
        encode(0x02, wvcdm::a2b_hex("00"
                                    "ed102acdb26871534d1c414ecad9a4d7"
                                    "32fe95b10eea370da62f05de2c393b1a"
                                    "633303ea741b6b3269c97f704b352702"
                                    "c9ae79922f7be8d10db67f026a8145de"
                                    "41b30c0a42bf923bac5f7504c248604b"
                                    "9faa57ed6b3246c6ba158e36c644f8b9"
                                    "548fcf4f07e054a56f768674054440bc"
                                    "0dcbbc9b528f64a01706e05b0b91106f"));
    vector<uint8_t> field_exp2 =
        encode(0x02, wvcdm::a2b_hex("6827924a85e88b55ba00f8219128bd37"
                                    "24c6b7d1dfe5629ef197925fecaff5ed"
                                    "b9cdf3a7befd8ea2e8dd3707138b3ff8"
                                    "7c3c39c57f439e562e2aa805a39d7cd7"
                                    "9966d2ece7845f1dbc16bee99999e4d0"
                                    "bf9eeca45fcda8a8500035fe6b5f03bc"
                                    "2f6d1bfc4d4d0a3723961af0cdce4a01"
                                    "eec82d7f5458ec19e71b90eeef7dff61"));
    vector<uint8_t> field_invq =
        encode(0x02, wvcdm::a2b_hex("57b73888d183a99a6307422277551a3d"
                                    "9e18adf06a91e8b55ceffef9077c8496"
                                    "948ecb3b16b78155cb2a3a57c119d379"
                                    "951c010aa635edcf62d84c5a122a8d67"
                                    "ab5fa9e5a4a8772a1e943bafc70ae3a4"
                                    "c1f0f3a4ddffaefd1892c8cb33bb0d0b"
                                    "9590e963a69110fb34db7b906fc4ba28"
                                    "36995aac7e527490ac952a02268a4f18"));

    // Header of rsa key is constant.
    encoded_rsa_key_ = wvcdm::a2b_hex(
        //  0x02 0x01 0x00 == integer, size 1 byte, value = 0 (field=version)
        "020100"
        //  0x30, sequence, size = d = 13 (field=pkeyalg)  AlgorithmIdentifier
        "300d"
        // 0x06 = object identifier. length = 9
        //  (this should be 1.2.840.113549.1.1.1) (field=algorithm)
        "0609"
        "2a"    // 1*0x40 + 2 = 42 = 0x2a.
        "8648"  // 840 = 0x348,  0x03 *2 + 0x80 + (0x48>>15) = 0x86.
        // 0x48 -> 0x48
        "86f70d"  // 113549 = 0x1668d -> (110 , 1110111, 0001101)
        // -> (0x80+0x06, 0x80+0x77, 0x0d)
        "01"  // 1
        "01"  // 1
        "01"  // 1
        "05"  // null object.  (field=parameter?)
        "00"  // size of null object
        );

    vector<uint8_t> pkey = wvcdm::a2b_hex("020100");  // integer, version = 0.
    pkey = concat(pkey, field_n);
    pkey = concat(pkey, field_e);
    pkey = concat(pkey, field_d);
    pkey = concat(pkey, field_p);
    pkey = concat(pkey, field_q);
    pkey = concat(pkey, field_exp1);
    pkey = concat(pkey, field_exp2);
    pkey = concat(pkey, field_invq);
    pkey = encode(0x30, pkey);
    pkey = encode(0x04, pkey);

    encoded_rsa_key_ = concat(encoded_rsa_key_, pkey);
    encoded_rsa_key_ = encode(0x30, encoded_rsa_key_);  // 0x30=sequence
  }

  // This is used to test a signature from the file pkcs1v15sign-vectors.txt.
  void TestSignature(RSA_Padding_Scheme scheme, const vector<uint8_t>& message,
                     const vector<uint8_t>& correct_signature) {
    OEMCryptoResult sts;
    Session s;
    ASSERT_NO_FATAL_FAILURE(s.open());
    sts = OEMCrypto_LoadDeviceRSAKey(s.session_id(), wrapped_rsa_key_.data(),
                                     wrapped_rsa_key_.size());
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);

    // The application will compute the SHA-1 Hash of the message, so this
    // test must do that also.
    uint8_t hash[SHA_DIGEST_LENGTH];
    if (!SHA1(message.data(), message.size(), hash)) {
      dump_boringssl_error();
      FAIL() << "boringssl error creating SHA1 hash.";
    }

    // The application will prepend the digest info to the hash.
    // SHA-1 digest info prefix = 0x30 0x21 0x30 ...
    vector<uint8_t> digest = wvcdm::a2b_hex("3021300906052b0e03021a05000414");
    digest.insert(digest.end(), hash, hash + SHA_DIGEST_LENGTH);

    // OEMCrypto will apply the padding, and encrypt to generate the signature.
    size_t signature_length = 0;
    sts = OEMCrypto_GenerateRSASignature(s.session_id(), digest.data(),
                                         digest.size(), NULL, &signature_length,
                                         scheme);
    ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER, sts);
    ASSERT_NE(static_cast<size_t>(0), signature_length);

    vector<uint8_t> signature(signature_length);
    sts = OEMCrypto_GenerateRSASignature(s.session_id(), digest.data(),
                                         digest.size(), signature.data(),
                                         &signature_length, scheme);

    ASSERT_EQ(OEMCrypto_SUCCESS, sts)
        << "Failed to sign with padding scheme=" << (int)scheme
        << ", size=" << (int)message.size();
    ASSERT_NO_FATAL_FAILURE(s.PreparePublicKey(encoded_rsa_key_.data(),
                                               encoded_rsa_key_.size()));

    // Verify that the signature matches the official test vector.
    ASSERT_EQ(correct_signature.size(), signature_length);
    signature.resize(signature_length);
    ASSERT_EQ(correct_signature, signature);

    // Also verify that our verification algorithm agrees.  This is not needed
    // to test OEMCrypto, but it does verify that this test is valid.
    ASSERT_NO_FATAL_FAILURE(s.VerifyRSASignature(
        digest, signature.data(), signature_length, scheme));
    ASSERT_NO_FATAL_FAILURE(s.VerifyRSASignature(
        digest, correct_signature.data(), correct_signature.size(), scheme));
  }
};

// CAST Receivers should report that they support cast certificates.
TEST_F(OEMCryptoCastReceiverTest, SupportsCertificatesAPI13) {
  ASSERT_NE(0u, OEMCrypto_Supports_RSA_CAST & OEMCrypto_SupportedCertificates());
}

// # PKCS#1 v1.5 Signature Example 15.1
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_1) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "f45d55f35551e975d6a8dc7ea9f48859"
      "3940cc75694a278f27e578a163d839b3"
      "4040841808cf9c58c9b8728bf5f9ce8e"
      "e811ea91714f47bab92d0f6d5a26fcfe"
      "ea6cd93b910c0a2c963e64eb1823f102"
      "753d41f0335910ad3a977104f1aaf6c3"
      "742716a9755d11b8eed690477f445c5d"
      "27208b2e284330fa3d301423fa7f2d08"
      "6e0ad0b892b9db544e456d3f0dab85d9"
      "53c12d340aa873eda727c8a649db7fa6"
      "3740e25e9af1533b307e61329993110e"
      "95194e039399c3824d24c51f22b26bde"
      "1024cd395958a2dfeb4816a6e8adedb5"
      "0b1f6b56d0b3060ff0f1c4cb0d0e001d"
      "d59d73be12");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "b75a5466b65d0f300ef53833f2175c8a"
      "347a3804fc63451dc902f0b71f908345"
      "9ed37a5179a3b723a53f1051642d7737"
      "4c4c6c8dbb1ca20525f5c9f32db77695"
      "3556da31290e22197482ceb69906c46a"
      "758fb0e7409ba801077d2a0a20eae7d1"
      "d6d392ab4957e86b76f0652d68b83988"
      "a78f26e11172ea609bf849fbbd78ad7e"
      "dce21de662a081368c040607cee29db0"
      "627227f44963ad171d2293b633a392e3"
      "31dca54fe3082752f43f63c161b447a4"
      "c65a6875670d5f6600fcc860a1caeb0a"
      "88f8fdec4e564398a5c46c87f68ce070"
      "01f6213abe0ab5625f87d19025f08d81"
      "dac7bd4586bc9382191f6d2880f6227e"
      "5df3eed21e7792d249480487f3655261");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}
// # PKCS#1 v1.5 Signature Example 15.2
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_2) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "c14b4c6075b2f9aad661def4ecfd3cb9"
      "33c623f4e63bf53410d2f016d1ab98e2"
      "729eccf8006cd8e08050737d95fdbf29"
      "6b66f5b9792a902936c4f7ac69f51453"
      "ce4369452dc22d96f037748114662000"
      "dd9cd3a5e179f4e0f81fa6a0311ca1ae"
      "e6519a0f63cec78d27bb726393fb7f1f"
      "88cde7c97f8a66cd66301281dac3f3a4"
      "33248c75d6c2dcd708b6a97b0a3f325e"
      "0b2964f8a5819e479b");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "afa7343462bea122cc149fca70abdae7"
      "9446677db5373666af7dc313015f4de7"
      "86e6e394946fad3cc0e2b02bedba5047"
      "fe9e2d7d099705e4a39f28683279cf0a"
      "c85c1530412242c0e918953be000e939"
      "cf3bf182525e199370fa7907eba69d5d"
      "b4631017c0e36df70379b5db8d4c695a"
      "979a8e6173224065d7dc15132ef28cd8"
      "22795163063b54c651141be86d36e367"
      "35bc61f31fca574e5309f3a3bbdf91ef"
      "f12b99e9cc1744f1ee9a1bd22c5bad96"
      "ad481929251f0343fd36bcf0acde7f11"
      "e5ad60977721202796fe061f9ada1fc4"
      "c8e00d6022a8357585ffe9fdd59331a2"
      "8c4aa3121588fb6cf68396d8ac054659"
      "9500c9708500a5972bd54f72cf8db0c8");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.3
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_3) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "d02371ad7ee48bbfdb2763de7a843b94"
      "08ce5eb5abf847ca3d735986df84e906"
      "0bdbcdd3a55ba55dde20d4761e1a21d2"
      "25c1a186f4ac4b3019d3adf78fe63346"
      "67f56f70c901a0a2700c6f0d56add719"
      "592dc88f6d2306c7009f6e7a635b4cb3"
      "a502dfe68ddc58d03be10a1170004fe7"
      "4dd3e46b82591ff75414f0c4a03e605e"
      "20524f2416f12eca589f111b75d639c6"
      "1baa80cafd05cf3500244a219ed9ced9"
      "f0b10297182b653b526f400f2953ba21"
      "4d5bcd47884132872ae90d4d6b1f4215"
      "39f9f34662a56dc0e7b4b923b6231e30"
      "d2676797817f7c337b5ac824ba93143b"
      "3381fa3dce0e6aebd38e67735187b1eb"
      "d95c02");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "3bac63f86e3b70271203106b9c79aabd"
      "9f477c56e4ee58a4fce5baf2cab4960f"
      "88391c9c23698be75c99aedf9e1abf17"
      "05be1dac33140adb48eb31f450bb9efe"
      "83b7b90db7f1576d33f40c1cba4b8d6b"
      "1d3323564b0f1774114fa7c08e6d1e20"
      "dd8fbba9b6ac7ad41e26b4568f4a8aac"
      "bfd178a8f8d2c9d5f5b88112935a8bc9"
      "ae32cda40b8d20375510735096536818"
      "ce2b2db71a9772c9b0dda09ae10152fa"
      "11466218d091b53d92543061b7294a55"
      "be82ff35d5c32fa233f05aaac7585030"
      "7ecf81383c111674397b1a1b9d3bf761"
      "2ccbe5bacd2b38f0a98397b24c83658f"
      "b6c0b4140ef11970c4630d44344e76ea"
      "ed74dcbee811dbf6575941f08a6523b8");
  TestSignature(kSign_PKCS1_Block1, message, signature);
};

// # PKCS#1 v1.5 Signature Example 15.4
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_4) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "29035584ab7e0226a9ec4b02e8dcf127"
      "2dc9a41d73e2820007b0f6e21feccd5b"
      "d9dbb9ef88cd6758769ee1f956da7ad1"
      "8441de6fab8386dbc693");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "28d8e3fcd5dddb21ffbd8df1630d7377"
      "aa2651e14cad1c0e43ccc52f907f946d"
      "66de7254e27a6c190eb022ee89ecf622"
      "4b097b71068cd60728a1aed64b80e545"
      "7bd3106dd91706c937c9795f2b36367f"
      "f153dc2519a8db9bdf2c807430c451de"
      "17bbcd0ce782b3e8f1024d90624dea7f"
      "1eedc7420b7e7caa6577cef43141a726"
      "4206580e44a167df5e41eea0e69a8054"
      "54c40eefc13f48e423d7a32d02ed42c0"
      "ab03d0a7cf70c5860ac92e03ee005b60"
      "ff3503424b98cc894568c7c56a023355"
      "1cebe588cf8b0167b7df13adcad82867"
      "6810499c704da7ae23414d69e3c0d2db"
      "5dcbc2613bc120421f9e3653c5a87672"
      "97643c7e0740de016355453d6c95ae72");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.5
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_5) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex("bda3a1c79059eae598308d3df609");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "a156176cb96777c7fb96105dbd913bc4"
      "f74054f6807c6008a1a956ea92c1f81c"
      "b897dc4b92ef9f4e40668dc7c556901a"
      "cb6cf269fe615b0fb72b30a513386923"
      "14b0e5878a88c2c7774bd16939b5abd8"
      "2b4429d67bd7ac8e5ea7fe924e20a6ec"
      "662291f2548d734f6634868b039aa5f9"
      "d4d906b2d0cb8585bf428547afc91c6e"
      "2052ddcd001c3ef8c8eefc3b6b2a82b6"
      "f9c88c56f2e2c3cb0be4b80da95eba37"
      "1d8b5f60f92538743ddbb5da2972c71f"
      "e7b9f1b790268a0e770fc5eb4d5dd852"
      "47d48ae2ec3f26255a3985520206a1f2"
      "68e483e9dbb1d5cab190917606de31e7"
      "c5182d8f151bf41dfeccaed7cde690b2"
      "1647106b490c729d54a8fe2802a6d126");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.6
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_6) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "c187915e4e87da81c08ed4356a0cceac"
      "1c4fb5c046b45281b387ec28f1abfd56"
      "7e546b236b37d01ae71d3b2834365d3d"
      "f380b75061b736b0130b070be58ae8a4"
      "6d12166361b613dbc47dfaeb4ca74645"
      "6c2e888385525cca9dd1c3c7a9ada76d"
      "6c");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "9cab74163608669f7555a333cf196fe3"
      "a0e9e5eb1a32d34bb5c85ff689aaab0e"
      "3e65668ed3b1153f94eb3d8be379b8ee"
      "f007c4a02c7071ce30d8bb341e58c620"
      "f73d37b4ecbf48be294f6c9e0ecb5e63"
      "fec41f120e5553dfa0ebebbb72640a95"
      "37badcb451330229d9f710f62e3ed8ec"
      "784e50ee1d9262b42671340011d7d098"
      "c6f2557b2131fa9bd0254636597e88ec"
      "b35a240ef0fd85957124df8080fee1e1"
      "49af939989e86b26c85a5881fae8673d"
      "9fd40800dd134eb9bdb6410f420b0aa9"
      "7b20efcf2eb0c807faeb83a3ccd9b51d"
      "4553e41dfc0df6ca80a1e81dc234bb83"
      "89dd195a38b42de4edc49d346478b9f1"
      "1f0557205f5b0bd7ffe9c850f396d7c4");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.7
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_7) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "abfa2ecb7d29bd5bcb9931ce2bad2f74"
      "383e95683cee11022f08e8e7d0b8fa05"
      "8bf9eb7eb5f98868b5bb1fb5c31ceda3"
      "a64f1a12cdf20fcd0e5a246d7a1773d8"
      "dba0e3b277545babe58f2b96e3f4edc1"
      "8eabf5cd2a560fca75fe96e07d859def"
      "b2564f3a34f16f11e91b3a717b41af53"
      "f6605323001aa406c6");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "c4b437bcf703f352e1faf74eb9622039"
      "426b5672caf2a7b381c6c4f0191e7e4a"
      "98f0eebcd6f41784c2537ff0f99e7498"
      "2c87201bfbc65eae832db71d16dacadb"
      "0977e5c504679e40be0f9db06ffd848d"
      "d2e5c38a7ec021e7f68c47dfd38cc354"
      "493d5339b4595a5bf31e3f8f13816807"
      "373df6ad0dc7e731e51ad19eb4754b13"
      "4485842fe709d378444d8e36b1724a4f"
      "da21cafee653ab80747f7952ee804dea"
      "b1039d84139945bbf4be82008753f3c5"
      "4c7821a1d241f42179c794ef7042bbf9"
      "955656222e45c34369a384697b6ae742"
      "e18fa5ca7abad27d9fe71052e3310d0f"
      "52c8d12ea33bf053a300f4afc4f098df"
      "4e6d886779d64594d369158fdbc1f694");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.8
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_8) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "df4044a89a83e9fcbf1262540ae3038b"
      "bc90f2b2628bf2a4467ac67722d8546b"
      "3a71cb0ea41669d5b4d61859c1b4e47c"
      "ecc5933f757ec86db0644e311812d00f"
      "b802f03400639c0e364dae5aebc5791b"
      "c655762361bc43c53d3c7886768f7968"
      "c1c544c6f79f7be820c7e2bd2f9d73e6"
      "2ded6d2e937e6a6daef90ee37a1a52a5"
      "4f00e31addd64894cf4c02e16099e29f"
      "9eb7f1a7bb7f84c47a2b594813be02a1"
      "7b7fc43b34c22c91925264126c89f86b"
      "b4d87f3ef131296c53a308e0331dac8b"
      "af3b63422266ecef2b90781535dbda41"
      "cbd0cf22a8cbfb532ec68fc6afb2ac06");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "1414b38567ae6d973ede4a06842dcc0e"
      "0559b19e65a4889bdbabd0fd02806829"
      "13bacd5dc2f01b30bb19eb810b7d9ded"
      "32b284f147bbe771c930c6052aa73413"
      "90a849f81da9cd11e5eccf246dbae95f"
      "a95828e9ae0ca3550325326deef9f495"
      "30ba441bed4ac29c029c9a2736b1a419"
      "0b85084ad150426b46d7f85bd702f48d"
      "ac5f71330bc423a766c65cc1dcab20d3"
      "d3bba72b63b3ef8244d42f157cb7e3a8"
      "ba5c05272c64cc1ad21a13493c3911f6"
      "0b4e9f4ecc9900eb056ee59d6fe4b8ff"
      "6e8048ccc0f38f2836fd3dfe91bf4a38"
      "6e1ecc2c32839f0ca4d1b27a568fa940"
      "dd64ad16bd0125d0348e383085f08894"
      "861ca18987227d37b42b584a8357cb04");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.9
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_9) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "ea941ff06f86c226927fcf0e3b11b087"
      "2676170c1bfc33bda8e265c77771f9d0"
      "850164a5eecbcc5ce827fbfa07c85214"
      "796d8127e8caa81894ea61ceb1449e72"
      "fea0a4c943b2da6d9b105fe053b9039a"
      "9cc53d420b7539fab2239c6b51d17e69"
      "4c957d4b0f0984461879a0759c4401be"
      "ecd4c606a0afbd7a076f50a2dfc2807f"
      "24f1919baa7746d3a64e268ed3f5f8e6"
      "da83a2a5c9152f837cb07812bd5ba7d3"
      "a07985de88113c1796e9b466ec299c5a"
      "c1059e27f09415");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "ceeb84ccb4e9099265650721eea0e8ec"
      "89ca25bd354d4f64564967be9d4b08b3"
      "f1c018539c9d371cf8961f2291fbe0dc"
      "2f2f95fea47b639f1e12f4bc381cef0c"
      "2b7a7b95c3adf27605b7f63998c3cbad"
      "542808c3822e064d4ad14093679e6e01"
      "418a6d5c059684cd56e34ed65ab605b8"
      "de4fcfa640474a54a8251bbb7326a42d"
      "08585cfcfc956769b15b6d7fdf7da84f"
      "81976eaa41d692380ff10eaecfe0a579"
      "682909b5521fade854d797b8a0345b9a"
      "864e0588f6caddbf65f177998e180d1f"
      "102443e6dca53a94823caa9c3b35f322"
      "583c703af67476159ec7ec93d1769b30"
      "0af0e7157dc298c6cd2dee2262f8cddc"
      "10f11e01741471bbfd6518a175734575");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.10
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_10) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "d8b81645c13cd7ecf5d00ed2c91b9acd"
      "46c15568e5303c4a9775ede76b48403d"
      "6be56c05b6b1cf77c6e75de096c5cb35"
      "51cb6fa964f3c879cf589d28e1da2f9d"
      "ec");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "2745074ca97175d992e2b44791c323c5"
      "7167165cdd8da579cdef4686b9bb404b"
      "d36a56504eb1fd770f60bfa188a7b24b"
      "0c91e881c24e35b04dc4dd4ce38566bc"
      "c9ce54f49a175fc9d0b22522d9579047"
      "f9ed42eca83f764a10163997947e7d2b"
      "52ff08980e7e7c2257937b23f3d279d4"
      "cd17d6f495546373d983d536efd7d1b6"
      "7181ca2cb50ac616c5c7abfbb9260b91"
      "b1a38e47242001ff452f8de10ca6eaea"
      "dcaf9edc28956f28a711291fc9a80878"
      "b8ba4cfe25b8281cb80bc9cd6d2bd182"
      "5246eebe252d9957ef93707352084e6d"
      "36d423551bf266a85340fb4a6af37088"
      "0aab07153d01f48d086df0bfbec05e7b"
      "443b97e71718970e2f4bf62023e95b67");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.11
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_11) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "e5739b6c14c92d510d95b826933337ff"
      "0d24ef721ac4ef64c2bad264be8b44ef"
      "a1516e08a27eb6b611d3301df0062dae"
      "fc73a8c0d92e2c521facbc7b26473876"
      "7ea6fc97d588a0baf6ce50adf79e600b"
      "d29e345fcb1dba71ac5c0289023fe4a8"
      "2b46a5407719197d2e958e3531fd54ae"
      "f903aabb4355f88318994ed3c3dd62f4"
      "20a7");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "be40a5fb94f113e1b3eff6b6a33986f2"
      "02e363f07483b792e68dfa5554df0466"
      "cc32150950783b4d968b639a04fd2fb9"
      "7f6eb967021f5adccb9fca95acc8f2cd"
      "885a380b0a4e82bc760764dbab88c1e6"
      "c0255caa94f232199d6f597cc9145b00"
      "e3d4ba346b559a8833ad1516ad5163f0"
      "16af6a59831c82ea13c8224d84d0765a"
      "9d12384da460a8531b4c407e04f4f350"
      "709eb9f08f5b220ffb45abf6b75d1579"
      "fd3f1eb55fc75b00af8ba3b087827fe9"
      "ae9fb4f6c5fa63031fe582852fe2834f"
      "9c89bff53e2552216bc7c1d4a3d5dc2b"
      "a6955cd9b17d1363e7fee8ed7629753f"
      "f3125edd48521ae3b9b03217f4496d0d"
      "8ede57acbc5bd4deae74a56f86671de2");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.12
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_12) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "7af42835917a88d6b3c6716ba2f5b0d5"
      "b20bd4e2e6e574e06af1eef7c81131be"
      "22bf8128b9cbc6ec00275ba80294a5d1"
      "172d0824a79e8fdd830183e4c00b9678"
      "2867b1227fea249aad32ffc5fe007bc5"
      "1f21792f728deda8b5708aa99cabab20"
      "a4aa783ed86f0f27b5d563f42e07158c"
      "ea72d097aa6887ec411dd012912a5e03"
      "2bbfa678507144bcc95f39b58be7bfd1"
      "759adb9a91fa1d6d8226a8343a8b849d"
      "ae76f7b98224d59e28f781f13ece605f"
      "84f6c90bae5f8cf378816f4020a7dda1"
      "bed90c92a23634d203fac3fcd86d68d3"
      "182a7d9ccabe7b0795f5c655e9acc4e3"
      "ec185140d10cef053464ab175c83bd83"
      "935e3dabaf3462eebe63d15f573d269a");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "4e78c5902b807914d12fa537ae6871c8"
      "6db8021e55d1adb8eb0ccf1b8f36ab7d"
      "ad1f682e947a627072f03e627371781d"
      "33221d174abe460dbd88560c22f69011"
      "6e2fbbe6e964363a3e5283bb5d946ef1"
      "c0047eba038c756c40be7923055809b0"
      "e9f34a03a58815ebdde767931f018f6f"
      "1878f2ef4f47dd374051dd48685ded6e"
      "fb3ea8021f44be1d7d149398f98ea9c0"
      "8d62888ebb56192d17747b6b8e170954"
      "31f125a8a8e9962aa31c285264e08fb2"
      "1aac336ce6c38aa375e42bc92ab0ab91"
      "038431e1f92c39d2af5ded7e43bc151e"
      "6ebea4c3e2583af3437e82c43c5e3b5b"
      "07cf0359683d2298e35948ed806c063c"
      "606ea178150b1efc15856934c7255cfe");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.13
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_13) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "ebaef3f9f23bdfe5fa6b8af4c208c189"
      "f2251bf32f5f137b9de4406378686b3f"
      "0721f62d24cb8688d6fc41a27cbae21d"
      "30e429feacc7111941c277");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "c48dbef507114f03c95fafbeb4df1bfa"
      "88e0184a33cc4f8a9a1035ff7f822a5e"
      "38cda18723915ff078244429e0f6081c"
      "14fd83331fa65c6ba7bb9a12dbf66223"
      "74cd0ca57de3774e2bd7ae823677d061"
      "d53ae9c4040d2da7ef7014f3bbdc95a3"
      "61a43855c8ce9b97ecabce174d926285"
      "142b534a3087f9f4ef74511ec742b0d5"
      "685603faf403b5072b985df46adf2d25"
      "29a02d40711e2190917052371b79b749"
      "b83abf0ae29486c3f2f62477b2bd362b"
      "039c013c0c5076ef520dbb405f42cee9"
      "5425c373a975e1cdd032c49622c85079"
      "b09e88dab2b13969ef7a723973781040"
      "459f57d5013638483de2d91cb3c490da"
      "81c46de6cd76ea8a0c8f6fe331712d24");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.14
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_14) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "c5a2711278761dfcdd4f0c99e6f5619d"
      "6c48b5d4c1a80982faa6b4cf1cf7a60f"
      "f327abef93c801429efde08640858146"
      "1056acc33f3d04f5ada21216cacd5fd1"
      "f9ed83203e0e2fe6138e3eae8424e591"
      "5a083f3f7ab76052c8be55ae882d6ec1"
      "482b1e45c5dae9f41015405327022ec3"
      "2f0ea2429763b255043b1958ee3cf6d6"
      "3983596eb385844f8528cc9a9865835d"
      "c5113c02b80d0fca68aa25e72bcaaeb3"
      "cf9d79d84f984fd417");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "6bd5257aa06611fb4660087cb4bc4a9e"
      "449159d31652bd980844daf3b1c7b353"
      "f8e56142f7ea9857433b18573b4deede"
      "818a93b0290297783f1a2f23cbc72797"
      "a672537f01f62484cd4162c3214b9ac6"
      "28224c5de01f32bb9b76b27354f2b151"
      "d0e8c4213e4615ad0bc71f515e300d6a"
      "64c6743411fffde8e5ff190e54923043"
      "126ecfc4c4539022668fb675f25c07e2"
      "0099ee315b98d6afec4b1a9a93dc3349"
      "6a15bd6fde1663a7d49b9f1e639d3866"
      "4b37a010b1f35e658682d9cd63e57de0"
      "f15e8bdd096558f07ec0caa218a8c06f"
      "4788453940287c9d34b6d40a3f09bf77"
      "99fe98ae4eb49f3ff41c5040a50cefc9"
      "bdf2394b749cf164480df1ab6880273b");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.15
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_15) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "9bf8aa253b872ea77a7e23476be26b23"
      "29578cf6ac9ea2805b357f6fc3ad130d"
      "baeb3d869a13cce7a808bbbbc969857e"
      "03945c7bb61df1b5c2589b8e046c2a5d"
      "7e4057b1a74f24c711216364288529ec"
      "9570f25197213be1f5c2e596f8bf8b2c"
      "f3cb38aa56ffe5e31df7395820e94ecf"
      "3b1189a965dcf9a9cb4298d3c88b2923"
      "c19fc6bc34aacecad4e0931a7c4e5d73"
      "dc86dfa798a8476d82463eefaa90a8a9"
      "192ab08b23088dd58e1280f7d72e4548"
      "396baac112252dd5c5346adb2004a2f7"
      "101ccc899cc7fafae8bbe295738896a5"
      "b2012285014ef6");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "27f7f4da9bd610106ef57d32383a448a"
      "8a6245c83dc1309c6d770d357ba89e73"
      "f2ad0832062eb0fe0ac915575bcd6b8b"
      "cadb4e2ba6fa9da73a59175152b2d4fe"
      "72b070c9b7379e50000e55e6c269f665"
      "8c937972797d3add69f130e34b85bdec"
      "9f3a9b392202d6f3e430d09caca82277"
      "59ab825f7012d2ff4b5b62c8504dbad8"
      "55c05edd5cab5a4cccdc67f01dd6517c"
      "7d41c43e2a4957aff19db6f18b17859a"
      "f0bc84ab67146ec1a4a60a17d7e05f8b"
      "4f9ced6ad10908d8d78f7fc88b76adc8"
      "290f87daf2a7be10ae408521395d54ed"
      "2556fb7661854a730ce3d82c71a8d493"
      "ec49a378ac8a3c74439f7cc555ba13f8"
      "59070890ee18ff658fa4d741969d70a5");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.16
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_16) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "32474830e2203754c8bf0681dc4f842a"
      "fe360930378616c108e833656e5640c8"
      "6856885bb05d1eb9438efede679263de"
      "07cb39553f6a25e006b0a52311a063ca"
      "088266d2564ff6490c46b5609818548f"
      "88764dad34a25e3a85d575023f0b9e66"
      "5048a03c350579a9d32446c7bb96cc92"
      "e065ab94d3c8952e8df68ef0d9fa456b"
      "3a06bb80e3bbc4b28e6a94b6d0ff7696"
      "a64efe05e735fea025d7bdbc4139f3a3"
      "b546075cba7efa947374d3f0ac80a68d"
      "765f5df6210bca069a2d88647af7ea04"
      "2dac690cb57378ec0777614fb8b65ff4"
      "53ca6b7dce6098451a2f8c0da9bfecf1"
      "fdf391bbaa4e2a91ca18a1121a7523a2"
      "abd42514f489e8");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "6917437257c22ccb5403290c3dee82d9"
      "cf7550b31bd31c51bd57bfd35d452ab4"
      "db7c4be6b2e25ac9a59a1d2a7feb627f"
      "0afd4976b3003cc9cffd8896505ec382"
      "f265104d4cf8c932fa9fe86e00870795"
      "9912389da4b2d6b369b36a5e72e29d24"
      "c9a98c9d31a3ab44e643e6941266a47a"
      "45e3446ce8776abe241a8f5fc6423b24"
      "b1ff250dc2c3a8172353561077e850a7"
      "69b25f0325dac88965a3b9b472c494e9"
      "5f719b4eac332caa7a65c7dfe46d9aa7"
      "e6e00f525f303dd63ab7919218901868"
      "f9337f8cd26aafe6f33b7fb2c98810af"
      "19f7fcb282ba1577912c1d368975fd5d"
      "440b86e10c199715fa0b6f4250b53373"
      "2d0befe1545150fc47b876de09b00a94");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.17
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_17) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "008e59505eafb550aae5e845584cebb0"
      "0b6de1733e9f95d42c882a5bbeb5ce1c"
      "57e119e7c0d4daca9f1ff7870217f7cf"
      "d8a6b373977cac9cab8e71e420");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "922503b673ee5f3e691e1ca85e9ff417"
      "3cf72b05ac2c131da5603593e3bc259c"
      "94c1f7d3a06a5b9891bf113fa39e59ff"
      "7c1ed6465e908049cb89e4e125cd37d2"
      "ffd9227a41b4a0a19c0a44fbbf3de55b"
      "ab802087a3bb8d4ff668ee6bbb8ad89e"
      "6857a79a9c72781990dfcf92cd519404"
      "c950f13d1143c3184f1d250c90e17ac6"
      "ce36163b9895627ad6ffec1422441f55"
      "e4499dba9be89546ae8bc63cca01dd08"
      "463ae7f1fce3d893996938778c1812e6"
      "74ad9c309c5acca3fde44e7dd8695993"
      "e9c1fa87acda99ece5c8499e468957ad"
      "66359bf12a51adbe78d3a213b449bf0b"
      "5f8d4d496acf03d3033b7ccd196bc22f"
      "68fb7bef4f697c5ea2b35062f48a36dd");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.18
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_18) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "6abc54cf8d1dff1f53b17d8160368878"
      "a8788cc6d22fa5c2258c88e660b09a89"
      "33f9f2c0504ddadc21f6e75e0b833beb"
      "555229dee656b9047b92f62e76b8ffcc"
      "60dab06b80");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "0b6daf42f7a862147e417493c2c401ef"
      "ae32636ab4cbd44192bbf5f195b50ae0"
      "96a475a1614f0a9fa8f7a026cb46c650"
      "6e518e33d83e56477a875aca8c7e714c"
      "e1bdbd61ef5d535239b33f2bfdd61771"
      "bab62776d78171a1423cea8731f82e60"
      "766d6454265620b15f5c5a584f55f95b"
      "802fe78c574ed5dacfc831f3cf2b0502"
      "c0b298f25ccf11f973b31f85e4744219"
      "85f3cff702df3946ef0a6605682111b2"
      "f55b1f8ab0d2ea3a683c69985ead93ed"
      "449ea48f0358ddf70802cb41de2fd83f"
      "3c808082d84936948e0c84a131b49278"
      "27460527bb5cd24bfab7b48e071b2417"
      "1930f99763272f9797bcb76f1d248157"
      "5558fcf260b1f0e554ebb3df3cfcb958");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.19
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_19) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "af2d78152cf10efe01d274f217b177f6"
      "b01b5e749f1567715da324859cd3dd88"
      "db848ec79f48dbba7b6f1d33111ef31b"
      "64899e7391c2bffd69f49025cf201fc5"
      "85dbd1542c1c778a2ce7a7ee108a309f"
      "eca26d133a5ffedc4e869dcd7656596a"
      "c8427ea3ef6e3fd78fe99d8ddc71d839"
      "f6786e0da6e786bd62b3a4f19b891a56"
      "157a554ec2a2b39e25a1d7c7d37321c7"
      "a1d946cf4fbe758d9276f08563449d67"
      "414a2c030f4251cfe2213d04a5410637"
      "87");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "209c61157857387b71e24bf3dd564145"
      "50503bec180ff53bdd9bac062a2d4995"
      "09bf991281b79527df9136615b7a6d9d"
      "b3a103b535e0202a2caca197a7b74e53"
      "56f3dd595b49acfd9d30049a98ca88f6"
      "25bca1d5f22a392d8a749efb6eed9b78"
      "21d3110ac0d244199ecb4aa3d735a83a"
      "2e8893c6bf8581383ccaee834635b7fa"
      "1faffa45b13d15c1da33af71e89303d6"
      "8090ff62ee615fdf5a84d120711da53c"
      "2889198ab38317a9734ab27d67924cea"
      "74156ff99bef9876bb5c339e93745283"
      "e1b34e072226b88045e017e9f05b2a8c"
      "416740258e223b2690027491732273f3"
      "229d9ef2b1b3807e321018920ad3e53d"
      "ae47e6d9395c184b93a374c671faa2ce");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// # PKCS#1 v1.5 Signature Example 15.20
TEST_F(OEMCryptoCastReceiverTest, TestSignaturePKCS1_15_20) {
  BuildRSAKey();
  LoadWithAllowedSchemes(kSign_PKCS1_Block1, true);
  vector<uint8_t> message = wvcdm::a2b_hex(
      "40ee992458d6f61486d25676a96dd2cb"
      "93a37f04b178482f2b186cf88215270d"
      "ba29d786d774b0c5e78c7f6e56a956e7"
      "f73950a2b0c0c10a08dbcd67e5b210bb"
      "21c58e2767d44f7dd4014e3966143bf7"
      "e3d66ff0c09be4c55f93b39994b8518d"
      "9c1d76d5b47374dea08f157d57d70634"
      "978f3856e0e5b481afbbdb5a3ac48d48"
      "4be92c93de229178354c2de526e9c65a"
      "31ede1ef68cb6398d7911684fec0babc"
      "3a781a66660783506974d0e14825101c"
      "3bfaea");
  vector<uint8_t> signature = wvcdm::a2b_hex(
      "927502b824afc42513ca6570de338b8a"
      "64c3a85eb828d3193624f27e8b1029c5"
      "5c119c9733b18f5849b3500918bcc005"
      "51d9a8fdf53a97749fa8dc480d6fe974"
      "2a5871f973926528972a1af49e3925b0"
      "adf14a842719b4a5a2d89fa9c0b6605d"
      "212bed1e6723b93406ad30e86829a5c7"
      "19b890b389306dc5506486ee2f36a8df"
      "e0a96af678c9cbd6aff397ca200e3edc"
      "1e36bd2f08b31d540c0cb282a9559e4a"
      "dd4fc9e6492eed0ccbd3a6982e5faa2d"
      "dd17be47417c80b4e5452d31f72401a0"
      "42325109544d954c01939079d409a5c3"
      "78d7512dfc2d2a71efcc3432a765d1c6"
      "a52cfce899cd79b15b4fc3723641ef6b"
      "d00acc10407e5df58dd1c3c5c559a506");
  TestSignature(kSign_PKCS1_Block1, message, signature);
}

// This class is for testing the generic crypto functionality.
class GenericCryptoTest : public OEMCryptoSessionTests {
 protected:
  // buffer_size_ must be a multiple of encryption block size, 16.  We'll use a
  // reasonable number of blocks for most of the tests.
  GenericCryptoTest() : buffer_size_(160) {}

  void SetUp() override {
    OEMCryptoSessionTests::SetUp();
    ASSERT_NO_FATAL_FAILURE(session_.open());
    ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&session_));
    ASSERT_NO_FATAL_FAILURE(MakeFourKeys());
  }

  void TearDown() override {
    session_.close();
    OEMCryptoSessionTests::TearDown();
  }

  // This makes four keys, one for each of the generic operations that we want
  // to test.
  void MakeFourKeys(uint32_t duration = kDuration, uint32_t control = 0,
                    uint32_t nonce = 0, const std::string& pst = "") {
    ASSERT_NO_FATAL_FAILURE(
        session_.FillSimpleMessage(duration, control, nonce, pst));
    session_.license().keys[0].control.control_bits |=
        htonl(wvoec::kControlAllowEncrypt);
    session_.license().keys[1].control.control_bits |=
        htonl(wvoec::kControlAllowDecrypt);
    session_.license().keys[2].control.control_bits |=
        htonl(wvoec::kControlAllowSign);
    session_.license().keys[3].control.control_bits |=
        htonl(wvoec::kControlAllowVerify);

    session_.license().keys[2].key_data_length = wvoec::MAC_KEY_SIZE;
    session_.license().keys[3].key_data_length = wvoec::MAC_KEY_SIZE;

    clear_buffer_.assign(buffer_size_, 0);
    for (size_t i = 0; i < clear_buffer_.size(); i++) {
      clear_buffer_[i] = 1 + i % 250;
    }
    for (size_t i = 0; i < wvoec::KEY_IV_SIZE; i++) {
      iv_[i] = i;
    }
  }

  void EncryptAndLoadKeys() {
    ASSERT_NO_FATAL_FAILURE(session_.EncryptAndSign());
    session_.LoadTestKeys();
  }

  // Encrypt the buffer with the specified key made in MakeFourKeys.
  void EncryptBuffer(unsigned int key_index, const vector<uint8_t>& in_buffer,
                     vector<uint8_t>* out_buffer) {
    AES_KEY aes_key;
    ASSERT_EQ(0,
              AES_set_encrypt_key(session_.license().keys[key_index].key_data,
                                  AES_BLOCK_SIZE * 8, &aes_key));
    uint8_t iv_buffer[wvoec::KEY_IV_SIZE];
    memcpy(iv_buffer, iv_, wvoec::KEY_IV_SIZE);
    out_buffer->resize(in_buffer.size());
    ASSERT_GT(in_buffer.size(), 0u);
    ASSERT_EQ(0u, in_buffer.size() % AES_BLOCK_SIZE);
    AES_cbc_encrypt(in_buffer.data(), out_buffer->data(), in_buffer.size(),
                    &aes_key, iv_buffer, AES_ENCRYPT);
  }

  // Sign the buffer with the specified key.
  void SignBuffer(unsigned int key_index, const vector<uint8_t>& in_buffer,
                  vector<uint8_t>* signature) {
    unsigned int md_len = SHA256_DIGEST_LENGTH;
    signature->resize(SHA256_DIGEST_LENGTH);
    HMAC(EVP_sha256(), session_.license().keys[key_index].key_data,
         wvoec::MAC_KEY_SIZE, in_buffer.data(), in_buffer.size(),
         signature->data(), &md_len);
  }

  // This asks OEMCrypto to encrypt with the specified key, and expects a
  // failure.
  void BadEncrypt(unsigned int key_index, OEMCrypto_Algorithm algorithm,
                  size_t buffer_length) {
    OEMCryptoResult sts;
    vector<uint8_t> expected_encrypted;
    EncryptBuffer(key_index, clear_buffer_, &expected_encrypted);
    sts = OEMCrypto_SelectKey(session_.session_id(),
                              session_.license().keys[key_index].key_id,
                              session_.license().keys[key_index].key_id_length,
                              OEMCrypto_CipherMode_CTR);
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);
    vector<uint8_t> encrypted(buffer_length);
    sts =
        OEMCrypto_Generic_Encrypt(session_.session_id(), clear_buffer_.data(),
                                  buffer_length, iv_, algorithm,
                                  encrypted.data());
    EXPECT_NE(OEMCrypto_SUCCESS, sts);
    expected_encrypted.resize(buffer_length);
    EXPECT_NE(encrypted, expected_encrypted);
  }

  // This asks OEMCrypto to decrypt with the specified key, and expects a
  // failure.
  void BadDecrypt(unsigned int key_index, OEMCrypto_Algorithm algorithm,
                  size_t buffer_length) {
    OEMCryptoResult sts;
    vector<uint8_t> encrypted;
    EncryptBuffer(key_index, clear_buffer_, &encrypted);
    sts = OEMCrypto_SelectKey(session_.session_id(),
                              session_.license().keys[key_index].key_id,
                              session_.license().keys[key_index].key_id_length,
                              OEMCrypto_CipherMode_CTR);
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);
    vector<uint8_t> resultant(encrypted.size());
    sts =
        OEMCrypto_Generic_Decrypt(session_.session_id(), encrypted.data(),
                                  buffer_length, iv_, algorithm,
                                  resultant.data());
    EXPECT_NE(OEMCrypto_SUCCESS, sts);
    EXPECT_NE(clear_buffer_, resultant);
  }

  // This asks OEMCrypto to sign with the specified key, and expects a
  // failure.
  void BadSign(unsigned int key_index, OEMCrypto_Algorithm algorithm) {
    OEMCryptoResult sts;
    vector<uint8_t> expected_signature;
    SignBuffer(key_index, clear_buffer_, &expected_signature);

    sts = OEMCrypto_SelectKey(session_.session_id(),
                              session_.license().keys[key_index].key_id,
                              session_.license().keys[key_index].key_id_length,
                              OEMCrypto_CipherMode_CTR);
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);
    size_t signature_length = (size_t)SHA256_DIGEST_LENGTH;
    vector<uint8_t> signature(SHA256_DIGEST_LENGTH);
    sts = OEMCrypto_Generic_Sign(session_.session_id(), clear_buffer_.data(),
                                 clear_buffer_.size(), algorithm,
                                 signature.data(), &signature_length);
    EXPECT_NE(OEMCrypto_SUCCESS, sts);
    EXPECT_NE(signature, expected_signature);
  }

  // This asks OEMCrypto to verify a signature with the specified key, and
  // expects a failure.
  void BadVerify(unsigned int key_index, OEMCrypto_Algorithm algorithm,
                 size_t signature_size, bool alter_data) {
    OEMCryptoResult sts;
    vector<uint8_t> signature;
    SignBuffer(key_index, clear_buffer_, &signature);
    if (alter_data) {
      signature[0] ^= 42;
    }

    sts = OEMCrypto_SelectKey(session_.session_id(),
                              session_.license().keys[key_index].key_id,
                              session_.license().keys[key_index].key_id_length,
                              OEMCrypto_CipherMode_CTR);
    ASSERT_EQ(OEMCrypto_SUCCESS, sts);
    sts = OEMCrypto_Generic_Verify(session_.session_id(), clear_buffer_.data(),
                                   clear_buffer_.size(), algorithm,
                                   signature.data(), signature_size);
    EXPECT_NE(OEMCrypto_SUCCESS, sts);
  }

  // This must be a multiple of encryption block size.
  size_t buffer_size_;
  vector<uint8_t> clear_buffer_;
  vector<uint8_t> encrypted_buffer_;
  uint8_t iv_[wvoec::KEY_IV_SIZE];
  Session session_;
};

TEST_F(GenericCryptoTest, GenericKeyLoad) { EncryptAndLoadKeys(); }

// Test that the Generic_Encrypt function works correctly.
TEST_F(GenericCryptoTest, GenericKeyEncrypt) {
  EncryptAndLoadKeys();
  unsigned int key_index = 0;
  vector<uint8_t> expected_encrypted;
  EncryptBuffer(key_index, clear_buffer_, &expected_encrypted);
  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_SelectKey(session_.session_id(),
                          session_.license().keys[key_index].key_id,
                          session_.license().keys[key_index].key_id_length,
                          OEMCrypto_CipherMode_CTR));
  vector<uint8_t> encrypted(clear_buffer_.size());
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_Generic_Encrypt(
                session_.session_id(), clear_buffer_.data(),
                clear_buffer_.size(), iv_, OEMCrypto_AES_CBC_128_NO_PADDING,
                encrypted.data()));
  ASSERT_EQ(expected_encrypted, encrypted);
}

// Test that the Generic_Encrypt function fails when not allowed.
TEST_F(GenericCryptoTest, GenericKeyBadEncrypt) {
  EncryptAndLoadKeys();
  BadEncrypt(0, OEMCrypto_HMAC_SHA256, buffer_size_);
  // The buffer size must be a multiple of 16, so subtracting 10 is bad.
  BadEncrypt(0, OEMCrypto_AES_CBC_128_NO_PADDING, buffer_size_ - 10);
  BadEncrypt(1, OEMCrypto_AES_CBC_128_NO_PADDING, buffer_size_);
  BadEncrypt(2, OEMCrypto_AES_CBC_128_NO_PADDING, buffer_size_);
  BadEncrypt(3, OEMCrypto_AES_CBC_128_NO_PADDING, buffer_size_);
}

// Test that the Generic_Encrypt works if the input and output buffers are the
// same.
TEST_F(GenericCryptoTest, GenericKeyEncryptSameBufferAPI12) {
  EncryptAndLoadKeys();
  unsigned int key_index = 0;
  vector<uint8_t> expected_encrypted;
  EncryptBuffer(key_index, clear_buffer_, &expected_encrypted);
  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_SelectKey(session_.session_id(),
                          session_.license().keys[key_index].key_id,
                          session_.license().keys[key_index].key_id_length,
                          OEMCrypto_CipherMode_CTR));
  // Input and output are same buffer:
  vector<uint8_t> buffer = clear_buffer_;
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_Generic_Encrypt(
                session_.session_id(), buffer.data(), buffer.size(),
                iv_, OEMCrypto_AES_CBC_128_NO_PADDING, buffer.data()));
  ASSERT_EQ(expected_encrypted, buffer);
}

// Test Generic_Decrypt works correctly.
TEST_F(GenericCryptoTest, GenericKeyDecrypt) {
  EncryptAndLoadKeys();
  unsigned int key_index = 1;
  vector<uint8_t> encrypted;
  EncryptBuffer(key_index, clear_buffer_, &encrypted);
  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_SelectKey(session_.session_id(),
                          session_.license().keys[key_index].key_id,
                          session_.license().keys[key_index].key_id_length,
                          OEMCrypto_CipherMode_CTR));
  vector<uint8_t> resultant(encrypted.size());
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_Generic_Decrypt(
                session_.session_id(), encrypted.data(), encrypted.size(), iv_,
                OEMCrypto_AES_CBC_128_NO_PADDING, resultant.data()));
  ASSERT_EQ(clear_buffer_, resultant);
}

// Test that Generic_Decrypt works correctly when the input and output buffers
// are the same.
TEST_F(GenericCryptoTest, GenericKeyDecryptSameBufferAPI12) {
  EncryptAndLoadKeys();
  unsigned int key_index = 1;
  vector<uint8_t> encrypted;
  EncryptBuffer(key_index, clear_buffer_, &encrypted);
  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_SelectKey(session_.session_id(),
                          session_.license().keys[key_index].key_id,
                          session_.license().keys[key_index].key_id_length,
                          OEMCrypto_CipherMode_CTR));
  vector<uint8_t> buffer = encrypted;
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_Generic_Decrypt(
                session_.session_id(), buffer.data(), buffer.size(), iv_,
                OEMCrypto_AES_CBC_128_NO_PADDING, buffer.data()));
  ASSERT_EQ(clear_buffer_, buffer);
}

// Test that Generic_Decrypt fails to decrypt to an insecure buffer if the key
// requires a secure data path.
TEST_F(GenericCryptoTest, GenericSecureToClear) {
  session_.license().keys[1].control.control_bits |= htonl(
      wvoec::kControlObserveDataPath | wvoec::kControlDataPathSecure);
  EncryptAndLoadKeys();
  unsigned int key_index = 1;
  vector<uint8_t> encrypted;
  EncryptBuffer(key_index, clear_buffer_, &encrypted);
  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_SelectKey(session_.session_id(),
                          session_.license().keys[key_index].key_id,
                          session_.license().keys[key_index].key_id_length,
                          OEMCrypto_CipherMode_CTR));
  vector<uint8_t> resultant(encrypted.size());
  ASSERT_NE(OEMCrypto_SUCCESS,
            OEMCrypto_Generic_Decrypt(
                session_.session_id(), encrypted.data(), encrypted.size(), iv_,
                OEMCrypto_AES_CBC_128_NO_PADDING, resultant.data()));
  ASSERT_NE(clear_buffer_, resultant);
}

// Test that the Generic_Decrypt function fails when not allowed.
TEST_F(GenericCryptoTest, GenericKeyBadDecrypt) {
  EncryptAndLoadKeys();
  BadDecrypt(1, OEMCrypto_HMAC_SHA256, buffer_size_);
  // The buffer size must be a multiple of 16, so subtracting 10 is bad.
  BadDecrypt(1, OEMCrypto_AES_CBC_128_NO_PADDING, buffer_size_ - 10);
  BadDecrypt(0, OEMCrypto_AES_CBC_128_NO_PADDING, buffer_size_);
  BadDecrypt(2, OEMCrypto_AES_CBC_128_NO_PADDING, buffer_size_);
  BadDecrypt(3, OEMCrypto_AES_CBC_128_NO_PADDING, buffer_size_);
}

TEST_F(GenericCryptoTest, GenericKeySign) {
  EncryptAndLoadKeys();
  unsigned int key_index = 2;
  vector<uint8_t> expected_signature;
  SignBuffer(key_index, clear_buffer_, &expected_signature);

  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_SelectKey(session_.session_id(),
                          session_.license().keys[key_index].key_id,
                          session_.license().keys[key_index].key_id_length,
                          OEMCrypto_CipherMode_CTR));
  size_t gen_signature_length = 0;
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER,
            OEMCrypto_Generic_Sign(session_.session_id(), clear_buffer_.data(),
                                   clear_buffer_.size(), OEMCrypto_HMAC_SHA256,
                                   NULL, &gen_signature_length));
  ASSERT_EQ(static_cast<size_t>(SHA256_DIGEST_LENGTH), gen_signature_length);
  vector<uint8_t> signature(SHA256_DIGEST_LENGTH);
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_Generic_Sign(session_.session_id(), clear_buffer_.data(),
                                   clear_buffer_.size(), OEMCrypto_HMAC_SHA256,
                                   signature.data(), &gen_signature_length));
  ASSERT_EQ(expected_signature, signature);
}

// Test that the Generic_Sign function fails when not allowed.
TEST_F(GenericCryptoTest, GenericKeyBadSign) {
  EncryptAndLoadKeys();
  BadSign(0, OEMCrypto_HMAC_SHA256);             // Can't sign with encrypt key.
  BadSign(1, OEMCrypto_HMAC_SHA256);             // Can't sign with decrypt key.
  BadSign(3, OEMCrypto_HMAC_SHA256);             // Can't sign with verify key.
  BadSign(2, OEMCrypto_AES_CBC_128_NO_PADDING);  // Bad signing algorithm.
}

TEST_F(GenericCryptoTest, GenericKeyVerify) {
  EncryptAndLoadKeys();
  unsigned int key_index = 3;
  vector<uint8_t> signature;
  SignBuffer(key_index, clear_buffer_, &signature);

  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_SelectKey(session_.session_id(),
                          session_.license().keys[key_index].key_id,
                          session_.license().keys[key_index].key_id_length,
                          OEMCrypto_CipherMode_CTR));
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_Generic_Verify(
                session_.session_id(), clear_buffer_.data(),
                clear_buffer_.size(), OEMCrypto_HMAC_SHA256, signature.data(),
                signature.size()));
}

// Test that the Generic_Verify function fails when not allowed.
TEST_F(GenericCryptoTest, GenericKeyBadVerify) {
  EncryptAndLoadKeys();
  BadVerify(0, OEMCrypto_HMAC_SHA256, SHA256_DIGEST_LENGTH, false);
  BadVerify(1, OEMCrypto_HMAC_SHA256, SHA256_DIGEST_LENGTH, false);
  BadVerify(2, OEMCrypto_HMAC_SHA256, SHA256_DIGEST_LENGTH, false);
  BadVerify(3, OEMCrypto_HMAC_SHA256, SHA256_DIGEST_LENGTH, true);
  BadVerify(3, OEMCrypto_HMAC_SHA256, SHA256_DIGEST_LENGTH - 1, false);
  BadVerify(3, OEMCrypto_HMAC_SHA256, SHA256_DIGEST_LENGTH + 1, false);
  BadVerify(3, OEMCrypto_AES_CBC_128_NO_PADDING, SHA256_DIGEST_LENGTH, false);
}

// Test Generic_Encrypt with the maximum buffer size.
TEST_F(GenericCryptoTest, GenericKeyEncryptLargeBuffer) {
  buffer_size_ = GetResourceValue(kMaxGenericBuffer);
  EncryptAndLoadKeys();
  unsigned int key_index = 0;
  vector<uint8_t> expected_encrypted;
  EncryptBuffer(key_index, clear_buffer_, &expected_encrypted);
  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_SelectKey(session_.session_id(),
                          session_.license().keys[key_index].key_id,
                          session_.license().keys[key_index].key_id_length,
                          OEMCrypto_CipherMode_CTR));
  vector<uint8_t> encrypted(clear_buffer_.size());
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_Generic_Encrypt(
                session_.session_id(), clear_buffer_.data(),
                clear_buffer_.size(), iv_, OEMCrypto_AES_CBC_128_NO_PADDING,
                encrypted.data()));
  ASSERT_EQ(expected_encrypted, encrypted);
}

// Test Generic_Decrypt with the maximum buffer size.
TEST_F(GenericCryptoTest, GenericKeyDecryptLargeBuffer) {
  // Some applications are known to pass in a block that is almost 400k.
  buffer_size_ = GetResourceValue(kMaxGenericBuffer);
  EncryptAndLoadKeys();
  unsigned int key_index = 1;
  vector<uint8_t> encrypted;
  EncryptBuffer(key_index, clear_buffer_, &encrypted);
  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_SelectKey(session_.session_id(),
                          session_.license().keys[key_index].key_id,
                          session_.license().keys[key_index].key_id_length,
                          OEMCrypto_CipherMode_CTR));
  vector<uint8_t> resultant(encrypted.size());
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_Generic_Decrypt(
                session_.session_id(), encrypted.data(), encrypted.size(), iv_,
                OEMCrypto_AES_CBC_128_NO_PADDING, resultant.data()));
  ASSERT_EQ(clear_buffer_, resultant);
}

// Test Generic_Sign with the maximum buffer size.
TEST_F(GenericCryptoTest, GenericKeySignLargeBuffer) {
  buffer_size_ = GetResourceValue(kMaxGenericBuffer);
  EncryptAndLoadKeys();
  unsigned int key_index = 2;
  vector<uint8_t> expected_signature;
  SignBuffer(key_index, clear_buffer_, &expected_signature);

  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_SelectKey(session_.session_id(),
                          session_.license().keys[key_index].key_id,
                          session_.license().keys[key_index].key_id_length,
                          OEMCrypto_CipherMode_CTR));
  size_t gen_signature_length = 0;
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER,
            OEMCrypto_Generic_Sign(session_.session_id(), clear_buffer_.data(),
                                   clear_buffer_.size(), OEMCrypto_HMAC_SHA256,
                                   NULL, &gen_signature_length));
  ASSERT_EQ(static_cast<size_t>(SHA256_DIGEST_LENGTH), gen_signature_length);
  vector<uint8_t> signature(SHA256_DIGEST_LENGTH);
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_Generic_Sign(session_.session_id(), clear_buffer_.data(),
                                   clear_buffer_.size(), OEMCrypto_HMAC_SHA256,
                                   signature.data(), &gen_signature_length));
  ASSERT_EQ(expected_signature, signature);
}

// Test Generic_Verify with the maximum buffer size.
TEST_F(GenericCryptoTest, GenericKeyVerifyLargeBuffer) {
  buffer_size_ = GetResourceValue(kMaxGenericBuffer);
  EncryptAndLoadKeys();
  unsigned int key_index = 3;
  vector<uint8_t> signature;
  SignBuffer(key_index, clear_buffer_, &signature);

  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_SelectKey(session_.session_id(),
                          session_.license().keys[key_index].key_id,
                          session_.license().keys[key_index].key_id_length,
                          OEMCrypto_CipherMode_CTR));
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_Generic_Verify(
                session_.session_id(), clear_buffer_.data(),
                clear_buffer_.size(), OEMCrypto_HMAC_SHA256, signature.data(),
                signature.size()));
}

// Test Generic_Encrypt when the key duration has expired.
TEST_F(GenericCryptoTest, KeyDurationEncrypt) {
  EncryptAndLoadKeys();
  vector<uint8_t> expected_encrypted;
  EncryptBuffer(0, clear_buffer_, &expected_encrypted);
  unsigned int key_index = 0;
  vector<uint8_t> encrypted(clear_buffer_.size());

  sleep(kShortSleep);  //  Should still be valid key.

  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_SelectKey(session_.session_id(),
                          session_.license().keys[key_index].key_id,
                          session_.license().keys[key_index].key_id_length,
                          OEMCrypto_CipherMode_CTR));
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_Generic_Encrypt(
                session_.session_id(), clear_buffer_.data(),
                clear_buffer_.size(), iv_, OEMCrypto_AES_CBC_128_NO_PADDING,
                encrypted.data()));
  ASSERT_EQ(expected_encrypted, encrypted);

  sleep(kLongSleep);  // Should be expired key.

  encrypted.assign(clear_buffer_.size(), 0);
  OEMCryptoResult status = OEMCrypto_Generic_Encrypt(
      session_.session_id(), clear_buffer_.data(), clear_buffer_.size(), iv_,
      OEMCrypto_AES_CBC_128_NO_PADDING, encrypted.data());
  ASSERT_NO_FATAL_FAILURE(
      session_.TestDecryptResult(OEMCrypto_ERROR_KEY_EXPIRED, status));
  ASSERT_NE(encrypted, expected_encrypted);
  ASSERT_NO_FATAL_FAILURE(session_.TestSelectExpired(key_index));
}

// Test Generic_Decrypt when the key duration has expired.
TEST_F(GenericCryptoTest, KeyDurationDecrypt) {
  EncryptAndLoadKeys();

  unsigned int key_index = 1;
  vector<uint8_t> encrypted;
  EncryptBuffer(key_index, clear_buffer_, &encrypted);
  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_SelectKey(session_.session_id(),
                          session_.license().keys[key_index].key_id,
                          session_.license().keys[key_index].key_id_length,
                          OEMCrypto_CipherMode_CTR));

  sleep(kShortSleep);  //  Should still be valid key.

  vector<uint8_t> resultant(encrypted.size());
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_Generic_Decrypt(
                session_.session_id(), encrypted.data(), encrypted.size(), iv_,
                OEMCrypto_AES_CBC_128_NO_PADDING, resultant.data()));
  ASSERT_EQ(clear_buffer_, resultant);

  sleep(kLongSleep);  // Should be expired key.

  resultant.assign(encrypted.size(), 0);
  OEMCryptoResult status = OEMCrypto_Generic_Decrypt(
      session_.session_id(), encrypted.data(), encrypted.size(), iv_,
      OEMCrypto_AES_CBC_128_NO_PADDING, resultant.data());
  ASSERT_NO_FATAL_FAILURE(
      session_.TestDecryptResult(OEMCrypto_ERROR_KEY_EXPIRED, status));
  ASSERT_NE(clear_buffer_, resultant);
  ASSERT_NO_FATAL_FAILURE(session_.TestSelectExpired(key_index));
}

// Test Generic_Sign when the key duration has expired.
TEST_F(GenericCryptoTest, KeyDurationSign) {
  EncryptAndLoadKeys();

  unsigned int key_index = 2;
  vector<uint8_t> expected_signature;
  vector<uint8_t> signature(SHA256_DIGEST_LENGTH);
  size_t signature_length = signature.size();
  SignBuffer(key_index, clear_buffer_, &expected_signature);

  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_SelectKey(session_.session_id(),
                          session_.license().keys[key_index].key_id,
                          session_.license().keys[key_index].key_id_length,
                          OEMCrypto_CipherMode_CTR));

  sleep(kShortSleep);  //  Should still be valid key.

  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_Generic_Sign(session_.session_id(), clear_buffer_.data(),
                                   clear_buffer_.size(), OEMCrypto_HMAC_SHA256,
                                   signature.data(), &signature_length));
  ASSERT_EQ(expected_signature, signature);

  sleep(kLongSleep);  // Should be expired key.

  signature.assign(SHA256_DIGEST_LENGTH, 0);
  OEMCryptoResult status = OEMCrypto_Generic_Sign(
      session_.session_id(), clear_buffer_.data(), clear_buffer_.size(),
      OEMCrypto_HMAC_SHA256, signature.data(), &signature_length);
  ASSERT_NO_FATAL_FAILURE(
      session_.TestDecryptResult(OEMCrypto_ERROR_KEY_EXPIRED, status));
  ASSERT_NE(expected_signature, signature);
  ASSERT_NO_FATAL_FAILURE(session_.TestSelectExpired(key_index));
}

// Test Generic_Verify when the key duration has expired.
TEST_F(GenericCryptoTest, KeyDurationVerify) {
  EncryptAndLoadKeys();

  unsigned int key_index = 3;
  vector<uint8_t> signature;
  SignBuffer(key_index, clear_buffer_, &signature);

  ASSERT_EQ(
      OEMCrypto_SUCCESS,
      OEMCrypto_SelectKey(session_.session_id(),
                          session_.license().keys[key_index].key_id,
                          session_.license().keys[key_index].key_id_length,
                          OEMCrypto_CipherMode_CTR));

  sleep(kShortSleep);  //  Should still be valid key.

  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_Generic_Verify(
                session_.session_id(), clear_buffer_.data(),
                clear_buffer_.size(), OEMCrypto_HMAC_SHA256, signature.data(),
                signature.size()));

  sleep(kLongSleep);  // Should be expired key.

  OEMCryptoResult status = OEMCrypto_Generic_Verify(
      session_.session_id(), clear_buffer_.data(), clear_buffer_.size(),
      OEMCrypto_HMAC_SHA256, signature.data(), signature.size());
  ASSERT_NO_FATAL_FAILURE(
      session_.TestDecryptResult(OEMCrypto_ERROR_KEY_EXPIRED, status));
  ASSERT_NO_FATAL_FAILURE(session_.TestSelectExpired(key_index));
}

const unsigned int kLongKeyId = 2;

// Test that short key ids are allowed.
class GenericCryptoKeyIdLengthTest : public GenericCryptoTest {
 protected:
  void SetUp() override {
    GenericCryptoTest::SetUp();
    const uint32_t kNoNonce = 0;
    session_.set_num_keys(5);
    ASSERT_NO_FATAL_FAILURE(session_.FillSimpleMessage(
        kDuration, wvoec::kControlAllowDecrypt, kNoNonce));
    SetUniformKeyIdLength(16);  // Start with all key ids being 16 bytes.
    // But, we are testing that the key ids do not have to have the same length.
    session_.SetKeyId(0, "123456789012");  // 12 bytes (common key id length).
    session_.SetKeyId(1, "12345");         // short key id.
    session_.SetKeyId(2, "1234567890123456");  // 16 byte key id. (default)
    session_.SetKeyId(3, "12345678901234");    // 14 byte. (uncommon)
    session_.SetKeyId(4, "1");                 // very short key id.
    ASSERT_EQ(2u, kLongKeyId);
  }

  // Make all four keys have the same length.
  void SetUniformKeyIdLength(size_t key_id_length) {
    for (unsigned int i = 0; i < session_.num_keys(); i++) {
      string key_id;
      key_id.resize(key_id_length, i + 'a');
      session_.SetKeyId(i, key_id);
    }
  }

  void TestWithKey(unsigned int key_index) {
    ASSERT_LT(key_index, session_.num_keys());
    EncryptAndLoadKeys();
    vector<uint8_t> encrypted;
    // To make sure OEMCrypto is not expecting the key_id to be zero padded, we
    // will create a buffer that is padded with 'Z'.
    // Then, we use fill the buffer with the longer of the three keys. If
    // OEMCrypto is paying attention to the key id length, it should pick out
    // the correct key.
    vector<uint8_t> key_id_buffer(
        session_.license().keys[kLongKeyId].key_id_length + 5,
        'Z');  // Fill a bigger buffer with letter 'Z'.
    memcpy(key_id_buffer.data(), session_.license().keys[kLongKeyId].key_id,
           session_.license().keys[kLongKeyId].key_id_length);
    EncryptBuffer(key_index, clear_buffer_, &encrypted);
    ASSERT_EQ(
        OEMCrypto_SUCCESS,
        OEMCrypto_SelectKey(session_.session_id(), key_id_buffer.data(),
                            session_.license().keys[key_index].key_id_length,
                            OEMCrypto_CipherMode_CTR));
    vector<uint8_t> resultant(encrypted.size());
    ASSERT_EQ(OEMCrypto_SUCCESS,
              OEMCrypto_Generic_Decrypt(
                  session_.session_id(), encrypted.data(), encrypted.size(),
                  iv_, OEMCrypto_AES_CBC_128_NO_PADDING, resultant.data()));
    ASSERT_EQ(clear_buffer_, resultant);
  }
};

TEST_F(GenericCryptoKeyIdLengthTest, MediumKeyId) { TestWithKey(0); }

TEST_F(GenericCryptoKeyIdLengthTest, ShortKeyId) { TestWithKey(1); }

TEST_F(GenericCryptoKeyIdLengthTest, LongKeyId) { TestWithKey(2); }

TEST_F(GenericCryptoKeyIdLengthTest, FourteenByteKeyId) { TestWithKey(3); }

TEST_F(GenericCryptoKeyIdLengthTest, VeryShortKeyId) { TestWithKey(4); }

TEST_F(GenericCryptoKeyIdLengthTest, UniformShortKeyId) {
  SetUniformKeyIdLength(5);
  TestWithKey(2);
}

TEST_F(GenericCryptoKeyIdLengthTest, UniformLongKeyId) {
  SetUniformKeyIdLength(kTestKeyIdMaxLength);
  TestWithKey(2);
}

// Test usage table functionality.
class UsageTableTest : public GenericCryptoTest {
 public:
  void SetUp() override {
    GenericCryptoTest::SetUp();
    new_mac_keys_ = true;
  }

  virtual void ShutDown() {
    ASSERT_NO_FATAL_FAILURE(session_.close());
    ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_Terminate());
  }

  virtual void Restart() {
    OEMCrypto_SetSandbox(kTestSandbox, sizeof(kTestSandbox));
    ASSERT_EQ(OEMCrypto_SUCCESS, OEMCrypto_Initialize());
    EnsureTestKeys();
    ASSERT_NO_FATAL_FAILURE(session_.open());
  }

  void LoadOfflineLicense(Session& s, const std::string& pst) {
    ASSERT_NO_FATAL_FAILURE(s.open());
    ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
    ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
        0, wvoec::kControlNonceOrEntry, s.get_nonce(), pst));
    ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
    ASSERT_NO_FATAL_FAILURE(s.CreateNewUsageEntry());
    ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
    ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
    ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kUnused));
    ASSERT_NO_FATAL_FAILURE(s.close());
  }

  void PrintDotsWhileSleep(time_t total_seconds, time_t interval_seconds) {
    time_t dot_time = interval_seconds;
    time_t elapsed_time = 0;
    time_t start_time = time(NULL);
    do {
      sleep(1);
      elapsed_time = time(NULL) - start_time;
      if (elapsed_time >= dot_time) {
        cout << ".";
        cout.flush();
        dot_time += interval_seconds;
      }
    } while (elapsed_time < total_seconds);
    cout << endl;
  }

 protected:
  bool new_mac_keys_;
};

// Some usage tables we want to check a license either with or without a
// new pair of mac keys in the license response.  This affects signatures after
// the license is loaded.
// This test is parameterized by a boolean which determines if the license
// installs new mac keys in LoadKeys.
class UsageTableTestWithMAC : public UsageTableTest,
                              public WithParamInterface<bool> {
 public:
  void SetUp() override {
    UsageTableTest::SetUp();
    new_mac_keys_ = GetParam();
  }
};

// Test an online or streaming license with PST.  This license requires a valid
// nonce and can only be loaded once.
TEST_P(UsageTableTestWithMAC, OnlineLicense) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      0, wvoec::kControlNonceEnabled | wvoec::kControlNonceRequired,
      s.get_nonce(), pst));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  // test repeated report generation
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kUnused));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kUnused));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kUnused));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kUnused));
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kActive));
  // Flag the entry as inactive.
  ASSERT_NO_FATAL_FAILURE(s.DeactivateUsageEntry(pst));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  // It should report as inactive.
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kInactiveUsed));
  // Decrypt should fail.
  ASSERT_NO_FATAL_FAILURE(
      s.TestDecryptCTR(false, OEMCrypto_ERROR_UNKNOWN_FAILURE));
  // We could call DeactivateUsageEntry multiple times.  The state should not
  // change.
  ASSERT_NO_FATAL_FAILURE(s.DeactivateUsageEntry(pst));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  // It should report as inactive.
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kInactiveUsed));
}

// Test the usage report when the license is loaded but the keys are never used
// for decryption.
TEST_P(UsageTableTestWithMAC, OnlineLicenseUnused) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      0, wvoec::kControlNonceEnabled | wvoec::kControlNonceRequired,
      s.get_nonce(), pst));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  // No decrypt.  We do not use this license.
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kUnused));
  // Flag the entry as inactive.
  ASSERT_NO_FATAL_FAILURE(s.DeactivateUsageEntry(pst));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  // It should report as inactive.
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kInactiveUnused));
  // Decrypt should fail.
  ASSERT_NO_FATAL_FAILURE(
      s.TestDecryptCTR(false, OEMCrypto_ERROR_UNKNOWN_FAILURE));
  // We could call DeactivateUsageEntry multiple times.  The state should not
  // change.
  ASSERT_NO_FATAL_FAILURE(s.DeactivateUsageEntry(pst));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  // It should report as inactive.
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kInactiveUnused));
}

// Test that the usage table has been updated and saved before a report can be
// generated.
TEST_P(UsageTableTestWithMAC, ForbidReportWithNoUpdate) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      0, wvoec::kControlNonceEnabled | wvoec::kControlNonceRequired,
      s.get_nonce(), pst));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kUnused));
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
  // Cannot generate a report without first updating the file.
  ASSERT_NO_FATAL_FAILURE(
      s.GenerateReport(pst, OEMCrypto_ERROR_ENTRY_NEEDS_UPDATE));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  // Now it's OK.
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kActive));
  // Flag the entry as inactive.
  ASSERT_NO_FATAL_FAILURE(s.DeactivateUsageEntry(pst));
  // Cannot generate a report without first updating the file.
  ASSERT_NO_FATAL_FAILURE(
      s.GenerateReport(pst, OEMCrypto_ERROR_ENTRY_NEEDS_UPDATE));
  // Decrypt should fail.
  ASSERT_NO_FATAL_FAILURE(
      s.TestDecryptCTR(false, OEMCrypto_ERROR_UNKNOWN_FAILURE));
}

// Test an online license with a license renewal.
TEST_P(UsageTableTestWithMAC, OnlineLicenseWithRefresh) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      0, wvoec::kControlNonceEnabled | wvoec::kControlNonceRequired,
      s.get_nonce(), pst));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
  time_t loaded = time(NULL);
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
  s.GenerateNonce();
  // License renewal message is signed by client and verified by the server.
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature());
  size_t kAllKeys = 1;
  ASSERT_NO_FATAL_FAILURE(s.RefreshTestKeys(
      kAllKeys,
      wvoec::kControlNonceEnabled | wvoec::kControlNonceRequired,
      s.get_nonce(), OEMCrypto_SUCCESS));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(
      s.GenerateVerifyReport(pst, kActive,
                             loaded,  // when license loaded. (not refreshed)
                             loaded,  // first decrypt.
                             0));     // last decrypt is now.
}

// Verify that a streaming license cannot be reloaded.
TEST_F(UsageTableTest, RepeatOnlineLicense) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      0, wvoec::kControlNonceEnabled | wvoec::kControlNonceRequired,
      s.get_nonce(), pst));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.close());
  Session s2;
  ASSERT_NO_FATAL_FAILURE(s2.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s2));
  s2.LoadUsageEntry(s);  // Use the same entry.
  // Trying to reuse a PST is bad. We use session ID for s2, everything else
  // reused from s.
  ASSERT_NE(
      OEMCrypto_SUCCESS,
      OEMCrypto_LoadKeys(s2.session_id(), s.message_ptr(), s.message_size(),
                         s.signature().data(), s.signature().size(),
                         s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
                         s.num_keys(), s.key_array(), s.pst_substr(),
                         GetSubstring(), OEMCrypto_ContentLicense));
  ASSERT_NO_FATAL_FAILURE(s2.close());
}

// A license with non-zero replay control bits needs a valid pst.
TEST_F(UsageTableTest, OnlineEmptyPST) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      0, wvoec::kControlNonceEnabled | wvoec::kControlNonceRequired,
      s.get_nonce()));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.CreateNewUsageEntry());
  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), s.message_ptr(), s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), GetSubstring(), GetSubstring(),
      OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
  ASSERT_NO_FATAL_FAILURE(s.close());
}

// A license with non-zero replay control bits needs a valid pst.
TEST_F(UsageTableTest, OnlineMissingEntry) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      0, wvoec::kControlNonceEnabled | wvoec::kControlNonceRequired,
      s.get_nonce(), pst));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  // ENTRY NOT CREATED: ASSERT_NO_FATAL_FAILURE(s.CreateNewUsageEntry());
  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), s.message_ptr(), s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), s.pst_substr(), GetSubstring(),
      OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
  ASSERT_NO_FATAL_FAILURE(s.close());
}

// Test generic encrypt when the license uses a PST.
TEST_P(UsageTableTestWithMAC, GenericCryptoEncrypt) {
  std::string pst = "A PST";
  uint32_t nonce = session_.get_nonce();
  MakeFourKeys(0, wvoec::kControlNonceEnabled | wvoec::kControlNonceRequired,
               nonce, pst);
  ASSERT_NO_FATAL_FAILURE(session_.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(session_.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(session_.LoadTestKeys(pst, new_mac_keys_));
  OEMCryptoResult sts;
  unsigned int key_index = 0;
  vector<uint8_t> expected_encrypted;
  EncryptBuffer(key_index, clear_buffer_, &expected_encrypted);
  sts = OEMCrypto_SelectKey(session_.session_id(),
                            session_.license().keys[key_index].key_id,
                            session_.license().keys[key_index].key_id_length,
                            OEMCrypto_CipherMode_CTR);
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  vector<uint8_t> encrypted(clear_buffer_.size());
  sts = OEMCrypto_Generic_Encrypt(
      session_.session_id(), clear_buffer_.data(), clear_buffer_.size(), iv_,
      OEMCrypto_AES_CBC_128_NO_PADDING, encrypted.data());
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  EXPECT_EQ(expected_encrypted, encrypted);
  ASSERT_NO_FATAL_FAILURE(session_.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(session_.GenerateVerifyReport(pst, kActive));
  ASSERT_NO_FATAL_FAILURE(session_.DeactivateUsageEntry(pst));
  ASSERT_NO_FATAL_FAILURE(session_.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(session_.GenerateVerifyReport(pst, kInactiveUsed));
  encrypted.assign(clear_buffer_.size(), 0);
  sts = OEMCrypto_Generic_Encrypt(
      session_.session_id(), clear_buffer_.data(), clear_buffer_.size(), iv_,
      OEMCrypto_AES_CBC_128_NO_PADDING, encrypted.data());
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
  EXPECT_NE(encrypted, expected_encrypted);
}

// Test generic decrypt when the license uses a PST.
TEST_P(UsageTableTestWithMAC, GenericCryptoDecrypt) {
  std::string pst = "my_pst";
  uint32_t nonce = session_.get_nonce();
  MakeFourKeys(
      0, wvoec::kControlNonceEnabled | wvoec::kControlNonceRequired,
      nonce, pst);
  ASSERT_NO_FATAL_FAILURE(session_.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(session_.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(session_.LoadTestKeys(pst, new_mac_keys_));
  OEMCryptoResult sts;
  unsigned int key_index = 1;
  vector<uint8_t> encrypted;
  EncryptBuffer(key_index, clear_buffer_, &encrypted);
  sts = OEMCrypto_SelectKey(session_.session_id(),
                            session_.license().keys[key_index].key_id,
                            session_.license().keys[key_index].key_id_length,
                            OEMCrypto_CipherMode_CTR);
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  vector<uint8_t> resultant(encrypted.size());
  sts = OEMCrypto_Generic_Decrypt(
      session_.session_id(), encrypted.data(), encrypted.size(), iv_,
      OEMCrypto_AES_CBC_128_NO_PADDING,resultant.data());
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  EXPECT_EQ(clear_buffer_, resultant);
  ASSERT_NO_FATAL_FAILURE(session_.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(session_.GenerateVerifyReport(pst, kActive));
  ASSERT_NO_FATAL_FAILURE(session_.DeactivateUsageEntry(pst));
  ASSERT_NO_FATAL_FAILURE(session_.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(session_.GenerateVerifyReport(pst, kInactiveUsed));
  resultant.assign(encrypted.size(), 0);
  sts = OEMCrypto_Generic_Decrypt(
      session_.session_id(), encrypted.data(), encrypted.size(), iv_,
      OEMCrypto_AES_CBC_128_NO_PADDING, resultant.data());
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
  EXPECT_NE(clear_buffer_, resultant);
}

// Test generic sign when the license uses a PST.
TEST_P(UsageTableTestWithMAC, GenericCryptoSign) {
  std::string pst = "my_pst";
  uint32_t nonce = session_.get_nonce();
  MakeFourKeys(
      0, wvoec::kControlNonceEnabled | wvoec::kControlNonceRequired,
      nonce, pst);
  ASSERT_NO_FATAL_FAILURE(session_.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(session_.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(session_.LoadTestKeys(pst, new_mac_keys_));

  OEMCryptoResult sts;
  unsigned int key_index = 2;
  vector<uint8_t> expected_signature;
  ASSERT_NO_FATAL_FAILURE(
      SignBuffer(key_index, clear_buffer_, &expected_signature));

  sts = OEMCrypto_SelectKey(session_.session_id(),
                            session_.license().keys[key_index].key_id,
                            session_.license().keys[key_index].key_id_length,
                            OEMCrypto_CipherMode_CTR);
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  size_t gen_signature_length = 0;
  sts = OEMCrypto_Generic_Sign(session_.session_id(), clear_buffer_.data(),
                               clear_buffer_.size(), OEMCrypto_HMAC_SHA256,
                               NULL, &gen_signature_length);
  ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER, sts);
  ASSERT_EQ(static_cast<size_t>(SHA256_DIGEST_LENGTH), gen_signature_length);
  vector<uint8_t> signature(SHA256_DIGEST_LENGTH);
  sts = OEMCrypto_Generic_Sign(session_.session_id(), clear_buffer_.data(),
                               clear_buffer_.size(), OEMCrypto_HMAC_SHA256,
                               signature.data(), &gen_signature_length);
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  ASSERT_EQ(expected_signature, signature);

  ASSERT_NO_FATAL_FAILURE(session_.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(session_.GenerateVerifyReport(pst, kActive));
  ASSERT_NO_FATAL_FAILURE(session_.DeactivateUsageEntry(pst));
  ASSERT_NO_FATAL_FAILURE(session_.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(session_.GenerateVerifyReport(pst, kInactiveUsed));
  signature.assign(SHA256_DIGEST_LENGTH, 0);
  gen_signature_length = SHA256_DIGEST_LENGTH;
  sts = OEMCrypto_Generic_Sign(session_.session_id(), clear_buffer_.data(),
                               clear_buffer_.size(), OEMCrypto_HMAC_SHA256,
                               signature.data(), &gen_signature_length);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
  ASSERT_NE(signature, expected_signature);
}

// Test generic verify when the license uses a PST.
TEST_P(UsageTableTestWithMAC, GenericCryptoVerify) {
  std::string pst = "my_pst";
  uint32_t nonce = session_.get_nonce();
  MakeFourKeys(
      0, wvoec::kControlNonceEnabled | wvoec::kControlNonceRequired,
      nonce, pst);
  ASSERT_NO_FATAL_FAILURE(session_.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(session_.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(session_.LoadTestKeys(pst, new_mac_keys_));

  OEMCryptoResult sts;
  unsigned int key_index = 3;
  vector<uint8_t> signature;
  SignBuffer(key_index, clear_buffer_, &signature);

  sts = OEMCrypto_SelectKey(session_.session_id(),
                            session_.license().keys[key_index].key_id,
                            session_.license().keys[key_index].key_id_length,
                            OEMCrypto_CipherMode_CTR);
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  sts = OEMCrypto_Generic_Verify(session_.session_id(), clear_buffer_.data(),
                                 clear_buffer_.size(), OEMCrypto_HMAC_SHA256,
                                 signature.data(), signature.size());
  ASSERT_EQ(OEMCrypto_SUCCESS, sts);
  ASSERT_NO_FATAL_FAILURE(session_.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(session_.GenerateVerifyReport(pst, kActive));
  ASSERT_NO_FATAL_FAILURE(session_.DeactivateUsageEntry(pst));
  ASSERT_NO_FATAL_FAILURE(session_.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(session_.GenerateVerifyReport(pst, kInactiveUsed));
  sts = OEMCrypto_Generic_Verify(session_.session_id(), clear_buffer_.data(),
                                 clear_buffer_.size(), OEMCrypto_HMAC_SHA256,
                                 signature.data(), signature.size());
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
}

// Test that an offline license can be loaded.
TEST_P(UsageTableTestWithMAC, OfflineLicense) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(LoadOfflineLicense(s, pst));
}

// Test that an offline license can be loaded and that the license can be
// renewed.
TEST_P(UsageTableTestWithMAC, OfflineLicenseRefresh) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      0, wvoec::kControlNonceOrEntry, s.get_nonce(), pst));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
  time_t loaded = time(NULL);
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
  s.GenerateNonce();
  // License renewal message is signed by client and verified by the server.
  ASSERT_NO_FATAL_FAILURE(s.VerifyClientSignature());
  size_t kAllKeys = 1;
  ASSERT_NO_FATAL_FAILURE(s.RefreshTestKeys(
      kAllKeys, wvoec::kControlNonceOrEntry, 0, OEMCrypto_SUCCESS));
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(
      s.GenerateVerifyReport(pst, kActive,
                             loaded,  // license recieved.
                             loaded,  // First decrypt when loaded, not refresh.
                             0));     // last decrypt now.
}

// Test that an offline license can be reloaded in a new session.
TEST_P(UsageTableTestWithMAC, ReloadOfflineLicense) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(LoadOfflineLicense(s, pst));

  ASSERT_NO_FATAL_FAILURE(s.open());
  // We will reuse the encrypted and signed message, so we don't call
  // FillSimpleMessage again.
  ASSERT_NO_FATAL_FAILURE(s.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kUnused));
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kActive));
  ASSERT_NO_FATAL_FAILURE(s.close());
}

// Test that an offline license can be reloaded in a new session, and then
// refreshed.
TEST_P(UsageTableTestWithMAC, ReloadOfflineLicenseWithRefresh) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(LoadOfflineLicense(s, pst));
  time_t loaded = time(NULL);

  ASSERT_NO_FATAL_FAILURE(s.open());
  // We will reuse the encrypted and signed message, so we don't call
  // FillSimpleMessage again.
  ASSERT_NO_FATAL_FAILURE(s.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kUnused, loaded, 0, 0));
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
  time_t decrypt_time = time(NULL);
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(
      s.GenerateVerifyReport(pst, kActive,
                             loaded,  // license received.
                             decrypt_time,  // first decrypt
                             decrypt_time));  // last decrypt
  size_t kAllKeys = 1;
  ASSERT_NO_FATAL_FAILURE(s.RefreshTestKeys(
      kAllKeys, wvoec::kControlNonceOrEntry, 0, OEMCrypto_SUCCESS));
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kActive,
                                                 loaded,  // license loaded.
                                                 decrypt_time, // first decrypt
                                                 0));          // last decrypt
  ASSERT_NO_FATAL_FAILURE(s.close());
}

// Verify that we can still reload an offline license after OEMCrypto_Terminate
// and Initialize are called. This is as close to a reboot as we can do in a
// unit test.
TEST_P(UsageTableTestWithMAC, ReloadOfflineLicenseWithTerminate) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(LoadOfflineLicense(s, pst));
  ShutDown();  // This calls OEMCrypto_Terminate.
  Restart();   // This calls OEMCrypto_Initialize.
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_LoadUsageTableHeader(encrypted_usage_header_.data(),
                                           encrypted_usage_header_.size()));

  ASSERT_NO_FATAL_FAILURE(s.open());
  // We will reuse the encrypted and signed message, so we don't call
  // FillSimpleMessage again.
  ASSERT_NO_FATAL_FAILURE(s.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kUnused));
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kActive));
  ASSERT_NO_FATAL_FAILURE(s.close());
}

// If we attempt to load a second license with the same usage entry as the
// first, but it has different mac keys, then the attempt should fail.  This is
// how we verify that we are reloading the same license.
TEST_P(UsageTableTestWithMAC, BadReloadOfflineLicense) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(LoadOfflineLicense(s, pst));
  time_t loaded = time(NULL);

  // Offline license with new mac keys should fail.
  Session s2;
  ASSERT_NO_FATAL_FAILURE(s2.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s2));
  ASSERT_NO_FATAL_FAILURE(s2.FillSimpleMessage(
      0, wvoec::kControlNonceOrEntry, s2.get_nonce(), pst));
  ASSERT_NO_FATAL_FAILURE(s2.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s2.LoadUsageEntry(s));
  ASSERT_NE(
      OEMCrypto_SUCCESS,
      OEMCrypto_LoadKeys(s2.session_id(), s2.message_ptr(), s2.message_size(),
                         s2.signature().data(), s2.signature().size(),
                         s2.enc_mac_keys_iv_substr(), s2.enc_mac_keys_substr(),
                         s.num_keys(), s2.key_array(), s2.pst_substr(),
                         GetSubstring(), OEMCrypto_ContentLicense));
  ASSERT_NO_FATAL_FAILURE(s2.close());

  // Offline license with same mac keys should still be OK.
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kUnused,
                                                 loaded,  // license loaded.
                                                 0,
                                                 0));  // first and last decrypt
}

// An offline license should not load on the first call if the nonce is bad.
TEST_P(UsageTableTestWithMAC, OfflineBadNonce) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(
      s.FillSimpleMessage(0, wvoec::kControlNonceOrEntry, 42, pst));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), s.message_ptr(), s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), s.pst_substr(), GetSubstring(),
      OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
  ASSERT_NO_FATAL_FAILURE(s.close());
}

// An offline license needs a valid pst.
TEST_P(UsageTableTestWithMAC, OfflineEmptyPST) {
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(
      s.FillSimpleMessage(0, wvoec::kControlNonceOrEntry, s.get_nonce()));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  OEMCryptoResult sts = OEMCrypto_LoadKeys(
      s.session_id(), s.message_ptr(), s.message_size(), s.signature().data(),
      s.signature().size(), s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
      s.num_keys(), s.key_array(), GetSubstring(), GetSubstring(),
      OEMCrypto_ContentLicense);
  ASSERT_NE(OEMCrypto_SUCCESS, sts);
  ASSERT_NO_FATAL_FAILURE(s.close());
}

// If we try to reload a license with a different PST, the attempt should fail.
TEST_P(UsageTableTestWithMAC, ReloadOfflineWrongPST) {
  std::string pst = "my_pst1";
  Session s;
  ASSERT_NO_FATAL_FAILURE(LoadOfflineLicense(s, pst));

  std::string bad_pst = "my_pst2";
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.ReloadUsageEntry());
  memcpy(s.license().pst, bad_pst.c_str(), bad_pst.length());
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NE(OEMCrypto_SUCCESS,
            OEMCrypto_LoadKeys(
                s.session_id(), s.message_ptr(), s.message_size(),
                s.signature().data(), s.signature().size(), GetSubstring(),
                GetSubstring(), s.num_keys(), s.key_array(), s.pst_substr(),
                GetSubstring(), OEMCrypto_ContentLicense));
}

// Once a license has been deactivated, the keys can no longer be used for
// decryption.  However, we can still generate a usage report.
TEST_P(UsageTableTestWithMAC, DeactivateOfflineLicense) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(LoadOfflineLicense(s, pst));

  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(
      s.LoadTestKeys(pst, new_mac_keys_));      // Reload the license
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());  // Should be able to decrypt.
  ASSERT_NO_FATAL_FAILURE(s.DeactivateUsageEntry(pst));  // Then deactivate.
  // After deactivate, should not be able to decrypt.
  ASSERT_NO_FATAL_FAILURE(
      s.TestDecryptCTR(false, OEMCrypto_ERROR_UNKNOWN_FAILURE));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kInactiveUsed));
  ASSERT_NO_FATAL_FAILURE(s.close());

  Session s2;
  ASSERT_NO_FATAL_FAILURE(s2.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s2));
  ASSERT_NO_FATAL_FAILURE(s2.LoadUsageEntry(s));
  // Offile license can not be reused if it has been deactivated.
  EXPECT_NE(
      OEMCrypto_SUCCESS,
      OEMCrypto_LoadKeys(s2.session_id(), s.message_ptr(), s.message_size(),
                         s.signature().data(), s.signature().size(),
                         s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
                         s.num_keys(), s.key_array(), s.pst_substr(),
                         GetSubstring(), OEMCrypto_ContentLicense));
  s2.close();
  // But we can still generate a report.
  Session s3;
  ASSERT_NO_FATAL_FAILURE(s3.open());
  ASSERT_NO_FATAL_FAILURE(s3.LoadUsageEntry(s));
  ASSERT_NO_FATAL_FAILURE(s3.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s3.GenerateReport(pst, OEMCrypto_SUCCESS, &s));
  EXPECT_EQ(kInactiveUsed, s3.pst_report().status());
  // We could call DeactivateUsageEntry multiple times.  The state should not
  // change.
  ASSERT_NO_FATAL_FAILURE(s3.DeactivateUsageEntry(pst));
  ASSERT_NO_FATAL_FAILURE(s3.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s3.GenerateReport(pst, OEMCrypto_SUCCESS, &s));
  EXPECT_EQ(kInactiveUsed, s3.pst_report().status());
}

// The usage report should indicate that the keys were never used for
// decryption.
TEST_P(UsageTableTestWithMAC, DeactivateOfflineLicenseUnused) {
  std::string pst = "my_pst";
  Session s1;
  ASSERT_NO_FATAL_FAILURE(LoadOfflineLicense(s1, pst));

  ASSERT_NO_FATAL_FAILURE(s1.open());
  ASSERT_NO_FATAL_FAILURE(s1.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s1));
  ASSERT_NO_FATAL_FAILURE(
      s1.LoadTestKeys(pst, new_mac_keys_));      // Reload the license
  // No Decrypt.  This license is unused.
  ASSERT_NO_FATAL_FAILURE(s1.DeactivateUsageEntry(pst));  // Then deactivate.
  // After deactivate, should not be able to decrypt.
  ASSERT_NO_FATAL_FAILURE(
      s1.TestDecryptCTR(false, OEMCrypto_ERROR_UNKNOWN_FAILURE));
  ASSERT_NO_FATAL_FAILURE(s1.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s1.GenerateVerifyReport(pst, kInactiveUnused));
  ASSERT_NO_FATAL_FAILURE(s1.close());

  Session s2;
  ASSERT_NO_FATAL_FAILURE(s2.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s2));
  ASSERT_NO_FATAL_FAILURE(s2.LoadUsageEntry(s1));
  // Offline license can not be reused if it has been deactivated.
  EXPECT_NE(
      OEMCrypto_SUCCESS,
      OEMCrypto_LoadKeys(s2.session_id(), s1.message_ptr(), s1.message_size(),
                         s1.signature().data(), s1.signature().size(),
                         s1.enc_mac_keys_iv_substr(), s1.enc_mac_keys_substr(),
                         s1.num_keys(), s1.key_array(), s1.pst_substr(),
                         GetSubstring(), OEMCrypto_ContentLicense));
  s2.close();
  // But we can still generate a report.
  Session s3;
  ASSERT_NO_FATAL_FAILURE(s3.open());
  ASSERT_NO_FATAL_FAILURE(s3.LoadUsageEntry(s1));
  ASSERT_NO_FATAL_FAILURE(s3.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s3.GenerateReport(pst, OEMCrypto_SUCCESS, &s1));
  EXPECT_EQ(kInactiveUnused, s3.pst_report().status());
  // We could call DeactivateUsageEntry multiple times.  The state should not
  // change.
  ASSERT_NO_FATAL_FAILURE(s3.DeactivateUsageEntry(pst));
  ASSERT_NO_FATAL_FAILURE(s3.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s3.GenerateReport(pst, OEMCrypto_SUCCESS, &s1));
  EXPECT_EQ(kInactiveUnused, s3.pst_report().status());
}

// If the PST pointers are not contained in the message, then LoadKeys should
// reject the attempt.
TEST_P(UsageTableTestWithMAC, BadRange) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(session_.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      0, wvoec::kControlNonceOrEntry, s.get_nonce(), pst));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  std::string double_message = DuplicateMessage(s.encrypted_license());
  OEMCrypto_Substring wrong_pst = s.pst_substr();
  wrong_pst.offset += s.message_size();
  ASSERT_NE(
      OEMCrypto_SUCCESS,
      OEMCrypto_LoadKeys(
          s.session_id(),
          reinterpret_cast<const uint8_t*>(double_message.data()),
          s.message_size(), s.signature().data(), s.signature().size(),
          s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(), s.num_keys(),
          s.key_array(), wrong_pst, GetSubstring(), OEMCrypto_ContentLicense));
}

// Test update usage table fails when passed a null pointer.
TEST_F(UsageTableTest, UpdateFailsWithNullPtr) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      0, wvoec::kControlNonceEnabled | wvoec::kControlNonceRequired,
      s.get_nonce(), pst));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  size_t header_buffer_length = encrypted_usage_header_.size();
  size_t entry_buffer_length = s.encrypted_usage_entry().size();
  vector<uint8_t> buffer(entry_buffer_length);
  // Now try to pass in null pointers for the buffers.  This should fail.
  ASSERT_NE(OEMCrypto_SUCCESS,
            OEMCrypto_UpdateUsageEntry(
                s.session_id(), NULL, &header_buffer_length,
                buffer.data(), &entry_buffer_length));
  ASSERT_NE(OEMCrypto_SUCCESS,
            OEMCrypto_UpdateUsageEntry(
                s.session_id(), encrypted_usage_header_.data(),
                &header_buffer_length, NULL, &entry_buffer_length));
}

// Class used to test usage table defragmentation.
class UsageTableDefragTest : public UsageTableTest {
 protected:
  void LoadFirstLicense(Session* s, uint32_t index) {
    ASSERT_NO_FATAL_FAILURE(s->open());
    ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(s));
    std::string pst = "pst ";
    char c1 = 'A' + (index / 26);
    char c2 = 'A' + (index % 26);
    pst = pst + c1 + c2;
    ASSERT_NO_FATAL_FAILURE(s->FillSimpleMessage(
        0, wvoec::kControlNonceOrEntry, s->get_nonce(), pst));
    ASSERT_NO_FATAL_FAILURE(s->EncryptAndSign());
    ASSERT_NO_FATAL_FAILURE(s->CreateNewUsageEntry());
    ASSERT_EQ(s->usage_entry_number(), index);
    ASSERT_NO_FATAL_FAILURE(s->LoadTestKeys(pst, new_mac_keys_));
    ASSERT_NO_FATAL_FAILURE(s->TestDecryptCTR());
    ASSERT_NO_FATAL_FAILURE(s->UpdateUsageEntry(&encrypted_usage_header_));
    ASSERT_NO_FATAL_FAILURE(s->close());
  }

  void ReloadLicense(Session* s, time_t start) {
    ASSERT_NO_FATAL_FAILURE(s->open());
    ASSERT_NO_FATAL_FAILURE(s->ReloadUsageEntry());
    ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(s));
    ASSERT_NO_FATAL_FAILURE(s->LoadTestKeys(s->pst(), new_mac_keys_));
    ASSERT_NO_FATAL_FAILURE(s->UpdateUsageEntry(&encrypted_usage_header_));
    ASSERT_NO_FATAL_FAILURE(s->TestDecryptCTR());
    ASSERT_NO_FATAL_FAILURE(s->UpdateUsageEntry(&encrypted_usage_header_));
    ASSERT_NO_FATAL_FAILURE(s->GenerateVerifyReport(s->pst(), kActive,
                                                   start, start, 0));
    ASSERT_NO_FATAL_FAILURE(s->close());
  }

  void FailReload(Session* s, OEMCryptoResult expected_result) {
    ASSERT_NO_FATAL_FAILURE(s->open());
    ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(s));
    ASSERT_EQ(expected_result,
              OEMCrypto_LoadUsageEntry(s->session_id(), s->usage_entry_number(),
                                       s->encrypted_usage_entry().data(),
                                       s->encrypted_usage_entry().size()));

    ASSERT_NE(OEMCrypto_SUCCESS,
              OEMCrypto_LoadKeys(
                  s->session_id(), s->message_ptr(), s->message_size(),
                  s->signature().data(), s->signature().size(),
                  s->enc_mac_keys_iv_substr(), s->enc_mac_keys_substr(),
                  s->num_keys(), s->key_array(), s->pst_substr(),
                  GetSubstring(), OEMCrypto_ContentLicense));
    ASSERT_NO_FATAL_FAILURE(s->close());
  }

  void ShrinkHeader(uint32_t new_size,
                    OEMCryptoResult expected_result = OEMCrypto_SUCCESS) {
    size_t header_buffer_length = 0;
    OEMCryptoResult sts =
        OEMCrypto_ShrinkUsageTableHeader(new_size, NULL, &header_buffer_length);
    if (expected_result == OEMCrypto_SUCCESS) {
      ASSERT_EQ(OEMCrypto_ERROR_SHORT_BUFFER, sts);
    } else {
      ASSERT_NE(OEMCrypto_SUCCESS, sts);
      if (sts != OEMCrypto_ERROR_SHORT_BUFFER) return;
    }
    ASSERT_LT(0u, header_buffer_length);
    encrypted_usage_header_.resize(header_buffer_length);
    sts = OEMCrypto_ShrinkUsageTableHeader(
        new_size, encrypted_usage_header_.data(),
        &header_buffer_length);
    ASSERT_EQ(expected_result, sts);
  }
};

// Verify that usage table entries can be moved around in the table.
TEST_F(UsageTableDefragTest, MoveUsageEntries) {
  const size_t ENTRY_COUNT = 10;
  vector<Session> sessions(ENTRY_COUNT);
  vector<time_t> start(ENTRY_COUNT);
  for (size_t i = 0; i < ENTRY_COUNT; i++) {
    ASSERT_NO_FATAL_FAILURE(LoadFirstLicense(&sessions[i], i))
        << "On license " << i << " pst=" << sessions[i].pst();
    start[i] = time(NULL);
  }
  for (size_t i = 0; i < ENTRY_COUNT; i++) {
    ASSERT_NO_FATAL_FAILURE(ReloadLicense(&sessions[i], start[i]))
        << "On license " << i << " pst=" << sessions[i].pst();
  }
  // Move 4 to 1.
  ASSERT_NO_FATAL_FAILURE(
      sessions[4].MoveUsageEntry(1, &encrypted_usage_header_));
  // Shrink header to 3 entries 0, 1 was 4, 2.
  ASSERT_NO_FATAL_FAILURE(ShrinkHeader(3));
  ShutDown();
  Restart();
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_LoadUsageTableHeader(encrypted_usage_header_.data(),
                                           encrypted_usage_header_.size()));
  ASSERT_NO_FATAL_FAILURE(ReloadLicense(&sessions[0], start[0]));
  // Now has index 1.
  ASSERT_NO_FATAL_FAILURE(ReloadLicense(&sessions[4], start[4]));
  ASSERT_NO_FATAL_FAILURE(ReloadLicense(&sessions[2], start[2]));
  // When 4 was moved to 1, it increased the gen. number in the header.
  ASSERT_NO_FATAL_FAILURE(
      FailReload(&sessions[1], OEMCrypto_ERROR_GENERATION_SKEW));
  // Index 3 is beyond the end of the table.
  ASSERT_NO_FATAL_FAILURE(
      FailReload(&sessions[3], OEMCrypto_ERROR_UNKNOWN_FAILURE));
}

// A usage table entry cannot be moved into an entry where an open session is
// currently using the entry.
TEST_F(UsageTableDefragTest, MoveUsageEntriesToOpenSession) {
  Session s0;
  Session s1;
  LoadFirstLicense(&s0, 0);
  LoadFirstLicense(&s1, 1);
  s0.open();
  ASSERT_NO_FATAL_FAILURE(s0.ReloadUsageEntry());
  // s0 currently open on index 0.  Expect this to fail:
  ASSERT_NO_FATAL_FAILURE(s1.MoveUsageEntry(0, &encrypted_usage_header_,
                                            OEMCrypto_ERROR_ENTRY_IN_USE));
}

// The usage table cannot be shrunk if any session is using an entry that would
// be deleted.
TEST_F(UsageTableDefragTest, ShrinkOverOpenSessions) {
  Session s0;
  Session s1;
  LoadFirstLicense(&s0, 0);
  LoadFirstLicense(&s1, 1);
  s0.open();
  ASSERT_NO_FATAL_FAILURE(s0.ReloadUsageEntry());
  s1.open();
  ASSERT_NO_FATAL_FAILURE(s1.ReloadUsageEntry());
  // Since s0 and s1 are open, we can't shrink.
  ASSERT_NO_FATAL_FAILURE(ShrinkHeader(1, OEMCrypto_ERROR_ENTRY_IN_USE));
  s1.close();  // Can shrink after closing s1, even if s0 is open.
  ASSERT_NO_FATAL_FAILURE(ShrinkHeader(1, OEMCrypto_SUCCESS));
}

// Verify the usage table size can be increased.
TEST_F(UsageTableDefragTest, EnlargeHeader) {
  Session s0;
  Session s1;
  LoadFirstLicense(&s0, 0);
  LoadFirstLicense(&s1, 1);
  // Can only shrink the header -- not make it bigger.
  ASSERT_NO_FATAL_FAILURE(ShrinkHeader(4, OEMCrypto_ERROR_UNKNOWN_FAILURE));
}

// A new header can only be created while no entries are in use.
TEST_F(UsageTableDefragTest, CreateNewHeaderWhileUsingOldOne) {
  Session s0;
  Session s1;
  LoadFirstLicense(&s0, 0);
  LoadFirstLicense(&s1, 1);
  s0.open();
  ASSERT_NO_FATAL_FAILURE(s0.ReloadUsageEntry());
  const bool kExpectFailure = false;
  ASSERT_NO_FATAL_FAILURE(CreateUsageTableHeader(kExpectFailure));
}

// Verify that a usage table entry can only be loaded into the correct index of
// the table.
TEST_F(UsageTableDefragTest, ReloadUsageEntryWrongIndex) {
  Session s0;
  Session s1;
  LoadFirstLicense(&s0, 0);
  LoadFirstLicense(&s1, 1);
  s0.set_usage_entry_number(1);
  ASSERT_NO_FATAL_FAILURE(
      FailReload(&s0, OEMCrypto_ERROR_INVALID_SESSION));
}

// Verify that a usage table entry cannot be loaded if it has been altered.
TEST_F(UsageTableDefragTest, ReloadUsageEntryBadData) {
  Session s;
  LoadFirstLicense(&s, 0);
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  vector<uint8_t> data = s.encrypted_usage_entry();
  ASSERT_LT(0UL, data.size());
  data[0] ^= 42;
  // Error could be signature or verification error.
  ASSERT_NE(OEMCrypto_SUCCESS,
            OEMCrypto_LoadUsageEntry(s.session_id(), s.usage_entry_number(),
                                     data.data(), data.size()));
}

static std::string MakePST(size_t n) {
  std::stringstream stream;
  stream << "pst-" << n;
  return stream.str();
}

// This verifies we can actually create two hundered usage table entries.
TEST_F(UsageTableDefragTest, TwoHundredEntries) {
  // OEMCrypto is required to store at least 200 entries in the usage table
  // header, but it is allowed to store more. This test verifies that if we keep
  // adding entries, the error indicates a resource limit.  It then verifies
  // that all of the successful entries are still valid after we throw out the
  // last invalid entry.
  const size_t ENTRY_COUNT = 2000;
  vector<Session> sessions(ENTRY_COUNT);
  size_t successful_count = 0;
  for (size_t i = 0; i < ENTRY_COUNT; i++) {
    if (i % 50 == 0) LOGD("Creating license %zd", i);
    ASSERT_NO_FATAL_FAILURE(sessions[i].open());
    ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&sessions[i]));
    std::string pst = MakePST(i);
    ASSERT_NO_FATAL_FAILURE(sessions[i].FillSimpleMessage(
        0, wvoec::kControlNonceOrEntry, sessions[i].get_nonce(), pst));
    ASSERT_NO_FATAL_FAILURE(sessions[i].EncryptAndSign());
    // We attempt to create a new usage table entry for this session.
    OEMCryptoResult status;
    ASSERT_NO_FATAL_FAILURE(sessions[i].CreateNewUsageEntry(&status));
    if (status == OEMCrypto_SUCCESS) {
      ASSERT_EQ(sessions[i].usage_entry_number(), i);
      ASSERT_NO_FATAL_FAILURE(sessions[i].LoadTestKeys(pst, new_mac_keys_));
      ASSERT_NO_FATAL_FAILURE(
          sessions[i].UpdateUsageEntry(&encrypted_usage_header_));
      successful_count++;
    } else {
      // If we failed to create this many entries because of limited resources,
      // then the error returned should be insufficient resources.
      EXPECT_EQ(OEMCrypto_ERROR_INSUFFICIENT_RESOURCES, status)
          << "Failed to create license " << i << " with pst = " << pst;
      break;
    }
    ASSERT_NO_FATAL_FAILURE(sessions[i].close());
  }
  LOGD("successful_count = %d", successful_count);
  EXPECT_GE(successful_count, 200u);
  sleep(kShortSleep);
  // Now we will loop through each valid entry, and verify that we can still
  // reload the license and perform a decrypt.
  for (size_t i = 0; i < successful_count; i++) {
    if (i % 50 == 0) LOGD("Reloading license %zd", i);
    ASSERT_NO_FATAL_FAILURE(sessions[i].open());
    std::string pst = MakePST(i);
    // Reuse license message created above.
    ASSERT_NO_FATAL_FAILURE(sessions[i].ReloadUsageEntry());
    ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&sessions[i]));
    ASSERT_NO_FATAL_FAILURE(sessions[i].LoadTestKeys(pst, new_mac_keys_))
        << "Failed to reload license " << i << " with pst = " << pst;
    ASSERT_NO_FATAL_FAILURE(
        sessions[i].UpdateUsageEntry(&encrypted_usage_header_))
        << "Failed to update license " << i << " with pst = " << pst;
    ASSERT_NO_FATAL_FAILURE(sessions[i].TestDecryptCTR())
        << "Failed to use license " << i << " with pst = " << pst;
    ASSERT_NO_FATAL_FAILURE(
        sessions[i].UpdateUsageEntry(&encrypted_usage_header_))
        << "Failed to update license " << i << " with pst = " << pst;
    ASSERT_NO_FATAL_FAILURE(sessions[i].close());
  }
  // We also need to verify that a full table can be shrunk, and the remaining
  // licenses still work.
  size_t smaller_size = 10u;  // 10 is smaller than 200.
  ASSERT_NO_FATAL_FAILURE(ShrinkHeader(smaller_size));
  for (size_t i = 0; i < smaller_size; i++) {
    ASSERT_NO_FATAL_FAILURE(sessions[i].open());
    std::string pst = MakePST(i);
    // Reuse license message created above.
    ASSERT_NO_FATAL_FAILURE(sessions[i].ReloadUsageEntry());
    ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&sessions[i]));
    ASSERT_NO_FATAL_FAILURE(sessions[i].LoadTestKeys(pst, new_mac_keys_))
        << "Failed to reload license " << i << " with pst = " << pst;
    ASSERT_NO_FATAL_FAILURE(
        sessions[i].UpdateUsageEntry(&encrypted_usage_header_))
        << "Failed to update license " << i << " with pst = " << pst;
    ASSERT_NO_FATAL_FAILURE(sessions[i].TestDecryptCTR())
        << "Failed to use license " << i << " with pst = " << pst;
    ASSERT_NO_FATAL_FAILURE(
        sessions[i].UpdateUsageEntry(&encrypted_usage_header_))
        << "Failed to update license " << i << " with pst = " << pst;
    ASSERT_NO_FATAL_FAILURE(sessions[i].close());
  }
}

// This verifies that copying the old usage table to the new one works.
TEST_F(UsageTableTest, CopyOldEntries) {
  // First create three old entries.  We open sessions first to force creation
  // of the mac keys.

  Session s1;
  ASSERT_NO_FATAL_FAILURE(s1.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s1));
  ASSERT_NO_FATAL_FAILURE(s1.FillSimpleMessage(0, 0, 0, "pst number 1"));
  ASSERT_NO_FATAL_FAILURE(s1.EncryptAndSign());

  Test_PST_Report report1(s1.pst(), kUnused);
  report1.seconds_since_license_received = 30;
  report1.seconds_since_first_decrypt = 20;
  report1.seconds_since_last_decrypt = 10;
  ASSERT_NO_FATAL_FAILURE(s1.CreateOldEntry(report1));

  Session s2;
  ASSERT_NO_FATAL_FAILURE(s2.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s2));
  ASSERT_NO_FATAL_FAILURE(s2.FillSimpleMessage(0, 0, 0, "pst number 2"));
  ASSERT_NO_FATAL_FAILURE(s2.EncryptAndSign());

  Test_PST_Report report2(s2.pst(), kActive);
  report2.seconds_since_license_received = 60;
  report2.seconds_since_first_decrypt = 50;
  report2.seconds_since_last_decrypt = 40;
  ASSERT_NO_FATAL_FAILURE(s2.CreateOldEntry(report2));

  Session s3;
  ASSERT_NO_FATAL_FAILURE(s3.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s3));
  ASSERT_NO_FATAL_FAILURE(s3.FillSimpleMessage(0, 0, 0, "pst number 3"));
  ASSERT_NO_FATAL_FAILURE(s3.EncryptAndSign());

  Test_PST_Report report3(s3.pst(), kInactive);
  report3.seconds_since_license_received = 90;
  report3.seconds_since_first_decrypt = 80;
  report3.seconds_since_last_decrypt = 70;
  ASSERT_NO_FATAL_FAILURE(s3.CreateOldEntry(report3));

  // Now we copy and verify each one.  The order is changed to make
  // sure there are no order dependecies.
  ASSERT_NO_FATAL_FAILURE(
      s2.CopyAndVerifyOldEntry(report2, &encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(
      s1.CopyAndVerifyOldEntry(report1, &encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(
      s3.CopyAndVerifyOldEntry(report3, &encrypted_usage_header_));
}

// This verifies that the usage table header can be loaded if the generation
// number is off by one, but not off by two.
TEST_F(UsageTableTest, ReloadUsageTableWithSkew) {
  // This also tests a few other error conditions with usage table headers.
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(LoadOfflineLicense(s, pst));

  // Reload the license, and save the header.
  ASSERT_NO_FATAL_FAILURE(s.open());
  // We will reuse the encrypted and signed message, so we don't call
  // FillSimpleMessage again.
  ASSERT_NO_FATAL_FAILURE(s.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  vector<uint8_t> old_usage_header_2_ = encrypted_usage_header_;
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  vector<uint8_t> old_usage_header_1_ = encrypted_usage_header_;
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.close());

  ShutDown();
  Restart();
  // Null pointer generates error.
  ASSERT_NE(OEMCrypto_SUCCESS,
            OEMCrypto_LoadUsageTableHeader(NULL,
                                           old_usage_header_2_.size()));
  ASSERT_NO_FATAL_FAILURE(s.open());
  // Cannot load an entry with if header didn't load.
  ASSERT_EQ(
      OEMCrypto_ERROR_UNKNOWN_FAILURE,
      OEMCrypto_LoadUsageEntry(s.session_id(), s.usage_entry_number(),
                               s.encrypted_usage_entry().data(),
                               s.encrypted_usage_entry().size()));
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.close());

  // Modified header generates error.
  vector<uint8_t> bad_header = encrypted_usage_header_;
  bad_header[3] ^= 42;
  ASSERT_NE(OEMCrypto_SUCCESS,
            OEMCrypto_LoadUsageTableHeader(bad_header.data(),
                                           bad_header.size()));
  ASSERT_NO_FATAL_FAILURE(s.open());
  // Cannot load an entry with if header didn't load.
  ASSERT_EQ(
      OEMCrypto_ERROR_UNKNOWN_FAILURE,
      OEMCrypto_LoadUsageEntry(s.session_id(), s.usage_entry_number(),
                               s.encrypted_usage_entry().data(),
                               s.encrypted_usage_entry().size()));
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.close());

  // Old by 2 generation numbers is error.
  ASSERT_EQ(OEMCrypto_ERROR_GENERATION_SKEW,
            OEMCrypto_LoadUsageTableHeader(old_usage_header_2_.data(),
                                           old_usage_header_2_.size()));
  ASSERT_NO_FATAL_FAILURE(s.open());
  // Cannot load an entry with if header didn't load.
  ASSERT_NE(
      OEMCrypto_SUCCESS,
      OEMCrypto_LoadUsageEntry(s.session_id(), s.usage_entry_number(),
                               s.encrypted_usage_entry().data(),
                               s.encrypted_usage_entry().size()));
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.close());

  // Old by 1 generation numbers is just warning.
  ASSERT_EQ(OEMCrypto_WARNING_GENERATION_SKEW,
            OEMCrypto_LoadUsageTableHeader(old_usage_header_1_.data(),
                                           old_usage_header_1_.size()));
  // Everything else should still work.  Skew by 1 is just a warning.
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_EQ(
      OEMCrypto_WARNING_GENERATION_SKEW,
      OEMCrypto_LoadUsageEntry(s.session_id(), s.usage_entry_number(),
                               s.encrypted_usage_entry().data(),
                               s.encrypted_usage_entry().size()));
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.close());
}

// A usage report with the wrong pst should fail.
TEST_F(UsageTableTest, GenerateReportWrongPST) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      0, wvoec::kControlNonceOrEntry, s.get_nonce(), pst));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateReport("wrong_pst",
                                           OEMCrypto_ERROR_WRONG_PST));
}

// Test usage table timing.
TEST_F(UsageTableTest, TimingTest) {
  std::string pst1 = "my_pst_1";
  std::string pst2 = "my_pst_2";
  std::string pst3 = "my_pst_3";
  Session s1;
  Session s2;
  Session s3;
  ASSERT_NO_FATAL_FAILURE(LoadOfflineLicense(s1, pst1));
  time_t loaded1 = time(NULL);
  ASSERT_NO_FATAL_FAILURE(LoadOfflineLicense(s2, pst2));
  time_t loaded2 = time(NULL);
  ASSERT_NO_FATAL_FAILURE(LoadOfflineLicense(s3, pst3));
  time_t loaded3 = time(NULL);

  sleep(kLongSleep);
  ASSERT_NO_FATAL_FAILURE(s1.open());
  ASSERT_NO_FATAL_FAILURE(s1.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s1));
  ASSERT_NO_FATAL_FAILURE(s1.LoadTestKeys(pst1, new_mac_keys_));
  time_t first_decrypt1 = time(NULL);
  ASSERT_NO_FATAL_FAILURE(s1.TestDecryptCTR());

  ASSERT_NO_FATAL_FAILURE(s2.open());
  ASSERT_NO_FATAL_FAILURE(s2.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s2));
  ASSERT_NO_FATAL_FAILURE(s2.LoadTestKeys(pst2, new_mac_keys_));
  time_t first_decrypt2 = time(NULL);
  ASSERT_NO_FATAL_FAILURE(s2.TestDecryptCTR());

  sleep(kLongSleep);
  time_t second_decrypt = time(NULL);
  ASSERT_NO_FATAL_FAILURE(s1.TestDecryptCTR());
  ASSERT_NO_FATAL_FAILURE(s2.TestDecryptCTR());

  sleep(kLongSleep);
  ASSERT_NO_FATAL_FAILURE(s1.DeactivateUsageEntry(pst1));
  ASSERT_NO_FATAL_FAILURE(s1.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s2.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s1.close());
  ASSERT_NO_FATAL_FAILURE(s2.close());

  sleep(kLongSleep);
  // This is as close to reboot as we can simulate in code.
  ShutDown();
  sleep(kShortSleep);
  Restart();
  ASSERT_EQ(OEMCrypto_SUCCESS,
            OEMCrypto_LoadUsageTableHeader(encrypted_usage_header_.data(),
                                           encrypted_usage_header_.size()));

  // After a reboot, we should be able to reload keys, and generate reports.
  sleep(kLongSleep);
  time_t third_decrypt = time(NULL);
  ASSERT_NO_FATAL_FAILURE(s2.open());
  ASSERT_NO_FATAL_FAILURE(s2.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s2));
  ASSERT_NO_FATAL_FAILURE(s2.LoadTestKeys(pst2, new_mac_keys_));
  ASSERT_NO_FATAL_FAILURE(s2.TestDecryptCTR());
  ASSERT_NO_FATAL_FAILURE(s2.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s2.close());

  ASSERT_NO_FATAL_FAILURE(s1.open());
  ASSERT_NO_FATAL_FAILURE(s2.open());
  ASSERT_NO_FATAL_FAILURE(s3.open());
  ASSERT_NO_FATAL_FAILURE(s1.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(s2.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(s3.ReloadUsageEntry());
  sleep(kLongSleep);
  ASSERT_NO_FATAL_FAILURE(s1.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s1.GenerateVerifyReport(pst1, kInactiveUsed,
                                                  loaded1,
                                                  first_decrypt1,
                                                  second_decrypt));
  ASSERT_NO_FATAL_FAILURE(s2.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s2.GenerateVerifyReport(pst2, kActive,
                                                  loaded2,
                                                  first_decrypt2,
                                                  third_decrypt));
  ASSERT_NO_FATAL_FAILURE(s3.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s3.GenerateVerifyReport(pst3, kUnused, loaded3));
}

// Verify the times in the usage report.  For performance reasons, we allow the
// times in the usage report to be off by as much as kUsageTimeTolerance, which
// is 10 seconds. This acceptable error is called slop. This test needs to run
// long enough that the reported values are distinct, even after accounting for
// this slop.
TEST_F(UsageTableTest, VerifyUsageTimes) {
  std::string pst = "my_pst";
  Session s;
  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(s.FillSimpleMessage(
      0, wvoec::kControlNonceEnabled | wvoec::kControlNonceRequired,
      s.get_nonce(), pst));
  ASSERT_NO_FATAL_FAILURE(s.EncryptAndSign());
  ASSERT_NO_FATAL_FAILURE(s.CreateNewUsageEntry());
  ASSERT_NO_FATAL_FAILURE(s.LoadTestKeys(pst, new_mac_keys_));
  time_t load_time = time(NULL);

  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kUnused));

  const time_t kDotIntervalInSeconds = 5;
  const time_t kIdleInSeconds = 20;
  const time_t kPlaybackLoopInSeconds = 2 * 60;

  cout << "This test verifies the elapsed time reported in the usage table "
          "for a 2 minute simulated playback."
       << endl;
  cout << "The total time for this test is about "
       << kPlaybackLoopInSeconds + 2 * kIdleInSeconds << " seconds." << endl;
  cout << "Wait " << kIdleInSeconds
       << " seconds to verify usage table time before playback." << endl;

  PrintDotsWhileSleep(kIdleInSeconds, kDotIntervalInSeconds);

  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kUnused, load_time));
  cout << "Start simulated playback..." << endl;

  time_t dot_time = kDotIntervalInSeconds;
  time_t playback_time = 0;
  time_t start_time = time(NULL);
  do {
    ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());
    ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
    ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kActive,
                                                   load_time,
                                                   start_time,
                                                   0));  // last decrypt = now.
    playback_time = time(NULL) - start_time;
    ASSERT_LE(0, playback_time);
    if (playback_time >= dot_time) {
      cout << ".";
      cout.flush();
      dot_time += kDotIntervalInSeconds;
    }
  } while (playback_time < kPlaybackLoopInSeconds);
  cout << "\nSimulated playback time = " << playback_time << " seconds.\n";

  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kActive,
                                                 load_time,
                                                 start_time,
                                                 0));  // last decrypt = now.
  EXPECT_NEAR(s.pst_report().seconds_since_first_decrypt() -
                  s.pst_report().seconds_since_last_decrypt(),
              playback_time, kUsageTableTimeTolerance);

  cout << "Wait another " << kIdleInSeconds
       << " seconds "
          "to verify usage table time since playback ended."
       << endl;
  PrintDotsWhileSleep(kIdleInSeconds, kDotIntervalInSeconds);

  // At this point, this is what we expect:
  // idle         playback loop       idle
  // |-----|-------------------------|-----|
  //                                 |<--->| = seconds_since_last_decrypt
  //       |<----------------------------->| = seconds_since_first_decrypt
  // |<------------------------------------| = seconds_since_license_received
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kActive,
                                                 load_time,
                                                 start_time,
                                                 kIdleInSeconds));
  ASSERT_NO_FATAL_FAILURE(s.DeactivateUsageEntry(pst));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kInactiveUsed,
                                                 load_time,
                                                 start_time,
                                                 kIdleInSeconds));
  ASSERT_NO_FATAL_FAILURE(
      s.TestDecryptCTR(false, OEMCrypto_ERROR_UNKNOWN_FAILURE));
}

// NOTE: This test needs root access since clock_settime messes with the system
// time in order to verify that OEMCrypto protects against rollbacks in usage
// entries. Therefore, this test is filtered if not run as root.
// We don't test roll-forward protection or instances where the user rolls back
// the time to the last decrypt call since this requires hardware-secure clocks
// to guarantee.
TEST_F(UsageTableTest, TimeRollbackPrevention) {
  std::string pst = "my_pst";
  Session s1;
  cout << "This test temporarily rolls back the system time in order to verify "
       << "that the usage report accounts for the change. It then rolls "
       << "the time back forward to the absolute time." << endl;
  // We use clock_gettime(CLOCK_REALTIME, ...) over time(...) so we can easily
  // set the time using clock_settime.
  timespec current_time;
  ASSERT_EQ(0, clock_gettime(CLOCK_REALTIME, &current_time));
  time_t loaded = current_time.tv_sec;
  ASSERT_NO_FATAL_FAILURE(LoadOfflineLicense(s1, pst));

  ASSERT_NO_FATAL_FAILURE(s1.open());
  ASSERT_NO_FATAL_FAILURE(s1.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s1));
  ASSERT_NO_FATAL_FAILURE(s1.LoadTestKeys(pst, new_mac_keys_));
  ASSERT_EQ(0, clock_gettime(CLOCK_REALTIME, &current_time));
  time_t first_decrypt = current_time.tv_sec;
  // Monotonic clock can't be changed. We use this since system clock will be
  // unreliable.
  ASSERT_EQ(0, clock_gettime(CLOCK_MONOTONIC, &current_time));
  time_t first_decrypt_monotonic = current_time.tv_sec;
  ASSERT_NO_FATAL_FAILURE(s1.TestDecryptCTR());
  ASSERT_NO_FATAL_FAILURE(s1.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s1.close());

  // Imitate playback.
  sleep(kLongDuration * 2);

  ASSERT_NO_FATAL_FAILURE(s1.open());
  ASSERT_NO_FATAL_FAILURE(s1.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s1));
  ASSERT_NO_FATAL_FAILURE(s1.LoadTestKeys(pst, new_mac_keys_));
  ASSERT_NO_FATAL_FAILURE(s1.TestDecryptCTR());
  ASSERT_NO_FATAL_FAILURE(s1.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s1.close());

  ASSERT_EQ(0, clock_gettime(CLOCK_REALTIME, &current_time));
  // Rollback the wall clock time.
  cout << "Rolling the system time back..." << endl;
  timeval current_time_of_day = {};
  current_time_of_day.tv_sec = current_time.tv_sec - kLongDuration * 10;
  ASSERT_EQ(0, settimeofday(&current_time_of_day, NULL));

  // Try to playback again.
  ASSERT_NO_FATAL_FAILURE(s1.open());
  ASSERT_NO_FATAL_FAILURE(s1.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s1));
  ASSERT_NO_FATAL_FAILURE(s1.LoadTestKeys(pst, new_mac_keys_));
  ASSERT_EQ(0, clock_gettime(CLOCK_MONOTONIC, &current_time));
  time_t third_decrypt = current_time.tv_sec;
  ASSERT_NO_FATAL_FAILURE(s1.TestDecryptCTR());
  ASSERT_NO_FATAL_FAILURE(s1.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s1.GenerateReport(pst));
  Test_PST_Report expected(pst, kActive);

  // Restore wall clock to its original position to verify that OEMCrypto does
  // not report negative times.
  ASSERT_EQ(0, clock_gettime(CLOCK_MONOTONIC, &current_time));
  current_time_of_day.tv_sec =
      first_decrypt + current_time.tv_sec - first_decrypt_monotonic;
  cout << "Rolling the system time forward to the absolute time..." << endl;
  ASSERT_EQ(0, settimeofday(&current_time_of_day, NULL));
  // Need to update time created since the verification checks the time of PST
  // report creation.
  expected.time_created = current_time_of_day.tv_sec;

  ASSERT_NO_FATAL_FAILURE(
      s1.VerifyReport(expected, loaded, first_decrypt,
                      first_decrypt + third_decrypt - first_decrypt_monotonic));
  ASSERT_NO_FATAL_FAILURE(s1.close());
}

// Verify that a large PST can be used with usage table entries.
TEST_F(UsageTableTest, PSTLargeBuffer) {
  std::string pst(kMaxPSTLength, 'a');  // A large PST.
  Session s;
  ASSERT_NO_FATAL_FAILURE(LoadOfflineLicense(s, pst));

  ASSERT_NO_FATAL_FAILURE(s.open());
  ASSERT_NO_FATAL_FAILURE(s.ReloadUsageEntry());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s));
  ASSERT_NO_FATAL_FAILURE(
      s.LoadTestKeys(pst, new_mac_keys_));      // Reload the license
  ASSERT_NO_FATAL_FAILURE(s.TestDecryptCTR());  // Should be able to decrypt.
  ASSERT_NO_FATAL_FAILURE(s.DeactivateUsageEntry(pst));  // Then deactivate.
  // After deactivate, should not be able to decrypt.
  ASSERT_NO_FATAL_FAILURE(
      s.TestDecryptCTR(false, OEMCrypto_ERROR_UNKNOWN_FAILURE));
  ASSERT_NO_FATAL_FAILURE(s.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s.GenerateVerifyReport(pst, kInactiveUsed));
  ASSERT_NO_FATAL_FAILURE(s.close());

  Session s2;
  ASSERT_NO_FATAL_FAILURE(s2.open());
  ASSERT_NO_FATAL_FAILURE(InstallTestSessionKeys(&s2));
  // Offile license can not be reused if it has been deactivated.
  ASSERT_NO_FATAL_FAILURE(s2.LoadUsageEntry(s));
  EXPECT_NE(
      OEMCrypto_SUCCESS,
      OEMCrypto_LoadKeys(s2.session_id(), s.message_ptr(), s.message_size(),
                         s.signature().data(), s.signature().size(),
                         s.enc_mac_keys_iv_substr(), s.enc_mac_keys_substr(),
                         s.num_keys(), s.key_array(), s.pst_substr(),
                         GetSubstring(), OEMCrypto_ContentLicense));
  s2.close();
  // But we can still generate a report.
  Session s3;
  ASSERT_NO_FATAL_FAILURE(s3.open());
  ASSERT_NO_FATAL_FAILURE(s3.LoadUsageEntry(s));
  ASSERT_NO_FATAL_FAILURE(s3.UpdateUsageEntry(&encrypted_usage_header_));
  ASSERT_NO_FATAL_FAILURE(s3.GenerateReport(pst, OEMCrypto_SUCCESS, &s));
  EXPECT_EQ(kInactiveUsed, s3.pst_report().status());
}

INSTANTIATE_TEST_CASE_P(TestUsageTables, UsageTableTestWithMAC,
                        Values(true, false));  // With and without new_mac_keys.
}  // namespace wvoec
