#include <gtest/gtest.h>

#include <string>

#include "services/PasswordHasher.h"

using namespace csms;

class PasswordHasherTest : public ::testing::Test {
protected:
    PasswordHasher hasher = PasswordHasher::lowCostForTests();
};

TEST_F(PasswordHasherTest, HashIsNotThePlaintextAndDoesNotContainIt) {
    const std::string password = "CorrectHorse9!";
    const std::string hash = hasher.hash(password);
    EXPECT_NE(hash, password);
    EXPECT_EQ(hash.find(password), std::string::npos);
}

TEST_F(PasswordHasherTest, UsesTheEstablishedArgon2idFormat) {
    EXPECT_EQ(hasher.hash("whatever123").rfind("$argon2id$", 0), 0u);
}

TEST_F(PasswordHasherTest, CorrectPasswordVerifies) {
    const std::string hash = hasher.hash("CorrectHorse9!");
    EXPECT_TRUE(hasher.verify(hash, "CorrectHorse9!"));
}

TEST_F(PasswordHasherTest, WrongPasswordsDoNotVerify) {
    const std::string hash = hasher.hash("CorrectHorse9!");
    EXPECT_FALSE(hasher.verify(hash, "correcthorse9!"));    // case matters
    EXPECT_FALSE(hasher.verify(hash, "CorrectHorse9! "));   // whitespace matters
    EXPECT_FALSE(hasher.verify(hash, ""));
}

TEST_F(PasswordHasherTest, SamePasswordGivesDifferentHashesBecauseOfRandomSalt) {
    const std::string a = hasher.hash("CorrectHorse9!");
    const std::string b = hasher.hash("CorrectHorse9!");
    EXPECT_NE(a, b);
    EXPECT_TRUE(hasher.verify(a, "CorrectHorse9!"));
    EXPECT_TRUE(hasher.verify(b, "CorrectHorse9!"));
}

TEST_F(PasswordHasherTest, MalformedHashesVerifyAsFalseInsteadOfThrowing) {
    EXPECT_FALSE(hasher.verify("", "x"));
    EXPECT_FALSE(hasher.verify("not-a-hash", "x"));
    EXPECT_FALSE(hasher.verify(std::string(500, 'a'), "x"));
}

TEST_F(PasswordHasherTest, HashesCreatedAtOneStrengthVerifyWithAnother) {
    // The cost parameters live inside the hash string, so verification does not
    // depend on how the hasher object was configured.
    const std::string hash = PasswordHasher::lowCostForTests().hash("CorrectHorse9!");
    EXPECT_TRUE(PasswordHasher::interactive().verify(hash, "CorrectHorse9!"));
}

TEST_F(PasswordHasherTest, HandlesUnicodeAndLongPasswords) {
    const std::string unicode = "pässwörd-密码-🔐";
    EXPECT_TRUE(hasher.verify(hasher.hash(unicode), unicode));
    const std::string longPw(128, 'x');
    EXPECT_TRUE(hasher.verify(hasher.hash(longPw), longPw));
}
