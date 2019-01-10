// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////

#include "tink/subtle/aes_siv_boringssl.h"

#include <string>
#include <vector>

#include "tink/subtle/wycheproof_util.h"
#include "tink/util/status.h"
#include "tink/util/statusor.h"
#include "tink/util/test_util.h"
#include "gtest/gtest.h"

namespace crypto {
namespace tink {
namespace subtle {
namespace {

TEST(AesSivBoringSslTest, testEncryptDecrypt) {
  std::string key(test::HexDecodeOrDie(
      "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
      "00112233445566778899aabbccddeefff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"));
  auto res = AesSivBoringSsl::New(key);
  EXPECT_TRUE(res.ok()) << res.status();
  auto cipher = std::move(res.ValueOrDie());
  std::string aad = "Additional data";
  std::string message = "Some data to encrypt.";
  auto ct = cipher->EncryptDeterministically(message, aad);
  EXPECT_TRUE(ct.ok()) << ct.status();
  auto pt = cipher->DecryptDeterministically(ct.ValueOrDie(), aad);
  EXPECT_TRUE(pt.ok()) << pt.status();
  EXPECT_EQ(pt.ValueOrDie(), message);
}

TEST(AesSivBoringSslTest, testNullPtrStringView) {
  std::string key(test::HexDecodeOrDie(
      "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
      "00112233445566778899aabbccddeefff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"));
  auto res = AesSivBoringSsl::New(key);
  EXPECT_TRUE(res.ok()) << res.status();
  // Checks that a string_view initialized with a null ptr works.
  auto cipher = std::move(res.ValueOrDie());
  absl::string_view null(nullptr);
  auto ct = cipher->EncryptDeterministically(null, null);
  EXPECT_TRUE(ct.ok()) << ct.status();
  auto pt = cipher->DecryptDeterministically(ct.ValueOrDie(), null);
  EXPECT_TRUE(pt.ok()) << pt.status();
  EXPECT_EQ("", pt.ValueOrDie());
  // Decryption with ct == null should return an appropriate status.
  pt = cipher->DecryptDeterministically(null, "");
  EXPECT_FALSE(pt.ok());
  // Additional data with an empty std::string view is the same an empty std::string.
  std::string message("123456789abcdefghijklmnop");
  ct = cipher->EncryptDeterministically(message, null);
  pt = cipher->DecryptDeterministically(ct.ValueOrDie(), "");
  EXPECT_TRUE(pt.ok()) << pt.status();
  EXPECT_EQ(message, pt.ValueOrDie());
  ct = cipher->EncryptDeterministically(message, "");
  pt = cipher->DecryptDeterministically(ct.ValueOrDie(), null);
  EXPECT_TRUE(pt.ok()) << pt.status();
  EXPECT_EQ(message, pt.ValueOrDie());
}

// Only 64 byte key sizes are supported.
TEST(AesSivBoringSslTest, testEncryptDecryptKeySizes) {
  std::string keymaterial(test::HexDecodeOrDie(
      "198371900187498172316311acf81d238ff7619873a61983d619c87b63a1987f"
      "987131819803719b847126381cd763871638aa71638176328761287361231321"
      "812731321de508761437195ff231765aa4913219873ac6918639816312130011"
      "abc900bba11400187984719827431246bbab1231eb4145215ff7141436616beb"
      "9817298148712fed3aab61000ff123313e"));
  for (int keysize = 0; keysize <= keymaterial.size(); ++keysize){
    std::string key = std::string(keymaterial, 0, keysize);
    auto cipher = AesSivBoringSsl::New(key);
    if (keysize == 64) {
      EXPECT_TRUE(cipher.ok());
    } else {
      EXPECT_FALSE(cipher.ok()) << "Accepted invalid key size:" << keysize;
    }
  }
}

// Checks a range of message sizes.
TEST(AesSivBoringSslTest, testEncryptDecryptMessageSize) {
  std::string key(test::HexDecodeOrDie(
      "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
      "00112233445566778899aabbccddeefff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"));
  auto res = AesSivBoringSsl::New(key);
  EXPECT_TRUE(res.ok()) << res.status();
  auto cipher = std::move(res.ValueOrDie());
  std::string aad = "Additional data";
  for (int i = 0; i < 1024; ++i) {
    std::string message = std::string(i, 'a');
    auto ct = cipher->EncryptDeterministically(message, aad);
    EXPECT_TRUE(ct.ok()) << ct.status();
    auto pt = cipher->DecryptDeterministically(ct.ValueOrDie(), aad);
    EXPECT_TRUE(pt.ok()) << pt.status();
    EXPECT_EQ(pt.ValueOrDie(), message);
  }
  for (int i = 1024; i < 100000; i+= 5000) {
    std::string message = std::string(i, 'a');
    auto ct = cipher->EncryptDeterministically(message, aad);
    EXPECT_TRUE(ct.ok()) << ct.status();
    auto pt = cipher->DecryptDeterministically(ct.ValueOrDie(), aad);
    EXPECT_TRUE(pt.ok()) << pt.status();
    EXPECT_EQ(pt.ValueOrDie(), message);
  }
}

// Checks a range of aad sizes.
TEST(AesSivBoringSslTest, testEncryptDecryptAadSize) {
  std::string key(test::HexDecodeOrDie(
      "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
      "00112233445566778899aabbccddeefff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"));
  auto res = AesSivBoringSsl::New(key);
  EXPECT_TRUE(res.ok()) << res.status();
  auto cipher = std::move(res.ValueOrDie());
  std::string message = "Some plaintext";
  for (int i = 0; i < 1028; ++i) {
    std::string aad = std::string(i, 'a');
    auto ct = cipher->EncryptDeterministically(message, aad);
    EXPECT_TRUE(ct.ok()) << ct.status();
    auto pt = cipher->DecryptDeterministically(ct.ValueOrDie(), aad);
    EXPECT_TRUE(pt.ok()) << pt.status();
    EXPECT_EQ(pt.ValueOrDie(), message);
  }
}

TEST(AesSivBoringSslTest, testDecryptModification) {
  std::string key(test::HexDecodeOrDie(
      "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
      "00112233445566778899aabbccddeefff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"));
  auto res = AesSivBoringSsl::New(key);
  EXPECT_TRUE(res.ok()) << res.status();
  auto cipher = std::move(res.ValueOrDie());
  std::string aad = "Additional data";
  for (int i = 0; i < 50; ++i) {
    std::string message = std::string(i, 'a');
    auto ct = cipher->EncryptDeterministically(message, aad);
    EXPECT_TRUE(ct.ok()) << ct.status();
    std::string ciphertext = ct.ValueOrDie();
    for (size_t b = 0; b < ciphertext.size(); ++b) {
      for (int bit = 0; bit < 8; ++bit) {
        std::string modified = ciphertext;
        modified[b] ^= (1 << bit);
        auto pt = cipher->DecryptDeterministically(modified, aad);
        EXPECT_FALSE(pt.ok())
            << "Modified ciphertext decrypted."
            << " byte:" << b
            << " bit:" << bit;
      }
    }
  }
}

// Test with test vectors from project Wycheproof.
void WycheproofTest(const rapidjson::Document &root) {
  for (const rapidjson::Value& test_group : root["testGroups"].GetArray()) {
    const size_t key_size = test_group["keySize"].GetInt();
    if (!AesSivBoringSsl::IsValidKeySizeInBytes(key_size / 8)) {
      // Currently the key size is restricted to two 256-bit AES keys.
      continue;
    }
    for (const rapidjson::Value& test : test_group["tests"].GetArray()) {
      std::string comment = test["comment"].GetString();
      std::string key = WycheproofUtil::GetBytes(test["key"]);
      std::string msg = WycheproofUtil::GetBytes(test["msg"]);
      std::string ct = WycheproofUtil::GetBytes(test["ct"]);
      std::string aad = WycheproofUtil::GetBytes(test["aad"]);
      int id = test["tcId"].GetInt();
      std::string result = test["result"].GetString();
      auto cipher = std::move(AesSivBoringSsl::New(key).ValueOrDie());

      // Test encryption.
      // Encryption should always succeed since msg and aad are valid inputs.
      std::string encrypted =
          cipher->EncryptDeterministically(msg, aad).ValueOrDie();
      std::string encrypted_hex = test::HexEncode(encrypted);
      std::string ct_hex = test::HexEncode(ct);
      if (result == "valid" || result == "acceptable") {
        EXPECT_EQ(ct_hex, encrypted_hex)
            << "incorrect encryption: " << id << " " << comment;
      } else {
        EXPECT_NE(ct_hex, encrypted_hex)
            << "invalid encryption: " << id << " " << comment;
      }

      // Test decryption
      auto decrypted = cipher->DecryptDeterministically(ct, aad);
      if (decrypted.ok()) {
        if (result == "invalid") {
          ADD_FAILURE() << "decrypted invalid ciphertext:" << id;
        } else {
          EXPECT_EQ(test::HexEncode(msg),
                    test::HexEncode(decrypted.ValueOrDie()))
              << "incorrect decryption: " << id << " " << comment;
        }
      } else {
        EXPECT_NE(result, "valid")
            << "failed to decrypt: " << id << " " << comment;
      }
    }
  }
}

TEST(AesSivBoringSslTest, TestVectors) {
  std::unique_ptr<rapidjson::Document> root =
      WycheproofUtil::ReadTestVectors("aes_siv_cmac_test.json");
  WycheproofTest(*root);
}

}  // namespace
}  // namespace subtle
}  // namespace tink
}  // namespace crypto